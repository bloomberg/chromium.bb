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

namespace nacl_arm_test {

// The following classes are derived class decoder testers that
// add row pattern constraints and decoder restrictions to each tester.
// This is done so that it can be used to make sure that the
// corresponding pattern is not tested for cases that would be excluded
//  due to row checks, or restrictions specified by the row restrictions.


// Neutral case:
// inst(23)=0 & inst(11:8)=0010
//    = {baseline: 'VectorStoreMultiple1',
//       constraints: ,
//       safety: ['inst(11:8)=0111 && inst(5:4)(1)=1 => UNDEFINED', 'inst(11:8)=1010 && inst(5:4)=11 => UNDEFINED', 'inst(11:8)=0110 && inst(5:4)(1)=1 => UNDEFINED', 'not inst(11:8)=0111 || inst(11:8)=1010 || inst(11:8)=0110 || inst(11:8)=0010 => DECODER_ERROR', '15 == inst(19:16) || 32 <= inst(22):inst(15:12) + 1 if inst(11:8)=0111 else 2 if inst(11:8)=1010 else 3 if inst(11:8)=0110 else 4 if inst(11:8)=0010 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=0 & B(11:8)=0010
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       baseline: VectorStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), align(5:4)],
//       n: Rn,
//       regs: 1 if type(11:8)=0111 else 2 if type(11:8)=1010 else 3 if type(11:8)=0110 else 4 if type(11:8)=0010 else 0,
//       safety: [type(11:8)=0111 && align(1)=1 => UNDEFINED, type(11:8)=1010 && align(5:4)=11 => UNDEFINED, type(11:8)=0110 && align(1)=1 => UNDEFINED, not type in bitset {'0111','1010','0110','0010'} => DECODER_ERROR, n == Pc || d + regs > 32 => UNPREDICTABLE],
//       type: type(11:8)}
class VectorStoreMultipleTesterCase0
    : public VectorStoreMultipleTester {
 public:
  VectorStoreMultipleTesterCase0(const NamedClassDecoder& decoder)
    : VectorStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorStoreMultipleTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00800000) != 0x00000000 /* A(23)=~0 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000200 /* B(11:8)=~0010 */) return false;

  // Check other preconditions defined for the base decoder.
  return VectorStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorStoreMultipleTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorStoreMultipleTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00) == 0x00000700) && ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002) == 0x00000002)) /* type(11:8)=0111 && align(1)=1 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00) == 0x00000A00) && ((inst.Bits() & 0x00000030) == 0x00000030)) /* type(11:8)=1010 && align(5:4)=11 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00) == 0x00000600) && ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002) == 0x00000002)) /* type(11:8)=0110 && align(1)=1 => UNDEFINED */);
  EXPECT_TRUE(((inst.Bits() & 0x00000F00) == 0x00000700) || ((inst.Bits() & 0x00000F00) == 0x00000A00) || ((inst.Bits() & 0x00000F00) == 0x00000600) || ((inst.Bits() & 0x00000F00) == 0x00000200) /* not type in bitset {'0111','1010','0110','0010'} => DECODER_ERROR */);
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00) == 0x00000700 ? 1 : ((inst.Bits() & 0x00000F00) == 0x00000A00 ? 2 : ((inst.Bits() & 0x00000F00) == 0x00000600 ? 3 : ((inst.Bits() & 0x00000F00) == 0x00000200 ? 4 : 0))))) > (32)))) /* n == Pc || d + regs > 32 => UNPREDICTABLE */);
  return VectorStoreMultipleTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(23)=0 & inst(11:8)=0011
//    = {baseline: 'VectorStoreMultiple2',
//       constraints: ,
//       safety: ['inst(7:6)=11 => UNDEFINED', 'inst(11:8)=1000 || inst(11:8)=1001 && inst(5:4)=11 => UNDEFINED', 'not inst(11:8)=1000 || inst(11:8)=1001 || inst(11:8)=0011 => DECODER_ERROR', '15 == inst(19:16) || 32 <= inst(22):inst(15:12) + 1 if inst(11:8)=1000 else 2 + 1 if inst(11:8)=1000 || inst(11:8)=1001 else 2 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=0 & B(11:8)=0011
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       baseline: VectorStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), size(7:6), align(5:4)],
//       inc: 1 if type(11:8)=1000 else 2,
//       n: Rn,
//       regs: 1 if type in bitset {'1000','1001'} else 2,
//       safety: [size(7:6)=11 => UNDEFINED, type in bitset {'1000','1001'} && align(5:4)=11 => UNDEFINED, not type in bitset {'1000','1001','0011'} => DECODER_ERROR, n == Pc || d2 + regs > 32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8)}
class VectorStoreMultipleTesterCase1
    : public VectorStoreMultipleTester {
 public:
  VectorStoreMultipleTesterCase1(const NamedClassDecoder& decoder)
    : VectorStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorStoreMultipleTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00800000) != 0x00000000 /* A(23)=~0 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000300 /* B(11:8)=~0011 */) return false;

  // Check other preconditions defined for the base decoder.
  return VectorStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorStoreMultipleTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorStoreMultipleTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((inst.Bits() & 0x000000C0) != 0x000000C0 /* size(7:6)=11 => UNDEFINED */);
  EXPECT_TRUE(!((((inst.Bits() & 0x00000F00) == 0x00000800) || ((inst.Bits() & 0x00000F00) == 0x00000900)) && ((inst.Bits() & 0x00000030) == 0x00000030)) /* type in bitset {'1000','1001'} && align(5:4)=11 => UNDEFINED */);
  EXPECT_TRUE(((inst.Bits() & 0x00000F00) == 0x00000800) || ((inst.Bits() & 0x00000F00) == 0x00000900) || ((inst.Bits() & 0x00000F00) == 0x00000300) /* not type in bitset {'1000','1001','0011'} => DECODER_ERROR */);
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00) == 0x00000800 ? 1 : 2) + (((inst.Bits() & 0x00000F00) == 0x00000800) || ((inst.Bits() & 0x00000F00) == 0x00000900) ? 1 : 2)) > (32)))) /* n == Pc || d2 + regs > 32 => UNPREDICTABLE */);
  return VectorStoreMultipleTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(23)=0 & inst(11:8)=1010
//    = {baseline: 'VectorStoreMultiple1',
//       constraints: ,
//       safety: ['inst(11:8)=0111 && inst(5:4)(1)=1 => UNDEFINED', 'inst(11:8)=1010 && inst(5:4)=11 => UNDEFINED', 'inst(11:8)=0110 && inst(5:4)(1)=1 => UNDEFINED', 'not inst(11:8)=0111 || inst(11:8)=1010 || inst(11:8)=0110 || inst(11:8)=0010 => DECODER_ERROR', '15 == inst(19:16) || 32 <= inst(22):inst(15:12) + 1 if inst(11:8)=0111 else 2 if inst(11:8)=1010 else 3 if inst(11:8)=0110 else 4 if inst(11:8)=0010 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=0 & B(11:8)=1010
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       baseline: VectorStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), align(5:4)],
//       n: Rn,
//       regs: 1 if type(11:8)=0111 else 2 if type(11:8)=1010 else 3 if type(11:8)=0110 else 4 if type(11:8)=0010 else 0,
//       safety: [type(11:8)=0111 && align(1)=1 => UNDEFINED, type(11:8)=1010 && align(5:4)=11 => UNDEFINED, type(11:8)=0110 && align(1)=1 => UNDEFINED, not type in bitset {'0111','1010','0110','0010'} => DECODER_ERROR, n == Pc || d + regs > 32 => UNPREDICTABLE],
//       type: type(11:8)}
class VectorStoreMultipleTesterCase2
    : public VectorStoreMultipleTester {
 public:
  VectorStoreMultipleTesterCase2(const NamedClassDecoder& decoder)
    : VectorStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorStoreMultipleTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00800000) != 0x00000000 /* A(23)=~0 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000A00 /* B(11:8)=~1010 */) return false;

  // Check other preconditions defined for the base decoder.
  return VectorStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorStoreMultipleTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorStoreMultipleTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00) == 0x00000700) && ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002) == 0x00000002)) /* type(11:8)=0111 && align(1)=1 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00) == 0x00000A00) && ((inst.Bits() & 0x00000030) == 0x00000030)) /* type(11:8)=1010 && align(5:4)=11 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00) == 0x00000600) && ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002) == 0x00000002)) /* type(11:8)=0110 && align(1)=1 => UNDEFINED */);
  EXPECT_TRUE(((inst.Bits() & 0x00000F00) == 0x00000700) || ((inst.Bits() & 0x00000F00) == 0x00000A00) || ((inst.Bits() & 0x00000F00) == 0x00000600) || ((inst.Bits() & 0x00000F00) == 0x00000200) /* not type in bitset {'0111','1010','0110','0010'} => DECODER_ERROR */);
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00) == 0x00000700 ? 1 : ((inst.Bits() & 0x00000F00) == 0x00000A00 ? 2 : ((inst.Bits() & 0x00000F00) == 0x00000600 ? 3 : ((inst.Bits() & 0x00000F00) == 0x00000200 ? 4 : 0))))) > (32)))) /* n == Pc || d + regs > 32 => UNPREDICTABLE */);
  return VectorStoreMultipleTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(23)=0 & inst(11:8)=000x
//    = {baseline: 'VectorStoreMultiple4',
//       constraints: ,
//       safety: ['inst(7:6)=11 => UNDEFINED', 'not inst(11:8)=0000 || inst(11:8)=0001 => DECODER_ERROR', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:8)=0000 else 2 + 1 if inst(11:8)=0000 else 2 + 1 if inst(11:8)=0000 else 2 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=0 & B(11:8)=000x
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       baseline: VectorStoreMultiple4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), size(7:6)],
//       inc: 1 if type(11:8)=0000 else 2,
//       n: Rn,
//       safety: [size(7:6)=11 => UNDEFINED, not type in bitset {'0000','0001'} => DECODER_ERROR, n == Pc || d4 > 31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8)}
class VectorStoreMultipleTesterCase3
    : public VectorStoreMultipleTester {
 public:
  VectorStoreMultipleTesterCase3(const NamedClassDecoder& decoder)
    : VectorStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorStoreMultipleTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00800000) != 0x00000000 /* A(23)=~0 */) return false;
  if ((inst.Bits() & 0x00000E00) != 0x00000000 /* B(11:8)=~000x */) return false;

  // Check other preconditions defined for the base decoder.
  return VectorStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorStoreMultipleTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorStoreMultipleTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((inst.Bits() & 0x000000C0) != 0x000000C0 /* size(7:6)=11 => UNDEFINED */);
  EXPECT_TRUE(((inst.Bits() & 0x00000F00) == 0x00000000) || ((inst.Bits() & 0x00000F00) == 0x00000100) /* not type in bitset {'0000','0001'} => DECODER_ERROR */);
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00) == 0x00000000 ? 1 : 2) + ((inst.Bits() & 0x00000F00) == 0x00000000 ? 1 : 2) + ((inst.Bits() & 0x00000F00) == 0x00000000 ? 1 : 2)) > (31)))) /* n == Pc || d4 > 31 => UNPREDICTABLE */);
  return VectorStoreMultipleTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(23)=0 & inst(11:8)=010x
//    = {baseline: 'VectorStoreMultiple3',
//       constraints: ,
//       safety: ['inst(7:6)=11 || inst(5:4)(1)=1 => UNDEFINED', 'not inst(11:8)=0100 || inst(11:8)=0101 => DECODER_ERROR', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:8)=0100 else 2 + 1 if inst(11:8)=0100 else 2 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=0 & B(11:8)=010x
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       baseline: VectorStoreMultiple3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), size(7:6), align(5:4)],
//       inc: 1 if type(11:8)=0100 else 2,
//       n: Rn,
//       safety: [size(7:6)=11 || align(1)=1 => UNDEFINED, not type in bitset {'0100','0101'} => DECODER_ERROR, n == Pc || d3 > 31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8)}
class VectorStoreMultipleTesterCase4
    : public VectorStoreMultipleTester {
 public:
  VectorStoreMultipleTesterCase4(const NamedClassDecoder& decoder)
    : VectorStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorStoreMultipleTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00800000) != 0x00000000 /* A(23)=~0 */) return false;
  if ((inst.Bits() & 0x00000E00) != 0x00000400 /* B(11:8)=~010x */) return false;

  // Check other preconditions defined for the base decoder.
  return VectorStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorStoreMultipleTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorStoreMultipleTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!(((inst.Bits() & 0x000000C0) == 0x000000C0) || ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002) == 0x00000002)) /* size(7:6)=11 || align(1)=1 => UNDEFINED */);
  EXPECT_TRUE(((inst.Bits() & 0x00000F00) == 0x00000400) || ((inst.Bits() & 0x00000F00) == 0x00000500) /* not type in bitset {'0100','0101'} => DECODER_ERROR */);
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00) == 0x00000400 ? 1 : 2) + ((inst.Bits() & 0x00000F00) == 0x00000400 ? 1 : 2)) > (31)))) /* n == Pc || d3 > 31 => UNPREDICTABLE */);
  return VectorStoreMultipleTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(23)=0 & inst(11:8)=011x
//    = {baseline: 'VectorStoreMultiple1',
//       constraints: ,
//       safety: ['inst(11:8)=0111 && inst(5:4)(1)=1 => UNDEFINED', 'inst(11:8)=1010 && inst(5:4)=11 => UNDEFINED', 'inst(11:8)=0110 && inst(5:4)(1)=1 => UNDEFINED', 'not inst(11:8)=0111 || inst(11:8)=1010 || inst(11:8)=0110 || inst(11:8)=0010 => DECODER_ERROR', '15 == inst(19:16) || 32 <= inst(22):inst(15:12) + 1 if inst(11:8)=0111 else 2 if inst(11:8)=1010 else 3 if inst(11:8)=0110 else 4 if inst(11:8)=0010 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=0 & B(11:8)=011x
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       baseline: VectorStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), align(5:4)],
//       n: Rn,
//       regs: 1 if type(11:8)=0111 else 2 if type(11:8)=1010 else 3 if type(11:8)=0110 else 4 if type(11:8)=0010 else 0,
//       safety: [type(11:8)=0111 && align(1)=1 => UNDEFINED, type(11:8)=1010 && align(5:4)=11 => UNDEFINED, type(11:8)=0110 && align(1)=1 => UNDEFINED, not type in bitset {'0111','1010','0110','0010'} => DECODER_ERROR, n == Pc || d + regs > 32 => UNPREDICTABLE],
//       type: type(11:8)}
class VectorStoreMultipleTesterCase5
    : public VectorStoreMultipleTester {
 public:
  VectorStoreMultipleTesterCase5(const NamedClassDecoder& decoder)
    : VectorStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorStoreMultipleTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00800000) != 0x00000000 /* A(23)=~0 */) return false;
  if ((inst.Bits() & 0x00000E00) != 0x00000600 /* B(11:8)=~011x */) return false;

  // Check other preconditions defined for the base decoder.
  return VectorStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorStoreMultipleTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorStoreMultipleTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00) == 0x00000700) && ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002) == 0x00000002)) /* type(11:8)=0111 && align(1)=1 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00) == 0x00000A00) && ((inst.Bits() & 0x00000030) == 0x00000030)) /* type(11:8)=1010 && align(5:4)=11 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000F00) == 0x00000600) && ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002) == 0x00000002)) /* type(11:8)=0110 && align(1)=1 => UNDEFINED */);
  EXPECT_TRUE(((inst.Bits() & 0x00000F00) == 0x00000700) || ((inst.Bits() & 0x00000F00) == 0x00000A00) || ((inst.Bits() & 0x00000F00) == 0x00000600) || ((inst.Bits() & 0x00000F00) == 0x00000200) /* not type in bitset {'0111','1010','0110','0010'} => DECODER_ERROR */);
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00) == 0x00000700 ? 1 : ((inst.Bits() & 0x00000F00) == 0x00000A00 ? 2 : ((inst.Bits() & 0x00000F00) == 0x00000600 ? 3 : ((inst.Bits() & 0x00000F00) == 0x00000200 ? 4 : 0))))) > (32)))) /* n == Pc || d + regs > 32 => UNPREDICTABLE */);
  return VectorStoreMultipleTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(23)=0 & inst(11:8)=100x
//    = {baseline: 'VectorStoreMultiple2',
//       constraints: ,
//       safety: ['inst(7:6)=11 => UNDEFINED', 'inst(11:8)=1000 || inst(11:8)=1001 && inst(5:4)=11 => UNDEFINED', 'not inst(11:8)=1000 || inst(11:8)=1001 || inst(11:8)=0011 => DECODER_ERROR', '15 == inst(19:16) || 32 <= inst(22):inst(15:12) + 1 if inst(11:8)=1000 else 2 + 1 if inst(11:8)=1000 || inst(11:8)=1001 else 2 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=0 & B(11:8)=100x
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       baseline: VectorStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), size(7:6), align(5:4)],
//       inc: 1 if type(11:8)=1000 else 2,
//       n: Rn,
//       regs: 1 if type in bitset {'1000','1001'} else 2,
//       safety: [size(7:6)=11 => UNDEFINED, type in bitset {'1000','1001'} && align(5:4)=11 => UNDEFINED, not type in bitset {'1000','1001','0011'} => DECODER_ERROR, n == Pc || d2 + regs > 32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8)}
class VectorStoreMultipleTesterCase6
    : public VectorStoreMultipleTester {
 public:
  VectorStoreMultipleTesterCase6(const NamedClassDecoder& decoder)
    : VectorStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorStoreMultipleTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00800000) != 0x00000000 /* A(23)=~0 */) return false;
  if ((inst.Bits() & 0x00000E00) != 0x00000800 /* B(11:8)=~100x */) return false;

  // Check other preconditions defined for the base decoder.
  return VectorStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorStoreMultipleTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorStoreMultipleTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((inst.Bits() & 0x000000C0) != 0x000000C0 /* size(7:6)=11 => UNDEFINED */);
  EXPECT_TRUE(!((((inst.Bits() & 0x00000F00) == 0x00000800) || ((inst.Bits() & 0x00000F00) == 0x00000900)) && ((inst.Bits() & 0x00000030) == 0x00000030)) /* type in bitset {'1000','1001'} && align(5:4)=11 => UNDEFINED */);
  EXPECT_TRUE(((inst.Bits() & 0x00000F00) == 0x00000800) || ((inst.Bits() & 0x00000F00) == 0x00000900) || ((inst.Bits() & 0x00000F00) == 0x00000300) /* not type in bitset {'1000','1001','0011'} => DECODER_ERROR */);
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00) == 0x00000800 ? 1 : 2) + (((inst.Bits() & 0x00000F00) == 0x00000800) || ((inst.Bits() & 0x00000F00) == 0x00000900) ? 1 : 2)) > (32)))) /* n == Pc || d2 + regs > 32 => UNPREDICTABLE */);
  return VectorStoreMultipleTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(23)=1 & inst(11:8)=1000
//    = {baseline: 'VectorStoreSingle1',
//       constraints: ,
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=00 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=01 && inst(7:4)(1)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(2)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=~00 && inst(7:4)(1:0)=~11 => UNDEFINED', '15 == inst(19:16) => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=1000
//    = {Rn: Rn(19:16),
//       baseline: VectorStoreSingle1,
//       constraints: ,
//       fields: [Rn(19:16), size(11:10), index_align(7:4)],
//       index_align: index_align(7:4),
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=00 && index_align(0)=~0 => UNDEFINED, size(11:10)=01 && index_align(1)=~0 => UNDEFINED, size(11:10)=10 && index_align(2)=~0 => UNDEFINED, size(11:10)=10 && index_align(1:0)=~00 && index_align(1:0)=~11 => UNDEFINED, n == Pc => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingleTesterCase7
    : public VectorStoreSingleTester {
 public:
  VectorStoreSingleTesterCase7(const NamedClassDecoder& decoder)
    : VectorStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorStoreSingleTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00800000) != 0x00800000 /* A(23)=~1 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000800 /* B(11:8)=~1000 */) return false;

  // Check other preconditions defined for the base decoder.
  return VectorStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorStoreSingleTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorStoreSingleTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((inst.Bits() & 0x00000C00) != 0x00000C00 /* size(11:10)=11 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000000) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001) != 0x00000000)) /* size(11:10)=00 && index_align(0)=~0 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000400) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) != 0x00000000)) /* size(11:10)=01 && index_align(1)=~0 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000800) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004) != 0x00000000)) /* size(11:10)=10 && index_align(2)=~0 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000800) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003) != 0x00000000) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003) != 0x00000003)) /* size(11:10)=10 && index_align(1:0)=~00 && index_align(1:0)=~11 => UNDEFINED */);
  EXPECT_TRUE(((((inst.Bits() & 0x000F0000) >> 16)) != (15)) /* n == Pc => UNPREDICTABLE */);
  return VectorStoreSingleTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(23)=1 & inst(11:8)=1001
//    = {baseline: 'VectorStoreSingle2',
//       constraints: ,
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1)=~0 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=1001
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       baseline: VectorStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=10 && index_align(1)=~0 => UNDEFINED, n == Pc || d2 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingleTesterCase8
    : public VectorStoreSingleTester {
 public:
  VectorStoreSingleTesterCase8(const NamedClassDecoder& decoder)
    : VectorStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorStoreSingleTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00800000) != 0x00800000 /* A(23)=~1 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000900 /* B(11:8)=~1001 */) return false;

  // Check other preconditions defined for the base decoder.
  return VectorStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorStoreSingleTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorStoreSingleTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((inst.Bits() & 0x00000C00) != 0x00000C00 /* size(11:10)=11 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000800) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) != 0x00000000)) /* size(11:10)=10 && index_align(1)=~0 => UNDEFINED */);
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00) == 0x00000000 ? 1 : ((inst.Bits() & 0x00000C00) == 0x00000400 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) == 0x00000000 ? 1 : 2)) : ((inst.Bits() & 0x00000C00) == 0x00000800 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004) == 0x00000000 ? 1 : 2)) : 0)))) > (31)))) /* n == Pc || d2 > 31 => UNPREDICTABLE */);
  return VectorStoreSingleTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(23)=1 & inst(11:8)=1010
//    = {baseline: 'VectorStoreSingle3',
//       constraints: ,
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=00 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=01 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=~00 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=1010
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       baseline: VectorStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=00 && index_align(0)=~0 => UNDEFINED, size(11:10)=01 && index_align(0)=~0 => UNDEFINED, size(11:10)=10 && index_align(1:0)=~00 => UNDEFINED, n == Pc || d3 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingleTesterCase9
    : public VectorStoreSingleTester {
 public:
  VectorStoreSingleTesterCase9(const NamedClassDecoder& decoder)
    : VectorStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorStoreSingleTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00800000) != 0x00800000 /* A(23)=~1 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000A00 /* B(11:8)=~1010 */) return false;

  // Check other preconditions defined for the base decoder.
  return VectorStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorStoreSingleTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorStoreSingleTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((inst.Bits() & 0x00000C00) != 0x00000C00 /* size(11:10)=11 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000000) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001) != 0x00000000)) /* size(11:10)=00 && index_align(0)=~0 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000400) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001) != 0x00000000)) /* size(11:10)=01 && index_align(0)=~0 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000800) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003) != 0x00000000)) /* size(11:10)=10 && index_align(1:0)=~00 => UNDEFINED */);
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00) == 0x00000000 ? 1 : ((inst.Bits() & 0x00000C00) == 0x00000400 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) == 0x00000000 ? 1 : 2)) : ((inst.Bits() & 0x00000C00) == 0x00000800 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004) == 0x00000000 ? 1 : 2)) : 0))) + ((inst.Bits() & 0x00000C00) == 0x00000000 ? 1 : ((inst.Bits() & 0x00000C00) == 0x00000400 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) == 0x00000000 ? 1 : 2)) : ((inst.Bits() & 0x00000C00) == 0x00000800 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004) == 0x00000000 ? 1 : 2)) : 0)))) > (31)))) /* n == Pc || d3 > 31 => UNPREDICTABLE */);
  return VectorStoreSingleTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(23)=1 & inst(11:8)=1011
//    = {baseline: 'VectorStoreSingle4',
//       constraints: ,
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=11 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=1011
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       baseline: VectorStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=10 && index_align(1:0)=11 => UNDEFINED, n == Pc || d4 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingleTesterCase10
    : public VectorStoreSingleTester {
 public:
  VectorStoreSingleTesterCase10(const NamedClassDecoder& decoder)
    : VectorStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorStoreSingleTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00800000) != 0x00800000 /* A(23)=~1 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000B00 /* B(11:8)=~1011 */) return false;

  // Check other preconditions defined for the base decoder.
  return VectorStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorStoreSingleTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorStoreSingleTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((inst.Bits() & 0x00000C00) != 0x00000C00 /* size(11:10)=11 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000800) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003) == 0x00000003)) /* size(11:10)=10 && index_align(1:0)=11 => UNDEFINED */);
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00) == 0x00000000 ? 1 : ((inst.Bits() & 0x00000C00) == 0x00000400 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) == 0x00000000 ? 1 : 2)) : ((inst.Bits() & 0x00000C00) == 0x00000800 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004) == 0x00000000 ? 1 : 2)) : 0))) + ((inst.Bits() & 0x00000C00) == 0x00000000 ? 1 : ((inst.Bits() & 0x00000C00) == 0x00000400 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) == 0x00000000 ? 1 : 2)) : ((inst.Bits() & 0x00000C00) == 0x00000800 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004) == 0x00000000 ? 1 : 2)) : 0))) + ((inst.Bits() & 0x00000C00) == 0x00000000 ? 1 : ((inst.Bits() & 0x00000C00) == 0x00000400 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) == 0x00000000 ? 1 : 2)) : ((inst.Bits() & 0x00000C00) == 0x00000800 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004) == 0x00000000 ? 1 : 2)) : 0)))) > (31)))) /* n == Pc || d4 > 31 => UNPREDICTABLE */);
  return VectorStoreSingleTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(23)=1 & inst(11:8)=0x00
//    = {baseline: 'VectorStoreSingle1',
//       constraints: ,
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=00 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=01 && inst(7:4)(1)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(2)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=~00 && inst(7:4)(1:0)=~11 => UNDEFINED', '15 == inst(19:16) => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=0x00
//    = {Rn: Rn(19:16),
//       baseline: VectorStoreSingle1,
//       constraints: ,
//       fields: [Rn(19:16), size(11:10), index_align(7:4)],
//       index_align: index_align(7:4),
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=00 && index_align(0)=~0 => UNDEFINED, size(11:10)=01 && index_align(1)=~0 => UNDEFINED, size(11:10)=10 && index_align(2)=~0 => UNDEFINED, size(11:10)=10 && index_align(1:0)=~00 && index_align(1:0)=~11 => UNDEFINED, n == Pc => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingleTesterCase11
    : public VectorStoreSingleTester {
 public:
  VectorStoreSingleTesterCase11(const NamedClassDecoder& decoder)
    : VectorStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorStoreSingleTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00800000) != 0x00800000 /* A(23)=~1 */) return false;
  if ((inst.Bits() & 0x00000B00) != 0x00000000 /* B(11:8)=~0x00 */) return false;

  // Check other preconditions defined for the base decoder.
  return VectorStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorStoreSingleTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorStoreSingleTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((inst.Bits() & 0x00000C00) != 0x00000C00 /* size(11:10)=11 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000000) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001) != 0x00000000)) /* size(11:10)=00 && index_align(0)=~0 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000400) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) != 0x00000000)) /* size(11:10)=01 && index_align(1)=~0 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000800) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004) != 0x00000000)) /* size(11:10)=10 && index_align(2)=~0 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000800) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003) != 0x00000000) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003) != 0x00000003)) /* size(11:10)=10 && index_align(1:0)=~00 && index_align(1:0)=~11 => UNDEFINED */);
  EXPECT_TRUE(((((inst.Bits() & 0x000F0000) >> 16)) != (15)) /* n == Pc => UNPREDICTABLE */);
  return VectorStoreSingleTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(23)=1 & inst(11:8)=0x01
//    = {baseline: 'VectorStoreSingle2',
//       constraints: ,
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1)=~0 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=0x01
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       baseline: VectorStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=10 && index_align(1)=~0 => UNDEFINED, n == Pc || d2 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingleTesterCase12
    : public VectorStoreSingleTester {
 public:
  VectorStoreSingleTesterCase12(const NamedClassDecoder& decoder)
    : VectorStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorStoreSingleTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00800000) != 0x00800000 /* A(23)=~1 */) return false;
  if ((inst.Bits() & 0x00000B00) != 0x00000100 /* B(11:8)=~0x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return VectorStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorStoreSingleTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorStoreSingleTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((inst.Bits() & 0x00000C00) != 0x00000C00 /* size(11:10)=11 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000800) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) != 0x00000000)) /* size(11:10)=10 && index_align(1)=~0 => UNDEFINED */);
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00) == 0x00000000 ? 1 : ((inst.Bits() & 0x00000C00) == 0x00000400 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) == 0x00000000 ? 1 : 2)) : ((inst.Bits() & 0x00000C00) == 0x00000800 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004) == 0x00000000 ? 1 : 2)) : 0)))) > (31)))) /* n == Pc || d2 > 31 => UNPREDICTABLE */);
  return VectorStoreSingleTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(23)=1 & inst(11:8)=0x10
//    = {baseline: 'VectorStoreSingle3',
//       constraints: ,
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=00 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=01 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=~00 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=0x10
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       baseline: VectorStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=00 && index_align(0)=~0 => UNDEFINED, size(11:10)=01 && index_align(0)=~0 => UNDEFINED, size(11:10)=10 && index_align(1:0)=~00 => UNDEFINED, n == Pc || d3 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingleTesterCase13
    : public VectorStoreSingleTester {
 public:
  VectorStoreSingleTesterCase13(const NamedClassDecoder& decoder)
    : VectorStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorStoreSingleTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00800000) != 0x00800000 /* A(23)=~1 */) return false;
  if ((inst.Bits() & 0x00000B00) != 0x00000200 /* B(11:8)=~0x10 */) return false;

  // Check other preconditions defined for the base decoder.
  return VectorStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorStoreSingleTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorStoreSingleTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((inst.Bits() & 0x00000C00) != 0x00000C00 /* size(11:10)=11 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000000) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001) != 0x00000000)) /* size(11:10)=00 && index_align(0)=~0 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000400) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001) != 0x00000000)) /* size(11:10)=01 && index_align(0)=~0 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000800) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003) != 0x00000000)) /* size(11:10)=10 && index_align(1:0)=~00 => UNDEFINED */);
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00) == 0x00000000 ? 1 : ((inst.Bits() & 0x00000C00) == 0x00000400 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) == 0x00000000 ? 1 : 2)) : ((inst.Bits() & 0x00000C00) == 0x00000800 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004) == 0x00000000 ? 1 : 2)) : 0))) + ((inst.Bits() & 0x00000C00) == 0x00000000 ? 1 : ((inst.Bits() & 0x00000C00) == 0x00000400 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) == 0x00000000 ? 1 : 2)) : ((inst.Bits() & 0x00000C00) == 0x00000800 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004) == 0x00000000 ? 1 : 2)) : 0)))) > (31)))) /* n == Pc || d3 > 31 => UNPREDICTABLE */);
  return VectorStoreSingleTester::
    ApplySanityChecks(inst, decoder);
}

// Neutral case:
// inst(23)=1 & inst(11:8)=0x11
//    = {baseline: 'VectorStoreSingle4',
//       constraints: ,
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=11 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=0x11
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       baseline: VectorStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=10 && index_align(1:0)=11 => UNDEFINED, n == Pc || d4 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingleTesterCase14
    : public VectorStoreSingleTester {
 public:
  VectorStoreSingleTesterCase14(const NamedClassDecoder& decoder)
    : VectorStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorStoreSingleTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00800000) != 0x00800000 /* A(23)=~1 */) return false;
  if ((inst.Bits() & 0x00000B00) != 0x00000300 /* B(11:8)=~0x11 */) return false;

  // Check other preconditions defined for the base decoder.
  return VectorStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorStoreSingleTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorStoreSingleTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((inst.Bits() & 0x00000C00) != 0x00000C00 /* size(11:10)=11 => UNDEFINED */);
  EXPECT_TRUE(!(((inst.Bits() & 0x00000C00) == 0x00000800) && ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003) == 0x00000003)) /* size(11:10)=10 && index_align(1:0)=11 => UNDEFINED */);
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) || ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00) == 0x00000000 ? 1 : ((inst.Bits() & 0x00000C00) == 0x00000400 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) == 0x00000000 ? 1 : 2)) : ((inst.Bits() & 0x00000C00) == 0x00000800 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004) == 0x00000000 ? 1 : 2)) : 0))) + ((inst.Bits() & 0x00000C00) == 0x00000000 ? 1 : ((inst.Bits() & 0x00000C00) == 0x00000400 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) == 0x00000000 ? 1 : 2)) : ((inst.Bits() & 0x00000C00) == 0x00000800 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004) == 0x00000000 ? 1 : 2)) : 0))) + ((inst.Bits() & 0x00000C00) == 0x00000000 ? 1 : ((inst.Bits() & 0x00000C00) == 0x00000400 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002) == 0x00000000 ? 1 : 2)) : ((inst.Bits() & 0x00000C00) == 0x00000800 ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004) == 0x00000000 ? 1 : 2)) : 0)))) > (31)))) /* n == Pc || d4 > 31 => UNPREDICTABLE */);
  return VectorStoreSingleTester::
    ApplySanityChecks(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(23)=0 & inst(11:8)=0010
//    = {baseline: 'VectorStoreMultiple1',
//       constraints: ,
//       rule: 'VST1_multiple_single_elements',
//       safety: ['inst(11:8)=0111 && inst(5:4)(1)=1 => UNDEFINED', 'inst(11:8)=1010 && inst(5:4)=11 => UNDEFINED', 'inst(11:8)=0110 && inst(5:4)(1)=1 => UNDEFINED', 'not inst(11:8)=0111 || inst(11:8)=1010 || inst(11:8)=0110 || inst(11:8)=0010 => DECODER_ERROR', '15 == inst(19:16) || 32 <= inst(22):inst(15:12) + 1 if inst(11:8)=0111 else 2 if inst(11:8)=1010 else 3 if inst(11:8)=0110 else 4 if inst(11:8)=0010 else 0 => UNPREDICTABLE']}
//
// Representative case:
// A(23)=0 & B(11:8)=0010
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       baseline: VectorStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), align(5:4)],
//       n: Rn,
//       regs: 1 if type(11:8)=0111 else 2 if type(11:8)=1010 else 3 if type(11:8)=0110 else 4 if type(11:8)=0010 else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 && align(1)=1 => UNDEFINED, type(11:8)=1010 && align(5:4)=11 => UNDEFINED, type(11:8)=0110 && align(1)=1 => UNDEFINED, not type in bitset {'0111','1010','0110','0010'} => DECODER_ERROR, n == Pc || d + regs > 32 => UNPREDICTABLE],
//       type: type(11:8)}
class VectorStoreMultiple1Tester_Case0
    : public VectorStoreMultipleTesterCase0 {
 public:
  VectorStoreMultiple1Tester_Case0()
    : VectorStoreMultipleTesterCase0(
      state_.VectorStoreMultiple1_VST1_multiple_single_elements_instance_)
  {}
};

// Neutral case:
// inst(23)=0 & inst(11:8)=0011
//    = {baseline: 'VectorStoreMultiple2',
//       constraints: ,
//       rule: 'VST2_multiple_2_element_structures',
//       safety: ['inst(7:6)=11 => UNDEFINED', 'inst(11:8)=1000 || inst(11:8)=1001 && inst(5:4)=11 => UNDEFINED', 'not inst(11:8)=1000 || inst(11:8)=1001 || inst(11:8)=0011 => DECODER_ERROR', '15 == inst(19:16) || 32 <= inst(22):inst(15:12) + 1 if inst(11:8)=1000 else 2 + 1 if inst(11:8)=1000 || inst(11:8)=1001 else 2 => UNPREDICTABLE']}
//
// Representative case:
// A(23)=0 & B(11:8)=0011
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       baseline: VectorStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), size(7:6), align(5:4)],
//       inc: 1 if type(11:8)=1000 else 2,
//       n: Rn,
//       regs: 1 if type in bitset {'1000','1001'} else 2,
//       rule: VST2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED, type in bitset {'1000','1001'} && align(5:4)=11 => UNDEFINED, not type in bitset {'1000','1001','0011'} => DECODER_ERROR, n == Pc || d2 + regs > 32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8)}
class VectorStoreMultiple2Tester_Case1
    : public VectorStoreMultipleTesterCase1 {
 public:
  VectorStoreMultiple2Tester_Case1()
    : VectorStoreMultipleTesterCase1(
      state_.VectorStoreMultiple2_VST2_multiple_2_element_structures_instance_)
  {}
};

// Neutral case:
// inst(23)=0 & inst(11:8)=1010
//    = {baseline: 'VectorStoreMultiple1',
//       constraints: ,
//       rule: 'VST1_multiple_single_elements',
//       safety: ['inst(11:8)=0111 && inst(5:4)(1)=1 => UNDEFINED', 'inst(11:8)=1010 && inst(5:4)=11 => UNDEFINED', 'inst(11:8)=0110 && inst(5:4)(1)=1 => UNDEFINED', 'not inst(11:8)=0111 || inst(11:8)=1010 || inst(11:8)=0110 || inst(11:8)=0010 => DECODER_ERROR', '15 == inst(19:16) || 32 <= inst(22):inst(15:12) + 1 if inst(11:8)=0111 else 2 if inst(11:8)=1010 else 3 if inst(11:8)=0110 else 4 if inst(11:8)=0010 else 0 => UNPREDICTABLE']}
//
// Representative case:
// A(23)=0 & B(11:8)=1010
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       baseline: VectorStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), align(5:4)],
//       n: Rn,
//       regs: 1 if type(11:8)=0111 else 2 if type(11:8)=1010 else 3 if type(11:8)=0110 else 4 if type(11:8)=0010 else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 && align(1)=1 => UNDEFINED, type(11:8)=1010 && align(5:4)=11 => UNDEFINED, type(11:8)=0110 && align(1)=1 => UNDEFINED, not type in bitset {'0111','1010','0110','0010'} => DECODER_ERROR, n == Pc || d + regs > 32 => UNPREDICTABLE],
//       type: type(11:8)}
class VectorStoreMultiple1Tester_Case2
    : public VectorStoreMultipleTesterCase2 {
 public:
  VectorStoreMultiple1Tester_Case2()
    : VectorStoreMultipleTesterCase2(
      state_.VectorStoreMultiple1_VST1_multiple_single_elements_instance_)
  {}
};

// Neutral case:
// inst(23)=0 & inst(11:8)=000x
//    = {baseline: 'VectorStoreMultiple4',
//       constraints: ,
//       rule: 'VST4_multiple_4_element_structures',
//       safety: ['inst(7:6)=11 => UNDEFINED', 'not inst(11:8)=0000 || inst(11:8)=0001 => DECODER_ERROR', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:8)=0000 else 2 + 1 if inst(11:8)=0000 else 2 + 1 if inst(11:8)=0000 else 2 => UNPREDICTABLE']}
//
// Representative case:
// A(23)=0 & B(11:8)=000x
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       baseline: VectorStoreMultiple4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), size(7:6)],
//       inc: 1 if type(11:8)=0000 else 2,
//       n: Rn,
//       rule: VST4_multiple_4_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED, not type in bitset {'0000','0001'} => DECODER_ERROR, n == Pc || d4 > 31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8)}
class VectorStoreMultiple4Tester_Case3
    : public VectorStoreMultipleTesterCase3 {
 public:
  VectorStoreMultiple4Tester_Case3()
    : VectorStoreMultipleTesterCase3(
      state_.VectorStoreMultiple4_VST4_multiple_4_element_structures_instance_)
  {}
};

// Neutral case:
// inst(23)=0 & inst(11:8)=010x
//    = {baseline: 'VectorStoreMultiple3',
//       constraints: ,
//       rule: 'VST3_multiple_3_element_structures',
//       safety: ['inst(7:6)=11 || inst(5:4)(1)=1 => UNDEFINED', 'not inst(11:8)=0100 || inst(11:8)=0101 => DECODER_ERROR', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:8)=0100 else 2 + 1 if inst(11:8)=0100 else 2 => UNPREDICTABLE']}
//
// Representative case:
// A(23)=0 & B(11:8)=010x
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       baseline: VectorStoreMultiple3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), size(7:6), align(5:4)],
//       inc: 1 if type(11:8)=0100 else 2,
//       n: Rn,
//       rule: VST3_multiple_3_element_structures,
//       safety: [size(7:6)=11 || align(1)=1 => UNDEFINED, not type in bitset {'0100','0101'} => DECODER_ERROR, n == Pc || d3 > 31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8)}
class VectorStoreMultiple3Tester_Case4
    : public VectorStoreMultipleTesterCase4 {
 public:
  VectorStoreMultiple3Tester_Case4()
    : VectorStoreMultipleTesterCase4(
      state_.VectorStoreMultiple3_VST3_multiple_3_element_structures_instance_)
  {}
};

// Neutral case:
// inst(23)=0 & inst(11:8)=011x
//    = {baseline: 'VectorStoreMultiple1',
//       constraints: ,
//       rule: 'VST1_multiple_single_elements',
//       safety: ['inst(11:8)=0111 && inst(5:4)(1)=1 => UNDEFINED', 'inst(11:8)=1010 && inst(5:4)=11 => UNDEFINED', 'inst(11:8)=0110 && inst(5:4)(1)=1 => UNDEFINED', 'not inst(11:8)=0111 || inst(11:8)=1010 || inst(11:8)=0110 || inst(11:8)=0010 => DECODER_ERROR', '15 == inst(19:16) || 32 <= inst(22):inst(15:12) + 1 if inst(11:8)=0111 else 2 if inst(11:8)=1010 else 3 if inst(11:8)=0110 else 4 if inst(11:8)=0010 else 0 => UNPREDICTABLE']}
//
// Representative case:
// A(23)=0 & B(11:8)=011x
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       baseline: VectorStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), align(5:4)],
//       n: Rn,
//       regs: 1 if type(11:8)=0111 else 2 if type(11:8)=1010 else 3 if type(11:8)=0110 else 4 if type(11:8)=0010 else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 && align(1)=1 => UNDEFINED, type(11:8)=1010 && align(5:4)=11 => UNDEFINED, type(11:8)=0110 && align(1)=1 => UNDEFINED, not type in bitset {'0111','1010','0110','0010'} => DECODER_ERROR, n == Pc || d + regs > 32 => UNPREDICTABLE],
//       type: type(11:8)}
class VectorStoreMultiple1Tester_Case5
    : public VectorStoreMultipleTesterCase5 {
 public:
  VectorStoreMultiple1Tester_Case5()
    : VectorStoreMultipleTesterCase5(
      state_.VectorStoreMultiple1_VST1_multiple_single_elements_instance_)
  {}
};

// Neutral case:
// inst(23)=0 & inst(11:8)=100x
//    = {baseline: 'VectorStoreMultiple2',
//       constraints: ,
//       rule: 'VST2_multiple_2_element_structures',
//       safety: ['inst(7:6)=11 => UNDEFINED', 'inst(11:8)=1000 || inst(11:8)=1001 && inst(5:4)=11 => UNDEFINED', 'not inst(11:8)=1000 || inst(11:8)=1001 || inst(11:8)=0011 => DECODER_ERROR', '15 == inst(19:16) || 32 <= inst(22):inst(15:12) + 1 if inst(11:8)=1000 else 2 + 1 if inst(11:8)=1000 || inst(11:8)=1001 else 2 => UNPREDICTABLE']}
//
// Representative case:
// A(23)=0 & B(11:8)=100x
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       align: align(5:4),
//       baseline: VectorStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), size(7:6), align(5:4)],
//       inc: 1 if type(11:8)=1000 else 2,
//       n: Rn,
//       regs: 1 if type in bitset {'1000','1001'} else 2,
//       rule: VST2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED, type in bitset {'1000','1001'} && align(5:4)=11 => UNDEFINED, not type in bitset {'1000','1001','0011'} => DECODER_ERROR, n == Pc || d2 + regs > 32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8)}
class VectorStoreMultiple2Tester_Case6
    : public VectorStoreMultipleTesterCase6 {
 public:
  VectorStoreMultiple2Tester_Case6()
    : VectorStoreMultipleTesterCase6(
      state_.VectorStoreMultiple2_VST2_multiple_2_element_structures_instance_)
  {}
};

// Neutral case:
// inst(23)=1 & inst(11:8)=1000
//    = {baseline: 'VectorStoreSingle1',
//       constraints: ,
//       rule: 'VST1_single_element_from_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=00 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=01 && inst(7:4)(1)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(2)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=~00 && inst(7:4)(1:0)=~11 => UNDEFINED', '15 == inst(19:16) => UNPREDICTABLE']}
//
// Representative case:
// A(23)=1 & B(11:8)=1000
//    = {Rn: Rn(19:16),
//       baseline: VectorStoreSingle1,
//       constraints: ,
//       fields: [Rn(19:16), size(11:10), index_align(7:4)],
//       index_align: index_align(7:4),
//       n: Rn,
//       rule: VST1_single_element_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=00 && index_align(0)=~0 => UNDEFINED, size(11:10)=01 && index_align(1)=~0 => UNDEFINED, size(11:10)=10 && index_align(2)=~0 => UNDEFINED, size(11:10)=10 && index_align(1:0)=~00 && index_align(1:0)=~11 => UNDEFINED, n == Pc => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingle1Tester_Case7
    : public VectorStoreSingleTesterCase7 {
 public:
  VectorStoreSingle1Tester_Case7()
    : VectorStoreSingleTesterCase7(
      state_.VectorStoreSingle1_VST1_single_element_from_one_lane_instance_)
  {}
};

// Neutral case:
// inst(23)=1 & inst(11:8)=1001
//    = {baseline: 'VectorStoreSingle2',
//       constraints: ,
//       rule: 'VST2_single_2_element_structure_from_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1)=~0 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representative case:
// A(23)=1 & B(11:8)=1001
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       baseline: VectorStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       rule: VST2_single_2_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=10 && index_align(1)=~0 => UNDEFINED, n == Pc || d2 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingle2Tester_Case8
    : public VectorStoreSingleTesterCase8 {
 public:
  VectorStoreSingle2Tester_Case8()
    : VectorStoreSingleTesterCase8(
      state_.VectorStoreSingle2_VST2_single_2_element_structure_from_one_lane_instance_)
  {}
};

// Neutral case:
// inst(23)=1 & inst(11:8)=1010
//    = {baseline: 'VectorStoreSingle3',
//       constraints: ,
//       rule: 'VST3_single_3_element_structure_from_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=00 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=01 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=~00 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representative case:
// A(23)=1 & B(11:8)=1010
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       baseline: VectorStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       rule: VST3_single_3_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=00 && index_align(0)=~0 => UNDEFINED, size(11:10)=01 && index_align(0)=~0 => UNDEFINED, size(11:10)=10 && index_align(1:0)=~00 => UNDEFINED, n == Pc || d3 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingle3Tester_Case9
    : public VectorStoreSingleTesterCase9 {
 public:
  VectorStoreSingle3Tester_Case9()
    : VectorStoreSingleTesterCase9(
      state_.VectorStoreSingle3_VST3_single_3_element_structure_from_one_lane_instance_)
  {}
};

// Neutral case:
// inst(23)=1 & inst(11:8)=1011
//    = {baseline: 'VectorStoreSingle4',
//       constraints: ,
//       rule: 'VST4_single_4_element_structure_form_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=11 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representative case:
// A(23)=1 & B(11:8)=1011
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       baseline: VectorStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       rule: VST4_single_4_element_structure_form_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=10 && index_align(1:0)=11 => UNDEFINED, n == Pc || d4 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingle4Tester_Case10
    : public VectorStoreSingleTesterCase10 {
 public:
  VectorStoreSingle4Tester_Case10()
    : VectorStoreSingleTesterCase10(
      state_.VectorStoreSingle4_VST4_single_4_element_structure_form_one_lane_instance_)
  {}
};

// Neutral case:
// inst(23)=1 & inst(11:8)=0x00
//    = {baseline: 'VectorStoreSingle1',
//       constraints: ,
//       rule: 'VST1_single_element_from_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=00 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=01 && inst(7:4)(1)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(2)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=~00 && inst(7:4)(1:0)=~11 => UNDEFINED', '15 == inst(19:16) => UNPREDICTABLE']}
//
// Representative case:
// A(23)=1 & B(11:8)=0x00
//    = {Rn: Rn(19:16),
//       baseline: VectorStoreSingle1,
//       constraints: ,
//       fields: [Rn(19:16), size(11:10), index_align(7:4)],
//       index_align: index_align(7:4),
//       n: Rn,
//       rule: VST1_single_element_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=00 && index_align(0)=~0 => UNDEFINED, size(11:10)=01 && index_align(1)=~0 => UNDEFINED, size(11:10)=10 && index_align(2)=~0 => UNDEFINED, size(11:10)=10 && index_align(1:0)=~00 && index_align(1:0)=~11 => UNDEFINED, n == Pc => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingle1Tester_Case11
    : public VectorStoreSingleTesterCase11 {
 public:
  VectorStoreSingle1Tester_Case11()
    : VectorStoreSingleTesterCase11(
      state_.VectorStoreSingle1_VST1_single_element_from_one_lane_instance_)
  {}
};

// Neutral case:
// inst(23)=1 & inst(11:8)=0x01
//    = {baseline: 'VectorStoreSingle2',
//       constraints: ,
//       rule: 'VST2_single_2_element_structure_from_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1)=~0 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representative case:
// A(23)=1 & B(11:8)=0x01
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       baseline: VectorStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       rule: VST2_single_2_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=10 && index_align(1)=~0 => UNDEFINED, n == Pc || d2 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingle2Tester_Case12
    : public VectorStoreSingleTesterCase12 {
 public:
  VectorStoreSingle2Tester_Case12()
    : VectorStoreSingleTesterCase12(
      state_.VectorStoreSingle2_VST2_single_2_element_structure_from_one_lane_instance_)
  {}
};

// Neutral case:
// inst(23)=1 & inst(11:8)=0x10
//    = {baseline: 'VectorStoreSingle3',
//       constraints: ,
//       rule: 'VST3_single_3_element_structure_from_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=00 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=01 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=~00 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representative case:
// A(23)=1 & B(11:8)=0x10
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       baseline: VectorStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       rule: VST3_single_3_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=00 && index_align(0)=~0 => UNDEFINED, size(11:10)=01 && index_align(0)=~0 => UNDEFINED, size(11:10)=10 && index_align(1:0)=~00 => UNDEFINED, n == Pc || d3 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingle3Tester_Case13
    : public VectorStoreSingleTesterCase13 {
 public:
  VectorStoreSingle3Tester_Case13()
    : VectorStoreSingleTesterCase13(
      state_.VectorStoreSingle3_VST3_single_3_element_structure_from_one_lane_instance_)
  {}
};

// Neutral case:
// inst(23)=1 & inst(11:8)=0x11
//    = {baseline: 'VectorStoreSingle4',
//       constraints: ,
//       rule: 'VST4_single_4_element_structure_form_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=11 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representative case:
// A(23)=1 & B(11:8)=0x11
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       baseline: VectorStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       rule: VST4_single_4_element_structure_form_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=10 && index_align(1:0)=11 => UNDEFINED, n == Pc || d4 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
class VectorStoreSingle4Tester_Case14
    : public VectorStoreSingleTesterCase14 {
 public:
  VectorStoreSingle4Tester_Case14()
    : VectorStoreSingleTesterCase14(
      state_.VectorStoreSingle4_VST4_single_4_element_structure_form_one_lane_instance_)
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
// inst(23)=0 & inst(11:8)=0010
//    = {actual: 'VectorStoreMultiple1',
//       baseline: 'VectorStoreMultiple1',
//       constraints: ,
//       pattern: '111101000d00nnnnddddttttssaammmm',
//       rule: 'VST1_multiple_single_elements',
//       safety: ['inst(11:8)=0111 && inst(5:4)(1)=1 => UNDEFINED', 'inst(11:8)=1010 && inst(5:4)=11 => UNDEFINED', 'inst(11:8)=0110 && inst(5:4)(1)=1 => UNDEFINED', 'not inst(11:8)=0111 || inst(11:8)=1010 || inst(11:8)=0110 || inst(11:8)=0010 => DECODER_ERROR', '15 == inst(19:16) || 32 <= inst(22):inst(15:12) + 1 if inst(11:8)=0111 else 2 if inst(11:8)=1010 else 3 if inst(11:8)=0110 else 4 if inst(11:8)=0010 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=0 & B(11:8)=0010
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorStoreMultiple1,
//       align: align(5:4),
//       baseline: VectorStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), align(5:4)],
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       regs: 1 if type(11:8)=0111 else 2 if type(11:8)=1010 else 3 if type(11:8)=0110 else 4 if type(11:8)=0010 else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 && align(1)=1 => UNDEFINED, type(11:8)=1010 && align(5:4)=11 => UNDEFINED, type(11:8)=0110 && align(1)=1 => UNDEFINED, not type in bitset {'0111','1010','0110','0010'} => DECODER_ERROR, n == Pc || d + regs > 32 => UNPREDICTABLE],
//       type: type(11:8)}
TEST_F(Arm32DecoderStateTests,
       VectorStoreMultiple1Tester_Case0_TestCase0) {
  VectorStoreMultiple1Tester_Case0 tester;
  tester.Test("111101000d00nnnnddddttttssaammmm");
}

// Neutral case:
// inst(23)=0 & inst(11:8)=0011
//    = {actual: 'VectorStoreMultiple2',
//       baseline: 'VectorStoreMultiple2',
//       constraints: ,
//       pattern: '111101000d00nnnnddddttttssaammmm',
//       rule: 'VST2_multiple_2_element_structures',
//       safety: ['inst(7:6)=11 => UNDEFINED', 'inst(11:8)=1000 || inst(11:8)=1001 && inst(5:4)=11 => UNDEFINED', 'not inst(11:8)=1000 || inst(11:8)=1001 || inst(11:8)=0011 => DECODER_ERROR', '15 == inst(19:16) || 32 <= inst(22):inst(15:12) + 1 if inst(11:8)=1000 else 2 + 1 if inst(11:8)=1000 || inst(11:8)=1001 else 2 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=0 & B(11:8)=0011
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorStoreMultiple2,
//       align: align(5:4),
//       baseline: VectorStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), size(7:6), align(5:4)],
//       inc: 1 if type(11:8)=1000 else 2,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       regs: 1 if type in bitset {'1000','1001'} else 2,
//       rule: VST2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED, type in bitset {'1000','1001'} && align(5:4)=11 => UNDEFINED, not type in bitset {'1000','1001','0011'} => DECODER_ERROR, n == Pc || d2 + regs > 32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8)}
TEST_F(Arm32DecoderStateTests,
       VectorStoreMultiple2Tester_Case1_TestCase1) {
  VectorStoreMultiple2Tester_Case1 tester;
  tester.Test("111101000d00nnnnddddttttssaammmm");
}

// Neutral case:
// inst(23)=0 & inst(11:8)=1010
//    = {actual: 'VectorStoreMultiple1',
//       baseline: 'VectorStoreMultiple1',
//       constraints: ,
//       pattern: '111101000d00nnnnddddttttssaammmm',
//       rule: 'VST1_multiple_single_elements',
//       safety: ['inst(11:8)=0111 && inst(5:4)(1)=1 => UNDEFINED', 'inst(11:8)=1010 && inst(5:4)=11 => UNDEFINED', 'inst(11:8)=0110 && inst(5:4)(1)=1 => UNDEFINED', 'not inst(11:8)=0111 || inst(11:8)=1010 || inst(11:8)=0110 || inst(11:8)=0010 => DECODER_ERROR', '15 == inst(19:16) || 32 <= inst(22):inst(15:12) + 1 if inst(11:8)=0111 else 2 if inst(11:8)=1010 else 3 if inst(11:8)=0110 else 4 if inst(11:8)=0010 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=0 & B(11:8)=1010
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorStoreMultiple1,
//       align: align(5:4),
//       baseline: VectorStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), align(5:4)],
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       regs: 1 if type(11:8)=0111 else 2 if type(11:8)=1010 else 3 if type(11:8)=0110 else 4 if type(11:8)=0010 else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 && align(1)=1 => UNDEFINED, type(11:8)=1010 && align(5:4)=11 => UNDEFINED, type(11:8)=0110 && align(1)=1 => UNDEFINED, not type in bitset {'0111','1010','0110','0010'} => DECODER_ERROR, n == Pc || d + regs > 32 => UNPREDICTABLE],
//       type: type(11:8)}
TEST_F(Arm32DecoderStateTests,
       VectorStoreMultiple1Tester_Case2_TestCase2) {
  VectorStoreMultiple1Tester_Case2 tester;
  tester.Test("111101000d00nnnnddddttttssaammmm");
}

// Neutral case:
// inst(23)=0 & inst(11:8)=000x
//    = {actual: 'VectorStoreMultiple4',
//       baseline: 'VectorStoreMultiple4',
//       constraints: ,
//       pattern: '111101000d00nnnnddddttttssaammmm',
//       rule: 'VST4_multiple_4_element_structures',
//       safety: ['inst(7:6)=11 => UNDEFINED', 'not inst(11:8)=0000 || inst(11:8)=0001 => DECODER_ERROR', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:8)=0000 else 2 + 1 if inst(11:8)=0000 else 2 + 1 if inst(11:8)=0000 else 2 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=0 & B(11:8)=000x
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorStoreMultiple4,
//       baseline: VectorStoreMultiple4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), size(7:6)],
//       inc: 1 if type(11:8)=0000 else 2,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       rule: VST4_multiple_4_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED, not type in bitset {'0000','0001'} => DECODER_ERROR, n == Pc || d4 > 31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8)}
TEST_F(Arm32DecoderStateTests,
       VectorStoreMultiple4Tester_Case3_TestCase3) {
  VectorStoreMultiple4Tester_Case3 tester;
  tester.Test("111101000d00nnnnddddttttssaammmm");
}

// Neutral case:
// inst(23)=0 & inst(11:8)=010x
//    = {actual: 'VectorStoreMultiple3',
//       baseline: 'VectorStoreMultiple3',
//       constraints: ,
//       pattern: '111101000d00nnnnddddttttssaammmm',
//       rule: 'VST3_multiple_3_element_structures',
//       safety: ['inst(7:6)=11 || inst(5:4)(1)=1 => UNDEFINED', 'not inst(11:8)=0100 || inst(11:8)=0101 => DECODER_ERROR', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:8)=0100 else 2 + 1 if inst(11:8)=0100 else 2 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=0 & B(11:8)=010x
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorStoreMultiple3,
//       align: align(5:4),
//       baseline: VectorStoreMultiple3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), size(7:6), align(5:4)],
//       inc: 1 if type(11:8)=0100 else 2,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       rule: VST3_multiple_3_element_structures,
//       safety: [size(7:6)=11 || align(1)=1 => UNDEFINED, not type in bitset {'0100','0101'} => DECODER_ERROR, n == Pc || d3 > 31 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8)}
TEST_F(Arm32DecoderStateTests,
       VectorStoreMultiple3Tester_Case4_TestCase4) {
  VectorStoreMultiple3Tester_Case4 tester;
  tester.Test("111101000d00nnnnddddttttssaammmm");
}

// Neutral case:
// inst(23)=0 & inst(11:8)=011x
//    = {actual: 'VectorStoreMultiple1',
//       baseline: 'VectorStoreMultiple1',
//       constraints: ,
//       pattern: '111101000d00nnnnddddttttssaammmm',
//       rule: 'VST1_multiple_single_elements',
//       safety: ['inst(11:8)=0111 && inst(5:4)(1)=1 => UNDEFINED', 'inst(11:8)=1010 && inst(5:4)=11 => UNDEFINED', 'inst(11:8)=0110 && inst(5:4)(1)=1 => UNDEFINED', 'not inst(11:8)=0111 || inst(11:8)=1010 || inst(11:8)=0110 || inst(11:8)=0010 => DECODER_ERROR', '15 == inst(19:16) || 32 <= inst(22):inst(15:12) + 1 if inst(11:8)=0111 else 2 if inst(11:8)=1010 else 3 if inst(11:8)=0110 else 4 if inst(11:8)=0010 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=0 & B(11:8)=011x
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorStoreMultiple1,
//       align: align(5:4),
//       baseline: VectorStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), align(5:4)],
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       regs: 1 if type(11:8)=0111 else 2 if type(11:8)=1010 else 3 if type(11:8)=0110 else 4 if type(11:8)=0010 else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 && align(1)=1 => UNDEFINED, type(11:8)=1010 && align(5:4)=11 => UNDEFINED, type(11:8)=0110 && align(1)=1 => UNDEFINED, not type in bitset {'0111','1010','0110','0010'} => DECODER_ERROR, n == Pc || d + regs > 32 => UNPREDICTABLE],
//       type: type(11:8)}
TEST_F(Arm32DecoderStateTests,
       VectorStoreMultiple1Tester_Case5_TestCase5) {
  VectorStoreMultiple1Tester_Case5 tester;
  tester.Test("111101000d00nnnnddddttttssaammmm");
}

// Neutral case:
// inst(23)=0 & inst(11:8)=100x
//    = {actual: 'VectorStoreMultiple2',
//       baseline: 'VectorStoreMultiple2',
//       constraints: ,
//       pattern: '111101000d00nnnnddddttttssaammmm',
//       rule: 'VST2_multiple_2_element_structures',
//       safety: ['inst(7:6)=11 => UNDEFINED', 'inst(11:8)=1000 || inst(11:8)=1001 && inst(5:4)=11 => UNDEFINED', 'not inst(11:8)=1000 || inst(11:8)=1001 || inst(11:8)=0011 => DECODER_ERROR', '15 == inst(19:16) || 32 <= inst(22):inst(15:12) + 1 if inst(11:8)=1000 else 2 + 1 if inst(11:8)=1000 || inst(11:8)=1001 else 2 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=0 & B(11:8)=100x
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorStoreMultiple2,
//       align: align(5:4),
//       baseline: VectorStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), type(11:8), size(7:6), align(5:4)],
//       inc: 1 if type(11:8)=1000 else 2,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       regs: 1 if type in bitset {'1000','1001'} else 2,
//       rule: VST2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED, type in bitset {'1000','1001'} && align(5:4)=11 => UNDEFINED, not type in bitset {'1000','1001','0011'} => DECODER_ERROR, n == Pc || d2 + regs > 32 => UNPREDICTABLE],
//       size: size(7:6),
//       type: type(11:8)}
TEST_F(Arm32DecoderStateTests,
       VectorStoreMultiple2Tester_Case6_TestCase6) {
  VectorStoreMultiple2Tester_Case6 tester;
  tester.Test("111101000d00nnnnddddttttssaammmm");
}

// Neutral case:
// inst(23)=1 & inst(11:8)=1000
//    = {actual: 'VectorStoreSingle1',
//       baseline: 'VectorStoreSingle1',
//       constraints: ,
//       pattern: '111101001d00nnnnddddss00aaaammmm',
//       rule: 'VST1_single_element_from_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=00 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=01 && inst(7:4)(1)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(2)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=~00 && inst(7:4)(1:0)=~11 => UNDEFINED', '15 == inst(19:16) => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=1000
//    = {Rn: Rn(19:16),
//       actual: VectorStoreSingle1,
//       baseline: VectorStoreSingle1,
//       constraints: ,
//       fields: [Rn(19:16), size(11:10), index_align(7:4)],
//       index_align: index_align(7:4),
//       n: Rn,
//       pattern: 111101001d00nnnnddddss00aaaammmm,
//       rule: VST1_single_element_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=00 && index_align(0)=~0 => UNDEFINED, size(11:10)=01 && index_align(1)=~0 => UNDEFINED, size(11:10)=10 && index_align(2)=~0 => UNDEFINED, size(11:10)=10 && index_align(1:0)=~00 && index_align(1:0)=~11 => UNDEFINED, n == Pc => UNPREDICTABLE],
//       size: size(11:10)}
TEST_F(Arm32DecoderStateTests,
       VectorStoreSingle1Tester_Case7_TestCase7) {
  VectorStoreSingle1Tester_Case7 tester;
  tester.Test("111101001d00nnnnddddss00aaaammmm");
}

// Neutral case:
// inst(23)=1 & inst(11:8)=1001
//    = {actual: 'VectorStoreSingle2',
//       baseline: 'VectorStoreSingle2',
//       constraints: ,
//       pattern: '111101001d00nnnnddddss01aaaammmm',
//       rule: 'VST2_single_2_element_structure_from_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1)=~0 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=1001
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorStoreSingle2,
//       baseline: VectorStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       pattern: 111101001d00nnnnddddss01aaaammmm,
//       rule: VST2_single_2_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=10 && index_align(1)=~0 => UNDEFINED, n == Pc || d2 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
TEST_F(Arm32DecoderStateTests,
       VectorStoreSingle2Tester_Case8_TestCase8) {
  VectorStoreSingle2Tester_Case8 tester;
  tester.Test("111101001d00nnnnddddss01aaaammmm");
}

// Neutral case:
// inst(23)=1 & inst(11:8)=1010
//    = {actual: 'VectorStoreSingle3',
//       baseline: 'VectorStoreSingle3',
//       constraints: ,
//       pattern: '111101001d00nnnnddddss10aaaammmm',
//       rule: 'VST3_single_3_element_structure_from_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=00 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=01 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=~00 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=1010
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorStoreSingle3,
//       baseline: VectorStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       pattern: 111101001d00nnnnddddss10aaaammmm,
//       rule: VST3_single_3_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=00 && index_align(0)=~0 => UNDEFINED, size(11:10)=01 && index_align(0)=~0 => UNDEFINED, size(11:10)=10 && index_align(1:0)=~00 => UNDEFINED, n == Pc || d3 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
TEST_F(Arm32DecoderStateTests,
       VectorStoreSingle3Tester_Case9_TestCase9) {
  VectorStoreSingle3Tester_Case9 tester;
  tester.Test("111101001d00nnnnddddss10aaaammmm");
}

// Neutral case:
// inst(23)=1 & inst(11:8)=1011
//    = {actual: 'VectorStoreSingle4',
//       baseline: 'VectorStoreSingle4',
//       constraints: ,
//       pattern: '111101001d00nnnnddddss11aaaammmm',
//       rule: 'VST4_single_4_element_structure_form_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=11 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=1011
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorStoreSingle4,
//       baseline: VectorStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       pattern: 111101001d00nnnnddddss11aaaammmm,
//       rule: VST4_single_4_element_structure_form_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=10 && index_align(1:0)=11 => UNDEFINED, n == Pc || d4 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
TEST_F(Arm32DecoderStateTests,
       VectorStoreSingle4Tester_Case10_TestCase10) {
  VectorStoreSingle4Tester_Case10 tester;
  tester.Test("111101001d00nnnnddddss11aaaammmm");
}

// Neutral case:
// inst(23)=1 & inst(11:8)=0x00
//    = {actual: 'VectorStoreSingle1',
//       baseline: 'VectorStoreSingle1',
//       constraints: ,
//       pattern: '111101001d00nnnnddddss00aaaammmm',
//       rule: 'VST1_single_element_from_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=00 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=01 && inst(7:4)(1)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(2)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=~00 && inst(7:4)(1:0)=~11 => UNDEFINED', '15 == inst(19:16) => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=0x00
//    = {Rn: Rn(19:16),
//       actual: VectorStoreSingle1,
//       baseline: VectorStoreSingle1,
//       constraints: ,
//       fields: [Rn(19:16), size(11:10), index_align(7:4)],
//       index_align: index_align(7:4),
//       n: Rn,
//       pattern: 111101001d00nnnnddddss00aaaammmm,
//       rule: VST1_single_element_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=00 && index_align(0)=~0 => UNDEFINED, size(11:10)=01 && index_align(1)=~0 => UNDEFINED, size(11:10)=10 && index_align(2)=~0 => UNDEFINED, size(11:10)=10 && index_align(1:0)=~00 && index_align(1:0)=~11 => UNDEFINED, n == Pc => UNPREDICTABLE],
//       size: size(11:10)}
TEST_F(Arm32DecoderStateTests,
       VectorStoreSingle1Tester_Case11_TestCase11) {
  VectorStoreSingle1Tester_Case11 tester;
  tester.Test("111101001d00nnnnddddss00aaaammmm");
}

// Neutral case:
// inst(23)=1 & inst(11:8)=0x01
//    = {actual: 'VectorStoreSingle2',
//       baseline: 'VectorStoreSingle2',
//       constraints: ,
//       pattern: '111101001d00nnnnddddss01aaaammmm',
//       rule: 'VST2_single_2_element_structure_from_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1)=~0 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=0x01
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorStoreSingle2,
//       baseline: VectorStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       pattern: 111101001d00nnnnddddss01aaaammmm,
//       rule: VST2_single_2_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=10 && index_align(1)=~0 => UNDEFINED, n == Pc || d2 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
TEST_F(Arm32DecoderStateTests,
       VectorStoreSingle2Tester_Case12_TestCase12) {
  VectorStoreSingle2Tester_Case12 tester;
  tester.Test("111101001d00nnnnddddss01aaaammmm");
}

// Neutral case:
// inst(23)=1 & inst(11:8)=0x10
//    = {actual: 'VectorStoreSingle3',
//       baseline: 'VectorStoreSingle3',
//       constraints: ,
//       pattern: '111101001d00nnnnddddss10aaaammmm',
//       rule: 'VST3_single_3_element_structure_from_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=00 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=01 && inst(7:4)(0)=~0 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=~00 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=0x10
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorStoreSingle3,
//       baseline: VectorStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       pattern: 111101001d00nnnnddddss10aaaammmm,
//       rule: VST3_single_3_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=00 && index_align(0)=~0 => UNDEFINED, size(11:10)=01 && index_align(0)=~0 => UNDEFINED, size(11:10)=10 && index_align(1:0)=~00 => UNDEFINED, n == Pc || d3 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
TEST_F(Arm32DecoderStateTests,
       VectorStoreSingle3Tester_Case13_TestCase13) {
  VectorStoreSingle3Tester_Case13 tester;
  tester.Test("111101001d00nnnnddddss10aaaammmm");
}

// Neutral case:
// inst(23)=1 & inst(11:8)=0x11
//    = {actual: 'VectorStoreSingle4',
//       baseline: 'VectorStoreSingle4',
//       constraints: ,
//       pattern: '111101001d00nnnnddddss11aaaammmm',
//       rule: 'VST4_single_4_element_structure_form_one_lane',
//       safety: ['inst(11:10)=11 => UNDEFINED', 'inst(11:10)=10 && inst(7:4)(1:0)=11 => UNDEFINED', '15 == inst(19:16) || 31 <= inst(22):inst(15:12) + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 + 1 if inst(11:10)=00 else (1 if inst(7:4)(1)=0 else 2) if inst(11:10)=01 else (1 if inst(7:4)(2)=0 else 2) if inst(11:10)=10 else 0 => UNPREDICTABLE']}
//
// Representaive case:
// A(23)=1 & B(11:8)=0x11
//    = {D: D(22),
//       Rn: Rn(19:16),
//       Vd: Vd(15:12),
//       actual: VectorStoreSingle4,
//       baseline: VectorStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       fields: [D(22), Rn(19:16), Vd(15:12), size(11:10), index_align(7:4)],
//       inc: 1 if size(11:10)=00 else (1 if index_align(1)=0 else 2) if size(11:10)=01 else (1 if index_align(2)=0 else 2) if size(11:10)=10 else 0,
//       index_align: index_align(7:4),
//       n: Rn,
//       pattern: 111101001d00nnnnddddss11aaaammmm,
//       rule: VST4_single_4_element_structure_form_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED, size(11:10)=10 && index_align(1:0)=11 => UNDEFINED, n == Pc || d4 > 31 => UNPREDICTABLE],
//       size: size(11:10)}
TEST_F(Arm32DecoderStateTests,
       VectorStoreSingle4Tester_Case14_TestCase14) {
  VectorStoreSingle4Tester_Case14 tester;
  tester.Test("111101001d00nnnnddddss11aaaammmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
