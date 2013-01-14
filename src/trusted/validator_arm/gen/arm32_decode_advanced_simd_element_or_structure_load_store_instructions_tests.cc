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
// inst(21)=0 & inst(23)=0 & inst(11:8)=0010
//    = {baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=0 & A(23)=0 & B(11:8)=0010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultipleTesterCase0
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultipleTesterCase0(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultipleTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~0010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000200) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreMultipleTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreMultipleTester::
               ApplySanityChecks(inst, decoder));

  // safety: type(11:8)=0111 &&
  //       align(1)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000700) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002)));

  // safety: type(11:8)=1010 &&
  //       align(5:4)=11 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000A00) &&
       ((inst.Bits() & 0x00000030)  ==
          0x00000030)));

  // safety: type(11:8)=0110 &&
  //       align(1)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000600) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002)));

  // safety: not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR
  EXPECT_TRUE(((inst.Bits() & 0x00000F00)  ==
          0x00000700) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000A00) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000600) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000200));

  // safety: n  ==
  //          Pc ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000700
       ? 1
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000A00
       ? 2
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000600
       ? 3
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000200
       ? 4
       : 0))))) > (32)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=0011
//    = {baseline: 'VectorLoadStoreMultiple2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=1000
//            else 2 + 1
//            if inst(11:8)=1000 ||
//            inst(11:8)=1001
//            else 2 => UNPREDICTABLE,
//         inst(11:8)=1000 ||
//            inst(11:8)=1001 &&
//            inst(5:4)=11 => UNDEFINED,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=1000 ||
//            inst(11:8)=1001 ||
//            inst(11:8)=0011 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=0 & A(23)=0 & B(11:8)=0011
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultipleTesterCase1
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultipleTesterCase1(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultipleTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~0011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000300) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreMultipleTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreMultipleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(7:6)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000000C0)  !=
          0x000000C0);

  // safety: type in bitset {'1000', '1001'} &&
  //       align(5:4)=11 => UNDEFINED
  EXPECT_TRUE(!((((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900)) &&
       ((inst.Bits() & 0x00000030)  ==
          0x00000030)));

  // safety: not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR
  EXPECT_TRUE(((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000300));

  // safety: n  ==
  //          Pc ||
  //       d2 + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000800
       ? 1
       : 2) + (((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900)
       ? 1
       : 2)) > (32)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=1010
//    = {baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=0 & A(23)=0 & B(11:8)=1010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultipleTesterCase2
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultipleTesterCase2(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultipleTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~1010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000A00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreMultipleTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreMultipleTester::
               ApplySanityChecks(inst, decoder));

  // safety: type(11:8)=0111 &&
  //       align(1)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000700) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002)));

  // safety: type(11:8)=1010 &&
  //       align(5:4)=11 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000A00) &&
       ((inst.Bits() & 0x00000030)  ==
          0x00000030)));

  // safety: type(11:8)=0110 &&
  //       align(1)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000600) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002)));

  // safety: not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR
  EXPECT_TRUE(((inst.Bits() & 0x00000F00)  ==
          0x00000700) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000A00) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000600) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000200));

  // safety: n  ==
  //          Pc ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000700
       ? 1
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000A00
       ? 2
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000600
       ? 3
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000200
       ? 4
       : 0))))) > (32)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=000x
//    = {baseline: 'VectorLoadStoreMultiple4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0000
//            else 2 + 1
//            if inst(11:8)=0000
//            else 2 + 1
//            if inst(11:8)=0000
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=0000 ||
//            inst(11:8)=0001 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=0 & A(23)=0 & B(11:8)=000x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreMultiple4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=0000
//            else 2,
//       m: Rm,
//       n: Rn,
//       safety: [size(7:6)=11 => UNDEFINED,
//         not type in bitset {'0000', '0001'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultipleTesterCase3
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultipleTesterCase3(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultipleTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~000x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreMultipleTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreMultipleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(7:6)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000000C0)  !=
          0x000000C0);

  // safety: not type in bitset {'0000', '0001'} => DECODER_ERROR
  EXPECT_TRUE(((inst.Bits() & 0x00000F00)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000100));

  // safety: n  ==
  //          Pc ||
  //       d4  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000000
       ? 1
       : 2) + ((inst.Bits() & 0x00000F00)  ==
          0x00000000
       ? 1
       : 2) + ((inst.Bits() & 0x00000F00)  ==
          0x00000000
       ? 1
       : 2)) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=010x
//    = {baseline: 'VectorLoadStoreMultiple3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0100
//            else 2 + 1
//            if inst(11:8)=0100
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 ||
//            inst(5:4)(1)=1 => UNDEFINED,
//         not inst(11:8)=0100 ||
//            inst(11:8)=0101 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=0 & A(23)=0 & B(11:8)=010x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=0100
//            else 2,
//       m: Rm,
//       n: Rn,
//       safety: [size(7:6)=11 ||
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0100', '0101'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultipleTesterCase4
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultipleTesterCase4(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultipleTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~010x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000400) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreMultipleTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreMultipleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(7:6)=11 ||
  //       align(1)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x000000C0)  ==
          0x000000C0) ||
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002)));

  // safety: not type in bitset {'0100', '0101'} => DECODER_ERROR
  EXPECT_TRUE(((inst.Bits() & 0x00000F00)  ==
          0x00000400) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000500));

  // safety: n  ==
  //          Pc ||
  //       d3  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000400
       ? 1
       : 2) + ((inst.Bits() & 0x00000F00)  ==
          0x00000400
       ? 1
       : 2)) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=011x
//    = {baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=0 & A(23)=0 & B(11:8)=011x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultipleTesterCase5
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultipleTesterCase5(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultipleTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~011x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000600) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreMultipleTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreMultipleTester::
               ApplySanityChecks(inst, decoder));

  // safety: type(11:8)=0111 &&
  //       align(1)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000700) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002)));

  // safety: type(11:8)=1010 &&
  //       align(5:4)=11 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000A00) &&
       ((inst.Bits() & 0x00000030)  ==
          0x00000030)));

  // safety: type(11:8)=0110 &&
  //       align(1)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000600) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002)));

  // safety: not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR
  EXPECT_TRUE(((inst.Bits() & 0x00000F00)  ==
          0x00000700) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000A00) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000600) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000200));

  // safety: n  ==
  //          Pc ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000700
       ? 1
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000A00
       ? 2
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000600
       ? 3
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000200
       ? 4
       : 0))))) > (32)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=100x
//    = {baseline: 'VectorLoadStoreMultiple2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=1000
//            else 2 + 1
//            if inst(11:8)=1000 ||
//            inst(11:8)=1001
//            else 2 => UNPREDICTABLE,
//         inst(11:8)=1000 ||
//            inst(11:8)=1001 &&
//            inst(5:4)=11 => UNDEFINED,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=1000 ||
//            inst(11:8)=1001 ||
//            inst(11:8)=0011 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=0 & A(23)=0 & B(11:8)=100x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultipleTesterCase6
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultipleTesterCase6(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultipleTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~100x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000800) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreMultipleTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreMultipleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(7:6)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000000C0)  !=
          0x000000C0);

  // safety: type in bitset {'1000', '1001'} &&
  //       align(5:4)=11 => UNDEFINED
  EXPECT_TRUE(!((((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900)) &&
       ((inst.Bits() & 0x00000030)  ==
          0x00000030)));

  // safety: not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR
  EXPECT_TRUE(((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000300));

  // safety: n  ==
  //          Pc ||
  //       d2 + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000800
       ? 1
       : 2) + (((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900)
       ? 1
       : 2)) > (32)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=1000
//    = {baseline: 'VectorLoadStoreSingle1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 &&
//            inst(7:4)(1:0)=~11 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(2)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=1000
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase7
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase7(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=00 &&
  //       index_align(0)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000)));

  // safety: size(11:10)=01 &&
  //       index_align(1)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000400) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  !=
          0x00000000)));

  // safety: size(11:10)=10 &&
  //       index_align(2)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  !=
          0x00000000)));

  // safety: size(11:10)=10 &&
  //       index_align(1:0)=~00 &&
  //       index_align(1:0)=~11 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000003)));

  // safety: n  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x000F0000) >> 16)) != (15)));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=1001
//    = {baseline: 'VectorLoadStoreSingle2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=1001
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase8
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase8(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=10 &&
  //       index_align(1)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  !=
          0x00000000)));

  // safety: n  ==
  //          Pc ||
  //       d2  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=1010
//    = {baseline: 'VectorLoadStoreSingle3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=1010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase9
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase9(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000A00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=00 &&
  //       index_align(0)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000)));

  // safety: size(11:10)=01 &&
  //       index_align(0)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000400) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000)));

  // safety: size(11:10)=10 &&
  //       index_align(1:0)=~00 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000000)));

  // safety: n  ==
  //          Pc ||
  //       d3  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=1011
//    = {baseline: 'VectorLoadStoreSingle4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=11 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=1011
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase10
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase10(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000B00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=10 &&
  //       index_align(1:0)=11 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  ==
          0x00000003)));

  // safety: n  ==
  //          Pc ||
  //       d4  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=0x00
//    = {baseline: 'VectorLoadStoreSingle1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 &&
//            inst(7:4)(1:0)=~11 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(2)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=0x00
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase11
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase11(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x00
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=00 &&
  //       index_align(0)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000)));

  // safety: size(11:10)=01 &&
  //       index_align(1)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000400) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  !=
          0x00000000)));

  // safety: size(11:10)=10 &&
  //       index_align(2)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  !=
          0x00000000)));

  // safety: size(11:10)=10 &&
  //       index_align(1:0)=~00 &&
  //       index_align(1:0)=~11 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000003)));

  // safety: n  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x000F0000) >> 16)) != (15)));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=0x01
//    = {baseline: 'VectorLoadStoreSingle2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=0x01
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase12
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase12(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x01
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=10 &&
  //       index_align(1)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  !=
          0x00000000)));

  // safety: n  ==
  //          Pc ||
  //       d2  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=0x10
//    = {baseline: 'VectorLoadStoreSingle3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=0x10
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase13
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase13(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x10
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000200) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=00 &&
  //       index_align(0)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000)));

  // safety: size(11:10)=01 &&
  //       index_align(0)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000400) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000)));

  // safety: size(11:10)=10 &&
  //       index_align(1:0)=~00 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000000)));

  // safety: n  ==
  //          Pc ||
  //       d3  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=0x11
//    = {baseline: 'VectorLoadStoreSingle4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=11 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=0x11
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase14
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase14(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x11
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000300) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=10 &&
  //       index_align(1:0)=11 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  ==
          0x00000003)));

  // safety: n  ==
  //          Pc ||
  //       d4  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=0010
//    = {baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=1 & A(23)=0 & B(11:8)=0010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultipleTesterCase15
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultipleTesterCase15(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultipleTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~0010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000200) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreMultipleTesterCase15
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreMultipleTester::
               ApplySanityChecks(inst, decoder));

  // safety: type(11:8)=0111 &&
  //       align(1)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000700) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002)));

  // safety: type(11:8)=1010 &&
  //       align(5:4)=11 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000A00) &&
       ((inst.Bits() & 0x00000030)  ==
          0x00000030)));

  // safety: type(11:8)=0110 &&
  //       align(1)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000600) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002)));

  // safety: not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR
  EXPECT_TRUE(((inst.Bits() & 0x00000F00)  ==
          0x00000700) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000A00) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000600) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000200));

  // safety: n  ==
  //          Pc ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000700
       ? 1
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000A00
       ? 2
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000600
       ? 3
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000200
       ? 4
       : 0))))) > (32)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=0011
//    = {baseline: 'VectorLoadStoreMultiple2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=1000
//            else 2 + 1
//            if inst(11:8)=1000 ||
//            inst(11:8)=1001
//            else 2 => UNPREDICTABLE,
//         inst(11:8)=1000 ||
//            inst(11:8)=1001 &&
//            inst(5:4)=11 => UNDEFINED,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=1000 ||
//            inst(11:8)=1001 ||
//            inst(11:8)=0011 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=1 & A(23)=0 & B(11:8)=0011
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultipleTesterCase16
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultipleTesterCase16(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultipleTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~0011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000300) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreMultipleTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreMultipleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(7:6)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000000C0)  !=
          0x000000C0);

  // safety: type in bitset {'1000', '1001'} &&
  //       align(5:4)=11 => UNDEFINED
  EXPECT_TRUE(!((((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900)) &&
       ((inst.Bits() & 0x00000030)  ==
          0x00000030)));

  // safety: not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR
  EXPECT_TRUE(((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000300));

  // safety: n  ==
  //          Pc ||
  //       d2 + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000800
       ? 1
       : 2) + (((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900)
       ? 1
       : 2)) > (32)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=1010
//    = {baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=1 & A(23)=0 & B(11:8)=1010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultipleTesterCase17
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultipleTesterCase17(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultipleTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~1010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000A00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreMultipleTesterCase17
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreMultipleTester::
               ApplySanityChecks(inst, decoder));

  // safety: type(11:8)=0111 &&
  //       align(1)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000700) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002)));

  // safety: type(11:8)=1010 &&
  //       align(5:4)=11 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000A00) &&
       ((inst.Bits() & 0x00000030)  ==
          0x00000030)));

  // safety: type(11:8)=0110 &&
  //       align(1)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000600) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002)));

  // safety: not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR
  EXPECT_TRUE(((inst.Bits() & 0x00000F00)  ==
          0x00000700) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000A00) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000600) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000200));

  // safety: n  ==
  //          Pc ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000700
       ? 1
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000A00
       ? 2
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000600
       ? 3
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000200
       ? 4
       : 0))))) > (32)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=000x
//    = {baseline: 'VectorLoadStoreMultiple4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0000
//            else 2 + 1
//            if inst(11:8)=0000
//            else 2 + 1
//            if inst(11:8)=0000
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=0000 ||
//            inst(11:8)=0001 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=1 & A(23)=0 & B(11:8)=000x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreMultiple4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=0000
//            else 2,
//       m: Rm,
//       n: Rn,
//       safety: [size(7:6)=11 => UNDEFINED,
//         not type in bitset {'0000', '0001'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultipleTesterCase18
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultipleTesterCase18(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultipleTesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~000x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreMultipleTesterCase18
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreMultipleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(7:6)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000000C0)  !=
          0x000000C0);

  // safety: not type in bitset {'0000', '0001'} => DECODER_ERROR
  EXPECT_TRUE(((inst.Bits() & 0x00000F00)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000100));

  // safety: n  ==
  //          Pc ||
  //       d4  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000000
       ? 1
       : 2) + ((inst.Bits() & 0x00000F00)  ==
          0x00000000
       ? 1
       : 2) + ((inst.Bits() & 0x00000F00)  ==
          0x00000000
       ? 1
       : 2)) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=010x
//    = {baseline: 'VectorLoadStoreMultiple3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0100
//            else 2 + 1
//            if inst(11:8)=0100
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 ||
//            inst(5:4)(1)=1 => UNDEFINED,
//         not inst(11:8)=0100 ||
//            inst(11:8)=0101 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=1 & A(23)=0 & B(11:8)=010x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=0100
//            else 2,
//       m: Rm,
//       n: Rn,
//       safety: [size(7:6)=11 ||
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0100', '0101'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultipleTesterCase19
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultipleTesterCase19(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultipleTesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~010x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000400) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreMultipleTesterCase19
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreMultipleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(7:6)=11 ||
  //       align(1)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x000000C0)  ==
          0x000000C0) ||
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002)));

  // safety: not type in bitset {'0100', '0101'} => DECODER_ERROR
  EXPECT_TRUE(((inst.Bits() & 0x00000F00)  ==
          0x00000400) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000500));

  // safety: n  ==
  //          Pc ||
  //       d3  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000400
       ? 1
       : 2) + ((inst.Bits() & 0x00000F00)  ==
          0x00000400
       ? 1
       : 2)) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=011x
//    = {baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=1 & A(23)=0 & B(11:8)=011x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultipleTesterCase20
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultipleTesterCase20(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultipleTesterCase20
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~011x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000600) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreMultipleTesterCase20
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreMultipleTester::
               ApplySanityChecks(inst, decoder));

  // safety: type(11:8)=0111 &&
  //       align(1)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000700) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002)));

  // safety: type(11:8)=1010 &&
  //       align(5:4)=11 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000A00) &&
       ((inst.Bits() & 0x00000030)  ==
          0x00000030)));

  // safety: type(11:8)=0110 &&
  //       align(1)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00)  ==
          0x00000600) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002)));

  // safety: not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR
  EXPECT_TRUE(((inst.Bits() & 0x00000F00)  ==
          0x00000700) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000A00) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000600) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000200));

  // safety: n  ==
  //          Pc ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000700
       ? 1
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000A00
       ? 2
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000600
       ? 3
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000200
       ? 4
       : 0))))) > (32)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=100x
//    = {baseline: 'VectorLoadStoreMultiple2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=1000
//            else 2 + 1
//            if inst(11:8)=1000 ||
//            inst(11:8)=1001
//            else 2 => UNPREDICTABLE,
//         inst(11:8)=1000 ||
//            inst(11:8)=1001 &&
//            inst(5:4)=11 => UNDEFINED,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=1000 ||
//            inst(11:8)=1001 ||
//            inst(11:8)=0011 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=1 & A(23)=0 & B(11:8)=100x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultipleTesterCase21
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultipleTesterCase21(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultipleTesterCase21
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~100x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000800) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreMultipleTesterCase21
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreMultipleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(7:6)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000000C0)  !=
          0x000000C0);

  // safety: type in bitset {'1000', '1001'} &&
  //       align(5:4)=11 => UNDEFINED
  EXPECT_TRUE(!((((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900)) &&
       ((inst.Bits() & 0x00000030)  ==
          0x00000030)));

  // safety: not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR
  EXPECT_TRUE(((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000300));

  // safety: n  ==
  //          Pc ||
  //       d2 + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000800
       ? 1
       : 2) + (((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900)
       ? 1
       : 2)) > (32)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1000
//    = {baseline: 'VectorLoadStoreSingle1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 &&
//            inst(7:4)(1:0)=~11 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(2)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1000
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase22
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase22(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase22
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase22
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=00 &&
  //       index_align(0)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000)));

  // safety: size(11:10)=01 &&
  //       index_align(1)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000400) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  !=
          0x00000000)));

  // safety: size(11:10)=10 &&
  //       index_align(2)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  !=
          0x00000000)));

  // safety: size(11:10)=10 &&
  //       index_align(1:0)=~00 &&
  //       index_align(1:0)=~11 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000003)));

  // safety: n  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x000F0000) >> 16)) != (15)));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1001
//    = {baseline: 'VectorLoadStoreSingle2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1001
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase23
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase23(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase23
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase23
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=10 &&
  //       index_align(1)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  !=
          0x00000000)));

  // safety: n  ==
  //          Pc ||
  //       d2  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1010
//    = {baseline: 'VectorLoadStoreSingle3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase24
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase24(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase24
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000A00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase24
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=00 &&
  //       index_align(0)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000)));

  // safety: size(11:10)=01 &&
  //       index_align(0)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000400) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000)));

  // safety: size(11:10)=10 &&
  //       index_align(1:0)=~00 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000000)));

  // safety: n  ==
  //          Pc ||
  //       d3  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1011
//    = {baseline: 'VectorLoadStoreSingle4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=11 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1011
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase25
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase25(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase25
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000B00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase25
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=10 &&
  //       index_align(1:0)=11 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  ==
          0x00000003)));

  // safety: n  ==
  //          Pc ||
  //       d4  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1100
//    = {baseline: 'VectorLoadSingle1AllLanes',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(5)=0
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 ||
//            (inst(7:6)=00 &&
//            inst(4)=1) => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1100
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       base: n,
//       baseline: VectorLoadSingle1AllLanes,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if T(5)=0
//            else 2,
//       safety: [size(7:6)=11 ||
//            (size(7:6)=00 &&
//            a(4)=1) => UNDEFINED,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       wback: (m  !=
//               Pc)}
class VectorLoadSingleAllLanesTesterCase26
    : public VectorLoadSingleAllLanesTester {
 public:
  VectorLoadSingleAllLanesTesterCase26(const NamedClassDecoder& decoder)
    : VectorLoadSingleAllLanesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadSingleAllLanesTesterCase26
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000C00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadSingleAllLanesTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadSingleAllLanesTesterCase26
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadSingleAllLanesTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(7:6)=11 ||
  //       (size(7:6)=00 &&
  //       a(4)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x000000C0)  ==
          0x000000C0) ||
       ((((inst.Bits() & 0x000000C0)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00000010)  ==
          0x00000010)))));

  // safety: n  ==
  //          Pc ||
  //       d + regs  >
  //          32 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000020)  ==
          0x00000000
       ? 1
       : 2)) > (32)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1101
//    = {baseline: 'VectorLoadSingle2AllLanes',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(5)=0
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1101
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       T: T(5),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadSingle2AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22), Rn(19:16), Vd(15:12), size(7:6), T(5), Rm(3:0)],
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       safety: [size(7:6)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       wback: (m  !=
//               Pc)}
class VectorLoadSingleAllLanesTesterCase27
    : public VectorLoadSingleAllLanesTester {
 public:
  VectorLoadSingleAllLanesTesterCase27(const NamedClassDecoder& decoder)
    : VectorLoadSingleAllLanesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadSingleAllLanesTesterCase27
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadSingleAllLanesTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadSingleAllLanesTesterCase27
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadSingleAllLanesTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(7:6)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000000C0)  !=
          0x000000C0);

  // safety: n  ==
  //          Pc ||
  //       d2  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000020)  ==
          0x00000000
       ? 1
       : 2)) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1110
//    = {baseline: 'VectorLoadSingle3AllLanes',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(5)=0
//            else 2 + 1
//            if inst(5)=0
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 ||
//            inst(4)=1 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1110
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       base: n,
//       baseline: VectorLoadSingle3AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       safety: [size(7:6)=11 ||
//            a(4)=1 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       wback: (m  !=
//               Pc)}
class VectorLoadSingleAllLanesTesterCase28
    : public VectorLoadSingleAllLanesTester {
 public:
  VectorLoadSingleAllLanesTesterCase28(const NamedClassDecoder& decoder)
    : VectorLoadSingleAllLanesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadSingleAllLanesTesterCase28
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000E00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadSingleAllLanesTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadSingleAllLanesTesterCase28
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadSingleAllLanesTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(7:6)=11 ||
  //       a(4)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x000000C0)  ==
          0x000000C0) ||
       ((inst.Bits() & 0x00000010)  ==
          0x00000010)));

  // safety: n  ==
  //          Pc ||
  //       d3  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000020)  ==
          0x00000000
       ? 1
       : 2) + ((inst.Bits() & 0x00000020)  ==
          0x00000000
       ? 1
       : 2)) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1111
//    = {baseline: 'VectorLoadSingle4AllLanes',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(5)=0
//            else 2 + 1
//            if inst(5)=0
//            else 2 + 1
//            if inst(5)=0
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 &&
//            inst(4)=0 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1111
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       base: n,
//       baseline: VectorLoadSingle4AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       safety: [size(7:6)=11 &&
//            a(4)=0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       wback: (m  !=
//               Pc)}
class VectorLoadSingleAllLanesTesterCase29
    : public VectorLoadSingleAllLanesTester {
 public:
  VectorLoadSingleAllLanesTesterCase29(const NamedClassDecoder& decoder)
    : VectorLoadSingleAllLanesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadSingleAllLanesTesterCase29
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadSingleAllLanesTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadSingleAllLanesTesterCase29
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadSingleAllLanesTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(7:6)=11 &&
  //       a(4)=0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x000000C0)  ==
          0x000000C0) &&
       ((inst.Bits() & 0x00000010)  ==
          0x00000000)));

  // safety: n  ==
  //          Pc ||
  //       d4  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000020)  ==
          0x00000000
       ? 1
       : 2) + ((inst.Bits() & 0x00000020)  ==
          0x00000000
       ? 1
       : 2) + ((inst.Bits() & 0x00000020)  ==
          0x00000000
       ? 1
       : 2)) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=0x00
//    = {baseline: 'VectorLoadStoreSingle1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 &&
//            inst(7:4)(1:0)=~11 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(2)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=0x00
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase30
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase30(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase30
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x00
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase30
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=00 &&
  //       index_align(0)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000)));

  // safety: size(11:10)=01 &&
  //       index_align(1)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000400) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  !=
          0x00000000)));

  // safety: size(11:10)=10 &&
  //       index_align(2)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  !=
          0x00000000)));

  // safety: size(11:10)=10 &&
  //       index_align(1:0)=~00 &&
  //       index_align(1:0)=~11 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000003)));

  // safety: n  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x000F0000) >> 16)) != (15)));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=0x01
//    = {baseline: 'VectorLoadStoreSingle2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=0x01
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase31
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase31(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase31
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x01
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase31
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=10 &&
  //       index_align(1)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  !=
          0x00000000)));

  // safety: n  ==
  //          Pc ||
  //       d2  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=0x10
//    = {baseline: 'VectorLoadStoreSingle3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=0x10
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase32
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase32(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase32
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x10
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000200) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase32
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=00 &&
  //       index_align(0)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000)));

  // safety: size(11:10)=01 &&
  //       index_align(0)=~0 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000400) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000)));

  // safety: size(11:10)=10 &&
  //       index_align(1:0)=~00 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000000)));

  // safety: n  ==
  //          Pc ||
  //       d3  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=0x11
//    = {baseline: 'VectorLoadStoreSingle4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=11 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=0x11
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingleTesterCase33
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingleTesterCase33(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingleTesterCase33
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x11
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000300) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorLoadStoreSingleTesterCase33
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorLoadStoreSingleTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(11:10)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000C00)  !=
          0x00000C00);

  // safety: size(11:10)=10 &&
  //       index_align(1:0)=11 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  ==
          0x00000003)));

  // safety: n  ==
  //          Pc ||
  //       d4  >
  //          31 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31)))));

  // defs: {base}
  //       if wback
  //       else {};
  EXPECT_TRUE(decoder.defs(inst).IsSame((((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList())));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=0010
//    = {baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VST1_multiple_single_elements',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representative case:
// L(21)=0 & A(23)=0 & B(11:8)=0010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1Tester_Case0
    : public VectorLoadStoreMultipleTesterCase0 {
 public:
  VectorLoadStoreMultiple1Tester_Case0()
    : VectorLoadStoreMultipleTesterCase0(
      state_.VectorLoadStoreMultiple1_VST1_multiple_single_elements_instance_)
  {}
};

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=0011
//    = {baseline: 'VectorLoadStoreMultiple2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VST2_multiple_2_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=1000
//            else 2 + 1
//            if inst(11:8)=1000 ||
//            inst(11:8)=1001
//            else 2 => UNPREDICTABLE,
//         inst(11:8)=1000 ||
//            inst(11:8)=1001 &&
//            inst(5:4)=11 => UNDEFINED,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=1000 ||
//            inst(11:8)=1001 ||
//            inst(11:8)=0011 => DECODER_ERROR]}
//
// Representative case:
// L(21)=0 & A(23)=0 & B(11:8)=0011
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VST2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple2Tester_Case1
    : public VectorLoadStoreMultipleTesterCase1 {
 public:
  VectorLoadStoreMultiple2Tester_Case1()
    : VectorLoadStoreMultipleTesterCase1(
      state_.VectorLoadStoreMultiple2_VST2_multiple_2_element_structures_instance_)
  {}
};

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=1010
//    = {baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VST1_multiple_single_elements',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representative case:
// L(21)=0 & A(23)=0 & B(11:8)=1010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1Tester_Case2
    : public VectorLoadStoreMultipleTesterCase2 {
 public:
  VectorLoadStoreMultiple1Tester_Case2()
    : VectorLoadStoreMultipleTesterCase2(
      state_.VectorLoadStoreMultiple1_VST1_multiple_single_elements_instance_)
  {}
};

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=000x
//    = {baseline: 'VectorLoadStoreMultiple4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VST4_multiple_4_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0000
//            else 2 + 1
//            if inst(11:8)=0000
//            else 2 + 1
//            if inst(11:8)=0000
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=0000 ||
//            inst(11:8)=0001 => DECODER_ERROR]}
//
// Representative case:
// L(21)=0 & A(23)=0 & B(11:8)=000x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreMultiple4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=0000
//            else 2,
//       m: Rm,
//       n: Rn,
//       rule: VST4_multiple_4_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         not type in bitset {'0000', '0001'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple4Tester_Case3
    : public VectorLoadStoreMultipleTesterCase3 {
 public:
  VectorLoadStoreMultiple4Tester_Case3()
    : VectorLoadStoreMultipleTesterCase3(
      state_.VectorLoadStoreMultiple4_VST4_multiple_4_element_structures_instance_)
  {}
};

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=010x
//    = {baseline: 'VectorLoadStoreMultiple3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VST3_multiple_3_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0100
//            else 2 + 1
//            if inst(11:8)=0100
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 ||
//            inst(5:4)(1)=1 => UNDEFINED,
//         not inst(11:8)=0100 ||
//            inst(11:8)=0101 => DECODER_ERROR]}
//
// Representative case:
// L(21)=0 & A(23)=0 & B(11:8)=010x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=0100
//            else 2,
//       m: Rm,
//       n: Rn,
//       rule: VST3_multiple_3_element_structures,
//       safety: [size(7:6)=11 ||
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0100', '0101'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple3Tester_Case4
    : public VectorLoadStoreMultipleTesterCase4 {
 public:
  VectorLoadStoreMultiple3Tester_Case4()
    : VectorLoadStoreMultipleTesterCase4(
      state_.VectorLoadStoreMultiple3_VST3_multiple_3_element_structures_instance_)
  {}
};

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=011x
//    = {baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VST1_multiple_single_elements',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representative case:
// L(21)=0 & A(23)=0 & B(11:8)=011x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1Tester_Case5
    : public VectorLoadStoreMultipleTesterCase5 {
 public:
  VectorLoadStoreMultiple1Tester_Case5()
    : VectorLoadStoreMultipleTesterCase5(
      state_.VectorLoadStoreMultiple1_VST1_multiple_single_elements_instance_)
  {}
};

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=100x
//    = {baseline: 'VectorLoadStoreMultiple2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VST2_multiple_2_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=1000
//            else 2 + 1
//            if inst(11:8)=1000 ||
//            inst(11:8)=1001
//            else 2 => UNPREDICTABLE,
//         inst(11:8)=1000 ||
//            inst(11:8)=1001 &&
//            inst(5:4)=11 => UNDEFINED,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=1000 ||
//            inst(11:8)=1001 ||
//            inst(11:8)=0011 => DECODER_ERROR]}
//
// Representative case:
// L(21)=0 & A(23)=0 & B(11:8)=100x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VST2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple2Tester_Case6
    : public VectorLoadStoreMultipleTesterCase6 {
 public:
  VectorLoadStoreMultiple2Tester_Case6()
    : VectorLoadStoreMultipleTesterCase6(
      state_.VectorLoadStoreMultiple2_VST2_multiple_2_element_structures_instance_)
  {}
};

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=1000
//    = {baseline: 'VectorLoadStoreSingle1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VST1_single_element_from_one_lane',
//       safety: [15  ==
//               inst(19:16) => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 &&
//            inst(7:4)(1:0)=~11 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(2)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=0 & A(23)=1 & B(11:8)=1000
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VST1_single_element_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle1Tester_Case7
    : public VectorLoadStoreSingleTesterCase7 {
 public:
  VectorLoadStoreSingle1Tester_Case7()
    : VectorLoadStoreSingleTesterCase7(
      state_.VectorLoadStoreSingle1_VST1_single_element_from_one_lane_instance_)
  {}
};

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=1001
//    = {baseline: 'VectorLoadStoreSingle2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VST2_single_2_element_structure_from_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=0 & A(23)=1 & B(11:8)=1001
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VST2_single_2_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle2Tester_Case8
    : public VectorLoadStoreSingleTesterCase8 {
 public:
  VectorLoadStoreSingle2Tester_Case8()
    : VectorLoadStoreSingleTesterCase8(
      state_.VectorLoadStoreSingle2_VST2_single_2_element_structure_from_one_lane_instance_)
  {}
};

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=1010
//    = {baseline: 'VectorLoadStoreSingle3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VST3_single_3_element_structure_from_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=0 & A(23)=1 & B(11:8)=1010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VST3_single_3_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle3Tester_Case9
    : public VectorLoadStoreSingleTesterCase9 {
 public:
  VectorLoadStoreSingle3Tester_Case9()
    : VectorLoadStoreSingleTesterCase9(
      state_.VectorLoadStoreSingle3_VST3_single_3_element_structure_from_one_lane_instance_)
  {}
};

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=1011
//    = {baseline: 'VectorLoadStoreSingle4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VST4_single_4_element_structure_form_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=11 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=0 & A(23)=1 & B(11:8)=1011
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VST4_single_4_element_structure_form_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle4Tester_Case10
    : public VectorLoadStoreSingleTesterCase10 {
 public:
  VectorLoadStoreSingle4Tester_Case10()
    : VectorLoadStoreSingleTesterCase10(
      state_.VectorLoadStoreSingle4_VST4_single_4_element_structure_form_one_lane_instance_)
  {}
};

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=0x00
//    = {baseline: 'VectorLoadStoreSingle1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VST1_single_element_from_one_lane',
//       safety: [15  ==
//               inst(19:16) => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 &&
//            inst(7:4)(1:0)=~11 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(2)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=0 & A(23)=1 & B(11:8)=0x00
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VST1_single_element_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle1Tester_Case11
    : public VectorLoadStoreSingleTesterCase11 {
 public:
  VectorLoadStoreSingle1Tester_Case11()
    : VectorLoadStoreSingleTesterCase11(
      state_.VectorLoadStoreSingle1_VST1_single_element_from_one_lane_instance_)
  {}
};

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=0x01
//    = {baseline: 'VectorLoadStoreSingle2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VST2_single_2_element_structure_from_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=0 & A(23)=1 & B(11:8)=0x01
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VST2_single_2_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle2Tester_Case12
    : public VectorLoadStoreSingleTesterCase12 {
 public:
  VectorLoadStoreSingle2Tester_Case12()
    : VectorLoadStoreSingleTesterCase12(
      state_.VectorLoadStoreSingle2_VST2_single_2_element_structure_from_one_lane_instance_)
  {}
};

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=0x10
//    = {baseline: 'VectorLoadStoreSingle3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VST3_single_3_element_structure_from_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=0 & A(23)=1 & B(11:8)=0x10
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VST3_single_3_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle3Tester_Case13
    : public VectorLoadStoreSingleTesterCase13 {
 public:
  VectorLoadStoreSingle3Tester_Case13()
    : VectorLoadStoreSingleTesterCase13(
      state_.VectorLoadStoreSingle3_VST3_single_3_element_structure_from_one_lane_instance_)
  {}
};

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=0x11
//    = {baseline: 'VectorLoadStoreSingle4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VST4_single_4_element_structure_form_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=11 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=0 & A(23)=1 & B(11:8)=0x11
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VST4_single_4_element_structure_form_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle4Tester_Case14
    : public VectorLoadStoreSingleTesterCase14 {
 public:
  VectorLoadStoreSingle4Tester_Case14()
    : VectorLoadStoreSingleTesterCase14(
      state_.VectorLoadStoreSingle4_VST4_single_4_element_structure_form_one_lane_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=0010
//    = {baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD1_multiple_single_elements',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representative case:
// L(21)=1 & A(23)=0 & B(11:8)=0010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VLD1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1Tester_Case15
    : public VectorLoadStoreMultipleTesterCase15 {
 public:
  VectorLoadStoreMultiple1Tester_Case15()
    : VectorLoadStoreMultipleTesterCase15(
      state_.VectorLoadStoreMultiple1_VLD1_multiple_single_elements_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=0011
//    = {baseline: 'VectorLoadStoreMultiple2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD2_multiple_2_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=1000
//            else 2 + 1
//            if inst(11:8)=1000 ||
//            inst(11:8)=1001
//            else 2 => UNPREDICTABLE,
//         inst(11:8)=1000 ||
//            inst(11:8)=1001 &&
//            inst(5:4)=11 => UNDEFINED,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=1000 ||
//            inst(11:8)=1001 ||
//            inst(11:8)=0011 => DECODER_ERROR]}
//
// Representative case:
// L(21)=1 & A(23)=0 & B(11:8)=0011
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VLD2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple2Tester_Case16
    : public VectorLoadStoreMultipleTesterCase16 {
 public:
  VectorLoadStoreMultiple2Tester_Case16()
    : VectorLoadStoreMultipleTesterCase16(
      state_.VectorLoadStoreMultiple2_VLD2_multiple_2_element_structures_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=1010
//    = {baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD1_multiple_single_elements',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representative case:
// L(21)=1 & A(23)=0 & B(11:8)=1010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VLD1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1Tester_Case17
    : public VectorLoadStoreMultipleTesterCase17 {
 public:
  VectorLoadStoreMultiple1Tester_Case17()
    : VectorLoadStoreMultipleTesterCase17(
      state_.VectorLoadStoreMultiple1_VLD1_multiple_single_elements_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=000x
//    = {baseline: 'VectorLoadStoreMultiple4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD4_multiple_4_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0000
//            else 2 + 1
//            if inst(11:8)=0000
//            else 2 + 1
//            if inst(11:8)=0000
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=0000 ||
//            inst(11:8)=0001 => DECODER_ERROR]}
//
// Representative case:
// L(21)=1 & A(23)=0 & B(11:8)=000x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreMultiple4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=0000
//            else 2,
//       m: Rm,
//       n: Rn,
//       rule: VLD4_multiple_4_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         not type in bitset {'0000', '0001'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple4Tester_Case18
    : public VectorLoadStoreMultipleTesterCase18 {
 public:
  VectorLoadStoreMultiple4Tester_Case18()
    : VectorLoadStoreMultipleTesterCase18(
      state_.VectorLoadStoreMultiple4_VLD4_multiple_4_element_structures_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=010x
//    = {baseline: 'VectorLoadStoreMultiple3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD3_multiple_3_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0100
//            else 2 + 1
//            if inst(11:8)=0100
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 ||
//            inst(5:4)(1)=1 => UNDEFINED,
//         not inst(11:8)=0100 ||
//            inst(11:8)=0101 => DECODER_ERROR]}
//
// Representative case:
// L(21)=1 & A(23)=0 & B(11:8)=010x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=0100
//            else 2,
//       m: Rm,
//       n: Rn,
//       rule: VLD3_multiple_3_element_structures,
//       safety: [size(7:6)=11 ||
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0100', '0101'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple3Tester_Case19
    : public VectorLoadStoreMultipleTesterCase19 {
 public:
  VectorLoadStoreMultiple3Tester_Case19()
    : VectorLoadStoreMultipleTesterCase19(
      state_.VectorLoadStoreMultiple3_VLD3_multiple_3_element_structures_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=011x
//    = {baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD1_multiple_single_elements',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representative case:
// L(21)=1 & A(23)=0 & B(11:8)=011x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VLD1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1Tester_Case20
    : public VectorLoadStoreMultipleTesterCase20 {
 public:
  VectorLoadStoreMultiple1Tester_Case20()
    : VectorLoadStoreMultipleTesterCase20(
      state_.VectorLoadStoreMultiple1_VLD1_multiple_single_elements_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=100x
//    = {baseline: 'VectorLoadStoreMultiple2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD2_multiple_2_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=1000
//            else 2 + 1
//            if inst(11:8)=1000 ||
//            inst(11:8)=1001
//            else 2 => UNPREDICTABLE,
//         inst(11:8)=1000 ||
//            inst(11:8)=1001 &&
//            inst(5:4)=11 => UNDEFINED,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=1000 ||
//            inst(11:8)=1001 ||
//            inst(11:8)=0011 => DECODER_ERROR]}
//
// Representative case:
// L(21)=1 & A(23)=0 & B(11:8)=100x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VLD2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple2Tester_Case21
    : public VectorLoadStoreMultipleTesterCase21 {
 public:
  VectorLoadStoreMultiple2Tester_Case21()
    : VectorLoadStoreMultipleTesterCase21(
      state_.VectorLoadStoreMultiple2_VLD2_multiple_2_element_structures_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1000
//    = {baseline: 'VectorLoadStoreSingle1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD1_single_element_to_one_lane',
//       safety: [15  ==
//               inst(19:16) => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 &&
//            inst(7:4)(1:0)=~11 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(2)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=1 & A(23)=1 & B(11:8)=1000
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VLD1_single_element_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle1Tester_Case22
    : public VectorLoadStoreSingleTesterCase22 {
 public:
  VectorLoadStoreSingle1Tester_Case22()
    : VectorLoadStoreSingleTesterCase22(
      state_.VectorLoadStoreSingle1_VLD1_single_element_to_one_lane_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1001
//    = {baseline: 'VectorLoadStoreSingle2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD2_single_2_element_structure_to_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=1 & A(23)=1 & B(11:8)=1001
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VLD2_single_2_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle2Tester_Case23
    : public VectorLoadStoreSingleTesterCase23 {
 public:
  VectorLoadStoreSingle2Tester_Case23()
    : VectorLoadStoreSingleTesterCase23(
      state_.VectorLoadStoreSingle2_VLD2_single_2_element_structure_to_one_lane_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1010
//    = {baseline: 'VectorLoadStoreSingle3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD3_single_3_element_structure_to_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=1 & A(23)=1 & B(11:8)=1010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VLD3_single_3_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle3Tester_Case24
    : public VectorLoadStoreSingleTesterCase24 {
 public:
  VectorLoadStoreSingle3Tester_Case24()
    : VectorLoadStoreSingleTesterCase24(
      state_.VectorLoadStoreSingle3_VLD3_single_3_element_structure_to_one_lane_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1011
//    = {baseline: 'VectorLoadStoreSingle4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD4_single_4_element_structure_to_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=11 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=1 & A(23)=1 & B(11:8)=1011
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VLD4_single_4_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle4Tester_Case25
    : public VectorLoadStoreSingleTesterCase25 {
 public:
  VectorLoadStoreSingle4Tester_Case25()
    : VectorLoadStoreSingleTesterCase25(
      state_.VectorLoadStoreSingle4_VLD4_single_4_element_structure_to_one_lane_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1100
//    = {baseline: 'VectorLoadSingle1AllLanes',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD1_single_element_to_all_lanes',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(5)=0
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 ||
//            (inst(7:6)=00 &&
//            inst(4)=1) => UNDEFINED]}
//
// Representative case:
// L(21)=1 & A(23)=1 & B(11:8)=1100
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       base: n,
//       baseline: VectorLoadSingle1AllLanes,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       regs: 1
//            if T(5)=0
//            else 2,
//       rule: VLD1_single_element_to_all_lanes,
//       safety: [size(7:6)=11 ||
//            (size(7:6)=00 &&
//            a(4)=1) => UNDEFINED,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       wback: (m  !=
//               Pc)}
class VectorLoadSingle1AllLanesTester_Case26
    : public VectorLoadSingleAllLanesTesterCase26 {
 public:
  VectorLoadSingle1AllLanesTester_Case26()
    : VectorLoadSingleAllLanesTesterCase26(
      state_.VectorLoadSingle1AllLanes_VLD1_single_element_to_all_lanes_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1101
//    = {baseline: 'VectorLoadSingle2AllLanes',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD2_single_2_element_structure_to_all_lanes',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(5)=0
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=1 & A(23)=1 & B(11:8)=1101
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       T: T(5),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadSingle2AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22), Rn(19:16), Vd(15:12), size(7:6), T(5), Rm(3:0)],
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       rule: VLD2_single_2_element_structure_to_all_lanes,
//       safety: [size(7:6)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       wback: (m  !=
//               Pc)}
class VectorLoadSingle2AllLanesTester_Case27
    : public VectorLoadSingleAllLanesTesterCase27 {
 public:
  VectorLoadSingle2AllLanesTester_Case27()
    : VectorLoadSingleAllLanesTesterCase27(
      state_.VectorLoadSingle2AllLanes_VLD2_single_2_element_structure_to_all_lanes_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1110
//    = {baseline: 'VectorLoadSingle3AllLanes',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD3_single_3_element_structure_to_all_lanes',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(5)=0
//            else 2 + 1
//            if inst(5)=0
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 ||
//            inst(4)=1 => UNDEFINED]}
//
// Representative case:
// L(21)=1 & A(23)=1 & B(11:8)=1110
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       base: n,
//       baseline: VectorLoadSingle3AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       rule: VLD3_single_3_element_structure_to_all_lanes,
//       safety: [size(7:6)=11 ||
//            a(4)=1 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       wback: (m  !=
//               Pc)}
class VectorLoadSingle3AllLanesTester_Case28
    : public VectorLoadSingleAllLanesTesterCase28 {
 public:
  VectorLoadSingle3AllLanesTester_Case28()
    : VectorLoadSingleAllLanesTesterCase28(
      state_.VectorLoadSingle3AllLanes_VLD3_single_3_element_structure_to_all_lanes_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1111
//    = {baseline: 'VectorLoadSingle4AllLanes',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD4_single_4_element_structure_to_all_lanes',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(5)=0
//            else 2 + 1
//            if inst(5)=0
//            else 2 + 1
//            if inst(5)=0
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 &&
//            inst(4)=0 => UNDEFINED]}
//
// Representative case:
// L(21)=1 & A(23)=1 & B(11:8)=1111
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       base: n,
//       baseline: VectorLoadSingle4AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       rule: VLD4_single_4_element_structure_to_all_lanes,
//       safety: [size(7:6)=11 &&
//            a(4)=0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       wback: (m  !=
//               Pc)}
class VectorLoadSingle4AllLanesTester_Case29
    : public VectorLoadSingleAllLanesTesterCase29 {
 public:
  VectorLoadSingle4AllLanesTester_Case29()
    : VectorLoadSingleAllLanesTesterCase29(
      state_.VectorLoadSingle4AllLanes_VLD4_single_4_element_structure_to_all_lanes_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=0x00
//    = {baseline: 'VectorLoadStoreSingle1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD1_single_element_to_one_lane',
//       safety: [15  ==
//               inst(19:16) => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 &&
//            inst(7:4)(1:0)=~11 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(2)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=1 & A(23)=1 & B(11:8)=0x00
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VLD1_single_element_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle1Tester_Case30
    : public VectorLoadStoreSingleTesterCase30 {
 public:
  VectorLoadStoreSingle1Tester_Case30()
    : VectorLoadStoreSingleTesterCase30(
      state_.VectorLoadStoreSingle1_VLD1_single_element_to_one_lane_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=0x01
//    = {baseline: 'VectorLoadStoreSingle2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD2_single_2_element_structure_to_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=1 & A(23)=1 & B(11:8)=0x01
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VLD2_single_2_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle2Tester_Case31
    : public VectorLoadStoreSingleTesterCase31 {
 public:
  VectorLoadStoreSingle2Tester_Case31()
    : VectorLoadStoreSingleTesterCase31(
      state_.VectorLoadStoreSingle2_VLD2_single_2_element_structure_to_one_lane_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=0x10
//    = {baseline: 'VectorLoadStoreSingle3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD3_single_3_element_structure_to_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=1 & A(23)=1 & B(11:8)=0x10
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VLD3_single_3_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle3Tester_Case32
    : public VectorLoadStoreSingleTesterCase32 {
 public:
  VectorLoadStoreSingle3Tester_Case32()
    : VectorLoadStoreSingleTesterCase32(
      state_.VectorLoadStoreSingle3_VLD3_single_3_element_structure_to_one_lane_instance_)
  {}
};

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=0x11
//    = {baseline: 'VectorLoadStoreSingle4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       rule: 'VLD4_single_4_element_structure_to_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=11 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representative case:
// L(21)=1 & A(23)=1 & B(11:8)=0x11
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       rule: VLD4_single_4_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle4Tester_Case33
    : public VectorLoadStoreSingleTesterCase33 {
 public:
  VectorLoadStoreSingle4Tester_Case33()
    : VectorLoadStoreSingleTesterCase33(
      state_.VectorLoadStoreSingle4_VLD4_single_4_element_structure_to_one_lane_instance_)
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
// inst(21)=0 & inst(23)=0 & inst(11:8)=0010
//    = {actual: 'VectorLoadStoreMultiple1',
//       baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101000d00nnnnddddttttssaammmm',
//       rule: 'VST1_multiple_single_elements',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=0 & A(23)=0 & B(11:8)=0010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreMultiple1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple1Tester_Case0_TestCase0) {
  VectorLoadStoreMultiple1Tester_Case0 tester;
  tester.Test("111101000d00nnnnddddttttssaammmm");
}

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=0011
//    = {actual: 'VectorLoadStoreMultiple2',
//       baseline: 'VectorLoadStoreMultiple2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101000d00nnnnddddttttssaammmm',
//       rule: 'VST2_multiple_2_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=1000
//            else 2 + 1
//            if inst(11:8)=1000 ||
//            inst(11:8)=1001
//            else 2 => UNPREDICTABLE,
//         inst(11:8)=1000 ||
//            inst(11:8)=1001 &&
//            inst(5:4)=11 => UNDEFINED,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=1000 ||
//            inst(11:8)=1001 ||
//            inst(11:8)=0011 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=0 & A(23)=0 & B(11:8)=0011
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreMultiple2,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VST2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple2Tester_Case1_TestCase1) {
  VectorLoadStoreMultiple2Tester_Case1 tester;
  tester.Test("111101000d00nnnnddddttttssaammmm");
}

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=1010
//    = {actual: 'VectorLoadStoreMultiple1',
//       baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101000d00nnnnddddttttssaammmm',
//       rule: 'VST1_multiple_single_elements',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=0 & A(23)=0 & B(11:8)=1010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreMultiple1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple1Tester_Case2_TestCase2) {
  VectorLoadStoreMultiple1Tester_Case2 tester;
  tester.Test("111101000d00nnnnddddttttssaammmm");
}

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=000x
//    = {actual: 'VectorLoadStoreMultiple4',
//       baseline: 'VectorLoadStoreMultiple4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101000d00nnnnddddttttssaammmm',
//       rule: 'VST4_multiple_4_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0000
//            else 2 + 1
//            if inst(11:8)=0000
//            else 2 + 1
//            if inst(11:8)=0000
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=0000 ||
//            inst(11:8)=0001 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=0 & A(23)=0 & B(11:8)=000x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreMultiple4,
//       base: n,
//       baseline: VectorLoadStoreMultiple4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=0000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       rule: VST4_multiple_4_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         not type in bitset {'0000', '0001'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple4Tester_Case3_TestCase3) {
  VectorLoadStoreMultiple4Tester_Case3 tester;
  tester.Test("111101000d00nnnnddddttttssaammmm");
}

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=010x
//    = {actual: 'VectorLoadStoreMultiple3',
//       baseline: 'VectorLoadStoreMultiple3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101000d00nnnnddddttttssaammmm',
//       rule: 'VST3_multiple_3_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0100
//            else 2 + 1
//            if inst(11:8)=0100
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 ||
//            inst(5:4)(1)=1 => UNDEFINED,
//         not inst(11:8)=0100 ||
//            inst(11:8)=0101 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=0 & A(23)=0 & B(11:8)=010x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreMultiple3,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=0100
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       rule: VST3_multiple_3_element_structures,
//       safety: [size(7:6)=11 ||
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0100', '0101'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple3Tester_Case4_TestCase4) {
  VectorLoadStoreMultiple3Tester_Case4 tester;
  tester.Test("111101000d00nnnnddddttttssaammmm");
}

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=011x
//    = {actual: 'VectorLoadStoreMultiple1',
//       baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101000d00nnnnddddttttssaammmm',
//       rule: 'VST1_multiple_single_elements',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=0 & A(23)=0 & B(11:8)=011x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreMultiple1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple1Tester_Case5_TestCase5) {
  VectorLoadStoreMultiple1Tester_Case5 tester;
  tester.Test("111101000d00nnnnddddttttssaammmm");
}

// Neutral case:
// inst(21)=0 & inst(23)=0 & inst(11:8)=100x
//    = {actual: 'VectorLoadStoreMultiple2',
//       baseline: 'VectorLoadStoreMultiple2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101000d00nnnnddddttttssaammmm',
//       rule: 'VST2_multiple_2_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=1000
//            else 2 + 1
//            if inst(11:8)=1000 ||
//            inst(11:8)=1001
//            else 2 => UNPREDICTABLE,
//         inst(11:8)=1000 ||
//            inst(11:8)=1001 &&
//            inst(5:4)=11 => UNDEFINED,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=1000 ||
//            inst(11:8)=1001 ||
//            inst(11:8)=0011 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=0 & A(23)=0 & B(11:8)=100x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreMultiple2,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VST2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple2Tester_Case6_TestCase6) {
  VectorLoadStoreMultiple2Tester_Case6 tester;
  tester.Test("111101000d00nnnnddddttttssaammmm");
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=1000
//    = {actual: 'VectorLoadStoreSingle1',
//       baseline: 'VectorLoadStoreSingle1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d00nnnnddddss00aaaammmm',
//       rule: 'VST1_single_element_from_one_lane',
//       safety: [15  ==
//               inst(19:16) => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 &&
//            inst(7:4)(1:0)=~11 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(2)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=1000
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: VectorLoadStoreSingle1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss00aaaammmm,
//       rule: VST1_single_element_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle1Tester_Case7_TestCase7) {
  VectorLoadStoreSingle1Tester_Case7 tester;
  tester.Test("111101001d00nnnnddddss00aaaammmm");
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=1001
//    = {actual: 'VectorLoadStoreSingle2',
//       baseline: 'VectorLoadStoreSingle2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d00nnnnddddss01aaaammmm',
//       rule: 'VST2_single_2_element_structure_from_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=1001
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreSingle2,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss01aaaammmm,
//       rule: VST2_single_2_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle2Tester_Case8_TestCase8) {
  VectorLoadStoreSingle2Tester_Case8 tester;
  tester.Test("111101001d00nnnnddddss01aaaammmm");
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=1010
//    = {actual: 'VectorLoadStoreSingle3',
//       baseline: 'VectorLoadStoreSingle3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d00nnnnddddss10aaaammmm',
//       rule: 'VST3_single_3_element_structure_from_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=1010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreSingle3,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss10aaaammmm,
//       rule: VST3_single_3_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle3Tester_Case9_TestCase9) {
  VectorLoadStoreSingle3Tester_Case9 tester;
  tester.Test("111101001d00nnnnddddss10aaaammmm");
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=1011
//    = {actual: 'VectorLoadStoreSingle4',
//       baseline: 'VectorLoadStoreSingle4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d00nnnnddddss11aaaammmm',
//       rule: 'VST4_single_4_element_structure_form_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=11 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=1011
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreSingle4,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss11aaaammmm,
//       rule: VST4_single_4_element_structure_form_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle4Tester_Case10_TestCase10) {
  VectorLoadStoreSingle4Tester_Case10 tester;
  tester.Test("111101001d00nnnnddddss11aaaammmm");
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=0x00
//    = {actual: 'VectorLoadStoreSingle1',
//       baseline: 'VectorLoadStoreSingle1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d00nnnnddddss00aaaammmm',
//       rule: 'VST1_single_element_from_one_lane',
//       safety: [15  ==
//               inst(19:16) => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 &&
//            inst(7:4)(1:0)=~11 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(2)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=0x00
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: VectorLoadStoreSingle1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss00aaaammmm,
//       rule: VST1_single_element_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle1Tester_Case11_TestCase11) {
  VectorLoadStoreSingle1Tester_Case11 tester;
  tester.Test("111101001d00nnnnddddss00aaaammmm");
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=0x01
//    = {actual: 'VectorLoadStoreSingle2',
//       baseline: 'VectorLoadStoreSingle2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d00nnnnddddss01aaaammmm',
//       rule: 'VST2_single_2_element_structure_from_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=0x01
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreSingle2,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss01aaaammmm,
//       rule: VST2_single_2_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle2Tester_Case12_TestCase12) {
  VectorLoadStoreSingle2Tester_Case12 tester;
  tester.Test("111101001d00nnnnddddss01aaaammmm");
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=0x10
//    = {actual: 'VectorLoadStoreSingle3',
//       baseline: 'VectorLoadStoreSingle3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d00nnnnddddss10aaaammmm',
//       rule: 'VST3_single_3_element_structure_from_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=0x10
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreSingle3,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss10aaaammmm,
//       rule: VST3_single_3_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle3Tester_Case13_TestCase13) {
  VectorLoadStoreSingle3Tester_Case13 tester;
  tester.Test("111101001d00nnnnddddss10aaaammmm");
}

// Neutral case:
// inst(21)=0 & inst(23)=1 & inst(11:8)=0x11
//    = {actual: 'VectorLoadStoreSingle4',
//       baseline: 'VectorLoadStoreSingle4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d00nnnnddddss11aaaammmm',
//       rule: 'VST4_single_4_element_structure_form_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=11 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=0 & A(23)=1 & B(11:8)=0x11
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreSingle4,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss11aaaammmm,
//       rule: VST4_single_4_element_structure_form_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle4Tester_Case14_TestCase14) {
  VectorLoadStoreSingle4Tester_Case14 tester;
  tester.Test("111101001d00nnnnddddss11aaaammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=0010
//    = {actual: 'VectorLoadStoreMultiple1',
//       baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101000d10nnnnddddttttssaammmm',
//       rule: 'VLD1_multiple_single_elements',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=1 & A(23)=0 & B(11:8)=0010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreMultiple1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VLD1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple1Tester_Case15_TestCase15) {
  VectorLoadStoreMultiple1Tester_Case15 tester;
  tester.Test("111101000d10nnnnddddttttssaammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=0011
//    = {actual: 'VectorLoadStoreMultiple2',
//       baseline: 'VectorLoadStoreMultiple2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101000d10nnnnddddttttssaammmm',
//       rule: 'VLD2_multiple_2_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=1000
//            else 2 + 1
//            if inst(11:8)=1000 ||
//            inst(11:8)=1001
//            else 2 => UNPREDICTABLE,
//         inst(11:8)=1000 ||
//            inst(11:8)=1001 &&
//            inst(5:4)=11 => UNDEFINED,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=1000 ||
//            inst(11:8)=1001 ||
//            inst(11:8)=0011 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=1 & A(23)=0 & B(11:8)=0011
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreMultiple2,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VLD2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple2Tester_Case16_TestCase16) {
  VectorLoadStoreMultiple2Tester_Case16 tester;
  tester.Test("111101000d10nnnnddddttttssaammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=1010
//    = {actual: 'VectorLoadStoreMultiple1',
//       baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101000d10nnnnddddttttssaammmm',
//       rule: 'VLD1_multiple_single_elements',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=1 & A(23)=0 & B(11:8)=1010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreMultiple1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VLD1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple1Tester_Case17_TestCase17) {
  VectorLoadStoreMultiple1Tester_Case17 tester;
  tester.Test("111101000d10nnnnddddttttssaammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=000x
//    = {actual: 'VectorLoadStoreMultiple4',
//       baseline: 'VectorLoadStoreMultiple4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101000d10nnnnddddttttssaammmm',
//       rule: 'VLD4_multiple_4_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0000
//            else 2 + 1
//            if inst(11:8)=0000
//            else 2 + 1
//            if inst(11:8)=0000
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=0000 ||
//            inst(11:8)=0001 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=1 & A(23)=0 & B(11:8)=000x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreMultiple4,
//       base: n,
//       baseline: VectorLoadStoreMultiple4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=0000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       rule: VLD4_multiple_4_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         not type in bitset {'0000', '0001'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple4Tester_Case18_TestCase18) {
  VectorLoadStoreMultiple4Tester_Case18 tester;
  tester.Test("111101000d10nnnnddddttttssaammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=010x
//    = {actual: 'VectorLoadStoreMultiple3',
//       baseline: 'VectorLoadStoreMultiple3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101000d10nnnnddddttttssaammmm',
//       rule: 'VLD3_multiple_3_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0100
//            else 2 + 1
//            if inst(11:8)=0100
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 ||
//            inst(5:4)(1)=1 => UNDEFINED,
//         not inst(11:8)=0100 ||
//            inst(11:8)=0101 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=1 & A(23)=0 & B(11:8)=010x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreMultiple3,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=0100
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       rule: VLD3_multiple_3_element_structures,
//       safety: [size(7:6)=11 ||
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0100', '0101'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple3Tester_Case19_TestCase19) {
  VectorLoadStoreMultiple3Tester_Case19 tester;
  tester.Test("111101000d10nnnnddddttttssaammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=011x
//    = {actual: 'VectorLoadStoreMultiple1',
//       baseline: 'VectorLoadStoreMultiple1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101000d10nnnnddddttttssaammmm',
//       rule: 'VLD1_multiple_single_elements',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=0111
//            else 2
//            if inst(11:8)=1010
//            else 3
//            if inst(11:8)=0110
//            else 4
//            if inst(11:8)=0010
//            else 0 => UNPREDICTABLE,
//         inst(11:8)=0110 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=0111 &&
//            inst(5:4)(1)=1 => UNDEFINED,
//         inst(11:8)=1010 &&
//            inst(5:4)=11 => UNDEFINED,
//         not inst(11:8)=0111 ||
//            inst(11:8)=1010 ||
//            inst(11:8)=0110 ||
//            inst(11:8)=0010 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=1 & A(23)=0 & B(11:8)=011x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreMultiple1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VLD1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple1Tester_Case20_TestCase20) {
  VectorLoadStoreMultiple1Tester_Case20 tester;
  tester.Test("111101000d10nnnnddddttttssaammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=0 & inst(11:8)=100x
//    = {actual: 'VectorLoadStoreMultiple2',
//       baseline: 'VectorLoadStoreMultiple2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101000d10nnnnddddttttssaammmm',
//       rule: 'VLD2_multiple_2_element_structures',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:8)=1000
//            else 2 + 1
//            if inst(11:8)=1000 ||
//            inst(11:8)=1001
//            else 2 => UNPREDICTABLE,
//         inst(11:8)=1000 ||
//            inst(11:8)=1001 &&
//            inst(5:4)=11 => UNDEFINED,
//         inst(7:6)=11 => UNDEFINED,
//         not inst(11:8)=1000 ||
//            inst(11:8)=1001 ||
//            inst(11:8)=0011 => DECODER_ERROR]}
//
// Representaive case:
// L(21)=1 & A(23)=0 & B(11:8)=100x
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreMultiple2,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VLD2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple2Tester_Case21_TestCase21) {
  VectorLoadStoreMultiple2Tester_Case21 tester;
  tester.Test("111101000d10nnnnddddttttssaammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1000
//    = {actual: 'VectorLoadStoreSingle1',
//       baseline: 'VectorLoadStoreSingle1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d10nnnnddddss00aaaammmm',
//       rule: 'VLD1_single_element_to_one_lane',
//       safety: [15  ==
//               inst(19:16) => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 &&
//            inst(7:4)(1:0)=~11 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(2)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1000
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: VectorLoadStoreSingle1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss00aaaammmm,
//       rule: VLD1_single_element_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle1Tester_Case22_TestCase22) {
  VectorLoadStoreSingle1Tester_Case22 tester;
  tester.Test("111101001d10nnnnddddss00aaaammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1001
//    = {actual: 'VectorLoadStoreSingle2',
//       baseline: 'VectorLoadStoreSingle2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d10nnnnddddss01aaaammmm',
//       rule: 'VLD2_single_2_element_structure_to_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1001
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreSingle2,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss01aaaammmm,
//       rule: VLD2_single_2_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle2Tester_Case23_TestCase23) {
  VectorLoadStoreSingle2Tester_Case23 tester;
  tester.Test("111101001d10nnnnddddss01aaaammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1010
//    = {actual: 'VectorLoadStoreSingle3',
//       baseline: 'VectorLoadStoreSingle3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d10nnnnddddss10aaaammmm',
//       rule: 'VLD3_single_3_element_structure_to_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1010
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreSingle3,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss10aaaammmm,
//       rule: VLD3_single_3_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle3Tester_Case24_TestCase24) {
  VectorLoadStoreSingle3Tester_Case24 tester;
  tester.Test("111101001d10nnnnddddss10aaaammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1011
//    = {actual: 'VectorLoadStoreSingle4',
//       baseline: 'VectorLoadStoreSingle4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d10nnnnddddss11aaaammmm',
//       rule: 'VLD4_single_4_element_structure_to_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=11 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1011
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreSingle4,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss11aaaammmm,
//       rule: VLD4_single_4_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle4Tester_Case25_TestCase25) {
  VectorLoadStoreSingle4Tester_Case25 tester;
  tester.Test("111101001d10nnnnddddss11aaaammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1100
//    = {actual: 'VectorLoadSingle1AllLanes',
//       baseline: 'VectorLoadSingle1AllLanes',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d10nnnndddd1100sstammmm',
//       rule: 'VLD1_single_element_to_all_lanes',
//       safety: [15  ==
//               inst(19:16) ||
//            32  <=
//               inst(22):inst(15:12) + 1
//            if inst(5)=0
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 ||
//            (inst(7:6)=00 &&
//            inst(4)=1) => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1100
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       actual: VectorLoadSingle1AllLanes,
//       base: n,
//       baseline: VectorLoadSingle1AllLanes,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1100sstammmm,
//       regs: 1
//            if T(5)=0
//            else 2,
//       rule: VLD1_single_element_to_all_lanes,
//       safety: [size(7:6)=11 ||
//            (size(7:6)=00 &&
//            a(4)=1) => UNDEFINED,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadSingle1AllLanesTester_Case26_TestCase26) {
  VectorLoadSingle1AllLanesTester_Case26 tester;
  tester.Test("111101001d10nnnndddd1100sstammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1101
//    = {actual: 'VectorLoadSingle2AllLanes',
//       baseline: 'VectorLoadSingle2AllLanes',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d10nnnndddd1101sstammmm',
//       rule: 'VLD2_single_2_element_structure_to_all_lanes',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(5)=0
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1101
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       T: T(5),
//       Vd: Vd(15:12),
//       actual: VectorLoadSingle2AllLanes,
//       base: n,
//       baseline: VectorLoadSingle2AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22), Rn(19:16), Vd(15:12), size(7:6), T(5), Rm(3:0)],
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1101sstammmm,
//       rule: VLD2_single_2_element_structure_to_all_lanes,
//       safety: [size(7:6)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadSingle2AllLanesTester_Case27_TestCase27) {
  VectorLoadSingle2AllLanesTester_Case27 tester;
  tester.Test("111101001d10nnnndddd1101sstammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1110
//    = {actual: 'VectorLoadSingle3AllLanes',
//       baseline: 'VectorLoadSingle3AllLanes',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d10nnnndddd1110sstammmm',
//       rule: 'VLD3_single_3_element_structure_to_all_lanes',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(5)=0
//            else 2 + 1
//            if inst(5)=0
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 ||
//            inst(4)=1 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1110
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       actual: VectorLoadSingle3AllLanes,
//       base: n,
//       baseline: VectorLoadSingle3AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1110sstammmm,
//       rule: VLD3_single_3_element_structure_to_all_lanes,
//       safety: [size(7:6)=11 ||
//            a(4)=1 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadSingle3AllLanesTester_Case28_TestCase28) {
  VectorLoadSingle3AllLanesTester_Case28 tester;
  tester.Test("111101001d10nnnndddd1110sstammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=1111
//    = {actual: 'VectorLoadSingle4AllLanes',
//       baseline: 'VectorLoadSingle4AllLanes',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d10nnnndddd1111sstammmm',
//       rule: 'VLD4_single_4_element_structure_to_all_lanes',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(5)=0
//            else 2 + 1
//            if inst(5)=0
//            else 2 + 1
//            if inst(5)=0
//            else 2 => UNPREDICTABLE,
//         inst(7:6)=11 &&
//            inst(4)=0 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=1111
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       actual: VectorLoadSingle4AllLanes,
//       base: n,
//       baseline: VectorLoadSingle4AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1111sstammmm,
//       rule: VLD4_single_4_element_structure_to_all_lanes,
//       safety: [size(7:6)=11 &&
//            a(4)=0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadSingle4AllLanesTester_Case29_TestCase29) {
  VectorLoadSingle4AllLanesTester_Case29 tester;
  tester.Test("111101001d10nnnndddd1111sstammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=0x00
//    = {actual: 'VectorLoadStoreSingle1',
//       baseline: 'VectorLoadStoreSingle1',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d10nnnnddddss00aaaammmm',
//       rule: 'VLD1_single_element_to_one_lane',
//       safety: [15  ==
//               inst(19:16) => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 &&
//            inst(7:4)(1:0)=~11 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(2)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=0x00
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: VectorLoadStoreSingle1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss00aaaammmm,
//       rule: VLD1_single_element_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle1Tester_Case30_TestCase30) {
  VectorLoadStoreSingle1Tester_Case30 tester;
  tester.Test("111101001d10nnnnddddss00aaaammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=0x01
//    = {actual: 'VectorLoadStoreSingle2',
//       baseline: 'VectorLoadStoreSingle2',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d10nnnnddddss01aaaammmm',
//       rule: 'VLD2_single_2_element_structure_to_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1)=~0 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=0x01
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreSingle2,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss01aaaammmm,
//       rule: VLD2_single_2_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle2Tester_Case31_TestCase31) {
  VectorLoadStoreSingle2Tester_Case31 tester;
  tester.Test("111101001d10nnnnddddss01aaaammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=0x10
//    = {actual: 'VectorLoadStoreSingle3',
//       baseline: 'VectorLoadStoreSingle3',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d10nnnnddddss10aaaammmm',
//       rule: 'VLD3_single_3_element_structure_to_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=00 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=01 &&
//            inst(7:4)(0)=~0 => UNDEFINED,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=~00 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=0x10
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreSingle3,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss10aaaammmm,
//       rule: VLD3_single_3_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle3Tester_Case32_TestCase32) {
  VectorLoadStoreSingle3Tester_Case32 tester;
  tester.Test("111101001d10nnnnddddss10aaaammmm");
}

// Neutral case:
// inst(21)=1 & inst(23)=1 & inst(11:8)=0x11
//    = {actual: 'VectorLoadStoreSingle4',
//       baseline: 'VectorLoadStoreSingle4',
//       constraints: ,
//       defs: {inst(19:16)}
//            if (15  !=
//               inst(3:0))
//            else {},
//       pattern: '111101001d10nnnnddddss11aaaammmm',
//       rule: 'VLD4_single_4_element_structure_to_one_lane',
//       safety: [15  ==
//               inst(19:16) ||
//            31  <=
//               inst(22):inst(15:12) + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 + 1
//            if inst(11:10)=00
//            else (1
//            if inst(7:4)(1)=0
//            else 2)
//            if inst(11:10)=01
//            else (1
//            if inst(7:4)(2)=0
//            else 2)
//            if inst(11:10)=10
//            else 0 => UNPREDICTABLE,
//         inst(11:10)=10 &&
//            inst(7:4)(1:0)=11 => UNDEFINED,
//         inst(11:10)=11 => UNDEFINED]}
//
// Representaive case:
// L(21)=1 & A(23)=1 & B(11:8)=0x11
//    = {D: D(22),
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorLoadStoreSingle4,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss11aaaammmm,
//       rule: VLD4_single_4_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle4Tester_Case33_TestCase33) {
  VectorLoadStoreSingle4Tester_Case33 tester;
  tester.Test("111101001d10nnnnddddss11aaaammmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
