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
// inst(11:8)=0100 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(19:16)(0)=1 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0100 & U(24)=0
//    = {Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterDifferentLength_I16_32_64,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vm(3:0)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vn(0)=1 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLengthTesterCase0
    : public VectorBinary3RegisterDifferentLengthTester {
 public:
  VectorBinary3RegisterDifferentLengthTesterCase0(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterDifferentLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterDifferentLengthTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000400) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterDifferentLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterDifferentLengthTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterDifferentLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: Vn(0)=1 ||
  //       Vm(0)=1 => UNDEFINED
  EXPECT_TRUE(!(((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0100 & inst(24)=1
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(19:16)(0)=1 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0100 & U(24)=1
//    = {Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterDifferentLength_I16_32_64,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vm(3:0)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vn(0)=1 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLengthTesterCase1
    : public VectorBinary3RegisterDifferentLengthTester {
 public:
  VectorBinary3RegisterDifferentLengthTesterCase1(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterDifferentLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterDifferentLengthTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000400) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterDifferentLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterDifferentLengthTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterDifferentLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: Vn(0)=1 ||
  //       Vm(0)=1 => UNDEFINED
  EXPECT_TRUE(!(((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0101
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0101
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLengthTesterCase2
    : public VectorBinary3RegisterDifferentLengthTester {
 public:
  VectorBinary3RegisterDifferentLengthTesterCase2(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterDifferentLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterDifferentLengthTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000500) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterDifferentLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterDifferentLengthTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterDifferentLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: Vd(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0110 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(19:16)(0)=1 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0110 & U(24)=0
//    = {Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterDifferentLength_I16_32_64,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vm(3:0)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vn(0)=1 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLengthTesterCase3
    : public VectorBinary3RegisterDifferentLengthTester {
 public:
  VectorBinary3RegisterDifferentLengthTesterCase3(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterDifferentLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterDifferentLengthTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000600) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterDifferentLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterDifferentLengthTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterDifferentLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: Vn(0)=1 ||
  //       Vm(0)=1 => UNDEFINED
  EXPECT_TRUE(!(((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0110 & inst(24)=1
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(19:16)(0)=1 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0110 & U(24)=1
//    = {Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterDifferentLength_I16_32_64,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vm(3:0)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vn(0)=1 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLengthTesterCase4
    : public VectorBinary3RegisterDifferentLengthTester {
 public:
  VectorBinary3RegisterDifferentLengthTesterCase4(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterDifferentLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterDifferentLengthTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000600) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterDifferentLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterDifferentLengthTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterDifferentLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: Vn(0)=1 ||
  //       Vm(0)=1 => UNDEFINED
  EXPECT_TRUE(!(((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0111
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0111
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLengthTesterCase5
    : public VectorBinary3RegisterDifferentLengthTester {
 public:
  VectorBinary3RegisterDifferentLengthTesterCase5(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterDifferentLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterDifferentLengthTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000700) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterDifferentLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterDifferentLengthTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterDifferentLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: Vd(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1100
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=1100
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLengthTesterCase6
    : public VectorBinary3RegisterDifferentLengthTester {
 public:
  VectorBinary3RegisterDifferentLengthTesterCase6(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterDifferentLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterDifferentLengthTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000C00) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterDifferentLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterDifferentLengthTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterDifferentLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: Vd(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1101 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(21:20)=00 ||
//            inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=1101 & U(24)=0
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary3RegisterDifferentLength_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 ||
//            Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLengthTesterCase7
    : public VectorBinary3RegisterDifferentLengthTester {
 public:
  VectorBinary3RegisterDifferentLengthTesterCase7(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterDifferentLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterDifferentLengthTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterDifferentLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterDifferentLengthTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterDifferentLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: size(21:20)=00 ||
  //       Vd(0)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1110
//    = {baseline: 'VectorBinary3RegisterDifferentLength_P8',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 ||
//            inst(21:20)=~00 => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1110
//    = {U: U(24),
//       Vd: Vd(15:12),
//       baseline: VectorBinary3RegisterDifferentLength_P8,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [U(24), size(21:20), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         U(24)=1 ||
//            size(21:20)=~00 => UNDEFINED,
//         Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLengthTesterCase8
    : public VectorBinary3RegisterDifferentLengthTester {
 public:
  VectorBinary3RegisterDifferentLengthTesterCase8(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterDifferentLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterDifferentLengthTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000E00) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterDifferentLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterDifferentLengthTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterDifferentLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: U(24)=1 ||
  //       size(21:20)=~00 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) ||
       ((inst.Bits() & 0x00300000)  !=
          0x00000000)));

  // safety: Vd(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=10x0
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=10x0
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLengthTesterCase9
    : public VectorBinary3RegisterDifferentLengthTester {
 public:
  VectorBinary3RegisterDifferentLengthTesterCase9(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterDifferentLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterDifferentLengthTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~10x0
  if ((inst.Bits() & 0x00000D00)  !=
          0x00000800) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterDifferentLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterDifferentLengthTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterDifferentLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: Vd(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=10x1 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(21:20)=00 ||
//            inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=10x1 & U(24)=0
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary3RegisterDifferentLength_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 ||
//            Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLengthTesterCase10
    : public VectorBinary3RegisterDifferentLengthTester {
 public:
  VectorBinary3RegisterDifferentLengthTesterCase10(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterDifferentLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterDifferentLengthTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~10x1
  if ((inst.Bits() & 0x00000D00)  !=
          0x00000900) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterDifferentLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterDifferentLengthTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterDifferentLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: size(21:20)=00 ||
  //       Vd(0)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=000x
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(15:12)(0)=1 ||
//            (inst(8)=1 &&
//            inst(19:16)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=000x
//    = {Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), op(8)],
//       op: op(8),
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vd(0)=1 ||
//            (op(8)=1 &&
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLengthTesterCase11
    : public VectorBinary3RegisterDifferentLengthTester {
 public:
  VectorBinary3RegisterDifferentLengthTesterCase11(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterDifferentLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterDifferentLengthTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~000x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000000) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterDifferentLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterDifferentLengthTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterDifferentLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: Vd(0)=1 ||
  //       (op(8)=1 &&
  //       Vn(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x00000100)  ==
          0x00000100) &&
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=001x
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(15:12)(0)=1 ||
//            (inst(8)=1 &&
//            inst(19:16)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=001x
//    = {Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), op(8)],
//       op: op(8),
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vd(0)=1 ||
//            (op(8)=1 &&
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLengthTesterCase12
    : public VectorBinary3RegisterDifferentLengthTester {
 public:
  VectorBinary3RegisterDifferentLengthTesterCase12(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterDifferentLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterDifferentLengthTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~001x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000200) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterDifferentLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterDifferentLengthTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterDifferentLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: Vd(0)=1 ||
  //       (op(8)=1 &&
  //       Vn(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x00000100)  ==
          0x00000100) &&
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(11:8)=0100 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VADDHN',
//       safety: [inst(19:16)(0)=1 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=0100 & U(24)=0
//    = {Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterDifferentLength_I16_32_64,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vm(3:0)],
//       rule: VADDHN,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vn(0)=1 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case0
    : public VectorBinary3RegisterDifferentLengthTesterCase0 {
 public:
  VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case0()
    : VectorBinary3RegisterDifferentLengthTesterCase0(
      state_.VectorBinary3RegisterDifferentLength_I16_32_64_VADDHN_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0100 & inst(24)=1
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VRADDHN',
//       safety: [inst(19:16)(0)=1 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=0100 & U(24)=1
//    = {Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterDifferentLength_I16_32_64,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vm(3:0)],
//       rule: VRADDHN,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vn(0)=1 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case1
    : public VectorBinary3RegisterDifferentLengthTesterCase1 {
 public:
  VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case1()
    : VectorBinary3RegisterDifferentLengthTesterCase1(
      state_.VectorBinary3RegisterDifferentLength_I16_32_64_VRADDHN_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0101
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VABAL_A2',
//       safety: [inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=0101
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       rule: VABAL_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case2
    : public VectorBinary3RegisterDifferentLengthTesterCase2 {
 public:
  VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case2()
    : VectorBinary3RegisterDifferentLengthTesterCase2(
      state_.VectorBinary3RegisterDifferentLength_I8_16_32L_VABAL_A2_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0110 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VSUBHN',
//       safety: [inst(19:16)(0)=1 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=0110 & U(24)=0
//    = {Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterDifferentLength_I16_32_64,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vm(3:0)],
//       rule: VSUBHN,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vn(0)=1 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case3
    : public VectorBinary3RegisterDifferentLengthTesterCase3 {
 public:
  VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case3()
    : VectorBinary3RegisterDifferentLengthTesterCase3(
      state_.VectorBinary3RegisterDifferentLength_I16_32_64_VSUBHN_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0110 & inst(24)=1
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VRSUBHN',
//       safety: [inst(19:16)(0)=1 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=0110 & U(24)=1
//    = {Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterDifferentLength_I16_32_64,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vm(3:0)],
//       rule: VRSUBHN,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vn(0)=1 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case4
    : public VectorBinary3RegisterDifferentLengthTesterCase4 {
 public:
  VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case4()
    : VectorBinary3RegisterDifferentLengthTesterCase4(
      state_.VectorBinary3RegisterDifferentLength_I16_32_64_VRSUBHN_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0111
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VABDL_integer_A2',
//       safety: [inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=0111
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       rule: VABDL_integer_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case5
    : public VectorBinary3RegisterDifferentLengthTesterCase5 {
 public:
  VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case5()
    : VectorBinary3RegisterDifferentLengthTesterCase5(
      state_.VectorBinary3RegisterDifferentLength_I8_16_32L_VABDL_integer_A2_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1100
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VMULL_integer_A2',
//       safety: [inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=1100
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       rule: VMULL_integer_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case6
    : public VectorBinary3RegisterDifferentLengthTesterCase6 {
 public:
  VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case6()
    : VectorBinary3RegisterDifferentLengthTesterCase6(
      state_.VectorBinary3RegisterDifferentLength_I8_16_32L_VMULL_integer_A2_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1101 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VQDMULL_A1',
//       safety: [inst(21:20)=00 ||
//            inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=1101 & U(24)=0
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary3RegisterDifferentLength_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       rule: VQDMULL_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 ||
//            Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLength_I16_32LTester_Case7
    : public VectorBinary3RegisterDifferentLengthTesterCase7 {
 public:
  VectorBinary3RegisterDifferentLength_I16_32LTester_Case7()
    : VectorBinary3RegisterDifferentLengthTesterCase7(
      state_.VectorBinary3RegisterDifferentLength_I16_32L_VQDMULL_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1110
//    = {baseline: 'VectorBinary3RegisterDifferentLength_P8',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VMULL_polynomial_A2',
//       safety: [inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 ||
//            inst(21:20)=~00 => UNDEFINED]}
//
// Representative case:
// A(11:8)=1110
//    = {U: U(24),
//       Vd: Vd(15:12),
//       baseline: VectorBinary3RegisterDifferentLength_P8,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [U(24), size(21:20), Vd(15:12)],
//       rule: VMULL_polynomial_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         U(24)=1 ||
//            size(21:20)=~00 => UNDEFINED,
//         Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLength_P8Tester_Case8
    : public VectorBinary3RegisterDifferentLengthTesterCase8 {
 public:
  VectorBinary3RegisterDifferentLength_P8Tester_Case8()
    : VectorBinary3RegisterDifferentLengthTesterCase8(
      state_.VectorBinary3RegisterDifferentLength_P8_VMULL_polynomial_A2_instance_)
  {}
};

// Neutral case:
// inst(11:8)=10x0
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VMLAL_VMLSL_integer_A2',
//       safety: [inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=10x0
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       rule: VMLAL_VMLSL_integer_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case9
    : public VectorBinary3RegisterDifferentLengthTesterCase9 {
 public:
  VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case9()
    : VectorBinary3RegisterDifferentLengthTesterCase9(
      state_.VectorBinary3RegisterDifferentLength_I8_16_32L_VMLAL_VMLSL_integer_A2_instance_)
  {}
};

// Neutral case:
// inst(11:8)=10x1 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VQDMLAL_VQDMLSL_A1',
//       safety: [inst(21:20)=00 ||
//            inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=10x1 & U(24)=0
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary3RegisterDifferentLength_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       rule: VQDMLAL_VQDMLSL_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 ||
//            Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLength_I16_32LTester_Case10
    : public VectorBinary3RegisterDifferentLengthTesterCase10 {
 public:
  VectorBinary3RegisterDifferentLength_I16_32LTester_Case10()
    : VectorBinary3RegisterDifferentLengthTesterCase10(
      state_.VectorBinary3RegisterDifferentLength_I16_32L_VQDMLAL_VQDMLSL_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=000x
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VADDL_VADDW',
//       safety: [inst(15:12)(0)=1 ||
//            (inst(8)=1 &&
//            inst(19:16)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=000x
//    = {Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), op(8)],
//       op: op(8),
//       rule: VADDL_VADDW,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vd(0)=1 ||
//            (op(8)=1 &&
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLength_I8_16_32Tester_Case11
    : public VectorBinary3RegisterDifferentLengthTesterCase11 {
 public:
  VectorBinary3RegisterDifferentLength_I8_16_32Tester_Case11()
    : VectorBinary3RegisterDifferentLengthTesterCase11(
      state_.VectorBinary3RegisterDifferentLength_I8_16_32_VADDL_VADDW_instance_)
  {}
};

// Neutral case:
// inst(11:8)=001x
//    = {baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VSUBL_VSUBW',
//       safety: [inst(15:12)(0)=1 ||
//            (inst(8)=1 &&
//            inst(19:16)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=001x
//    = {Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), op(8)],
//       op: op(8),
//       rule: VSUBL_VSUBW,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vd(0)=1 ||
//            (op(8)=1 &&
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterDifferentLength_I8_16_32Tester_Case12
    : public VectorBinary3RegisterDifferentLengthTesterCase12 {
 public:
  VectorBinary3RegisterDifferentLength_I8_16_32Tester_Case12()
    : VectorBinary3RegisterDifferentLengthTesterCase12(
      state_.VectorBinary3RegisterDifferentLength_I8_16_32_VSUBL_VSUBW_instance_)
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
// inst(11:8)=0100 & inst(24)=0
//    = {actual: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       baseline: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '111100101dssnnnndddd0100n0m0mmmm',
//       rule: 'VADDHN',
//       safety: [inst(19:16)(0)=1 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0100 & U(24)=0
//    = {Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterDifferentLength_I16_32_64,
//       baseline: VectorBinary3RegisterDifferentLength_I16_32_64,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vm(3:0)],
//       pattern: 111100101dssnnnndddd0100n0m0mmmm,
//       rule: VADDHN,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vn(0)=1 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case0_TestCase0) {
  VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case0 tester;
  tester.Test("111100101dssnnnndddd0100n0m0mmmm");
}

// Neutral case:
// inst(11:8)=0100 & inst(24)=1
//    = {actual: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       baseline: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '111100111dssnnnndddd0100n0m0mmmm',
//       rule: 'VRADDHN',
//       safety: [inst(19:16)(0)=1 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0100 & U(24)=1
//    = {Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterDifferentLength_I16_32_64,
//       baseline: VectorBinary3RegisterDifferentLength_I16_32_64,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vm(3:0)],
//       pattern: 111100111dssnnnndddd0100n0m0mmmm,
//       rule: VRADDHN,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vn(0)=1 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case1_TestCase1) {
  VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case1 tester;
  tester.Test("111100111dssnnnndddd0100n0m0mmmm");
}

// Neutral case:
// inst(11:8)=0101
//    = {actual: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001u1dssnnnndddd0101n0m0mmmm',
//       rule: 'VABAL_A2',
//       safety: [inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0101
//    = {Vd: Vd(15:12),
//       actual: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       pattern: 1111001u1dssnnnndddd0101n0m0mmmm,
//       rule: VABAL_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case2_TestCase2) {
  VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case2 tester;
  tester.Test("1111001u1dssnnnndddd0101n0m0mmmm");
}

// Neutral case:
// inst(11:8)=0110 & inst(24)=0
//    = {actual: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       baseline: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '111100101dssnnnndddd0110n0m0mmmm',
//       rule: 'VSUBHN',
//       safety: [inst(19:16)(0)=1 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0110 & U(24)=0
//    = {Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterDifferentLength_I16_32_64,
//       baseline: VectorBinary3RegisterDifferentLength_I16_32_64,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vm(3:0)],
//       pattern: 111100101dssnnnndddd0110n0m0mmmm,
//       rule: VSUBHN,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vn(0)=1 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case3_TestCase3) {
  VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case3 tester;
  tester.Test("111100101dssnnnndddd0110n0m0mmmm");
}

// Neutral case:
// inst(11:8)=0110 & inst(24)=1
//    = {actual: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       baseline: 'VectorBinary3RegisterDifferentLength_I16_32_64',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '111100111dssnnnndddd0110n0m0mmmm',
//       rule: 'VRSUBHN',
//       safety: [inst(19:16)(0)=1 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0110 & U(24)=1
//    = {Vm: Vm(3:0),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterDifferentLength_I16_32_64,
//       baseline: VectorBinary3RegisterDifferentLength_I16_32_64,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vm(3:0)],
//       pattern: 111100111dssnnnndddd0110n0m0mmmm,
//       rule: VRSUBHN,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vn(0)=1 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case4_TestCase4) {
  VectorBinary3RegisterDifferentLength_I16_32_64Tester_Case4 tester;
  tester.Test("111100111dssnnnndddd0110n0m0mmmm");
}

// Neutral case:
// inst(11:8)=0111
//    = {actual: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001u1dssnnnndddd0111n0m0mmmm',
//       rule: 'VABDL_integer_A2',
//       safety: [inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0111
//    = {Vd: Vd(15:12),
//       actual: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       pattern: 1111001u1dssnnnndddd0111n0m0mmmm,
//       rule: VABDL_integer_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case5_TestCase5) {
  VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case5 tester;
  tester.Test("1111001u1dssnnnndddd0111n0m0mmmm");
}

// Neutral case:
// inst(11:8)=1100
//    = {actual: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001u1dssnnnndddd11p0n0m0mmmm',
//       rule: 'VMULL_integer_A2',
//       safety: [inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=1100
//    = {Vd: Vd(15:12),
//       actual: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       pattern: 1111001u1dssnnnndddd11p0n0m0mmmm,
//       rule: VMULL_integer_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case6_TestCase6) {
  VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case6 tester;
  tester.Test("1111001u1dssnnnndddd11p0n0m0mmmm");
}

// Neutral case:
// inst(11:8)=1101 & inst(24)=0
//    = {actual: 'VectorBinary3RegisterDifferentLength_I16_32L',
//       baseline: 'VectorBinary3RegisterDifferentLength_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '111100101dssnnnndddd1101n0m0mmmm',
//       rule: 'VQDMULL_A1',
//       safety: [inst(21:20)=00 ||
//            inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=1101 & U(24)=0
//    = {Vd: Vd(15:12),
//       actual: VectorBinary3RegisterDifferentLength_I16_32L,
//       baseline: VectorBinary3RegisterDifferentLength_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       pattern: 111100101dssnnnndddd1101n0m0mmmm,
//       rule: VQDMULL_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 ||
//            Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterDifferentLength_I16_32LTester_Case7_TestCase7) {
  VectorBinary3RegisterDifferentLength_I16_32LTester_Case7 tester;
  tester.Test("111100101dssnnnndddd1101n0m0mmmm");
}

// Neutral case:
// inst(11:8)=1110
//    = {actual: 'VectorBinary3RegisterDifferentLength_P8',
//       baseline: 'VectorBinary3RegisterDifferentLength_P8',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001u1dssnnnndddd11p0n0m0mmmm',
//       rule: 'VMULL_polynomial_A2',
//       safety: [inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 ||
//            inst(21:20)=~00 => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1110
//    = {U: U(24),
//       Vd: Vd(15:12),
//       actual: VectorBinary3RegisterDifferentLength_P8,
//       baseline: VectorBinary3RegisterDifferentLength_P8,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [U(24), size(21:20), Vd(15:12)],
//       pattern: 1111001u1dssnnnndddd11p0n0m0mmmm,
//       rule: VMULL_polynomial_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         U(24)=1 ||
//            size(21:20)=~00 => UNDEFINED,
//         Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterDifferentLength_P8Tester_Case8_TestCase8) {
  VectorBinary3RegisterDifferentLength_P8Tester_Case8 tester;
  tester.Test("1111001u1dssnnnndddd11p0n0m0mmmm");
}

// Neutral case:
// inst(11:8)=10x0
//    = {actual: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001u1dssnnnndddd10p0n0m0mmmm',
//       rule: 'VMLAL_VMLSL_integer_A2',
//       safety: [inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=10x0
//    = {Vd: Vd(15:12),
//       actual: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       pattern: 1111001u1dssnnnndddd10p0n0m0mmmm,
//       rule: VMLAL_VMLSL_integer_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case9_TestCase9) {
  VectorBinary3RegisterDifferentLength_I8_16_32LTester_Case9 tester;
  tester.Test("1111001u1dssnnnndddd10p0n0m0mmmm");
}

// Neutral case:
// inst(11:8)=10x1 & inst(24)=0
//    = {actual: 'VectorBinary3RegisterDifferentLength_I16_32L',
//       baseline: 'VectorBinary3RegisterDifferentLength_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '111100101dssnnnndddd10p1n0m0mmmm',
//       rule: 'VQDMLAL_VQDMLSL_A1',
//       safety: [inst(21:20)=00 ||
//            inst(15:12)(0)=1 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=10x1 & U(24)=0
//    = {Vd: Vd(15:12),
//       actual: VectorBinary3RegisterDifferentLength_I16_32L,
//       baseline: VectorBinary3RegisterDifferentLength_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       pattern: 111100101dssnnnndddd10p1n0m0mmmm,
//       rule: VQDMLAL_VQDMLSL_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 ||
//            Vd(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterDifferentLength_I16_32LTester_Case10_TestCase10) {
  VectorBinary3RegisterDifferentLength_I16_32LTester_Case10 tester;
  tester.Test("111100101dssnnnndddd10p1n0m0mmmm");
}

// Neutral case:
// inst(11:8)=000x
//    = {actual: 'VectorBinary3RegisterDifferentLength_I8_16_32',
//       baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001u1dssnnnndddd000pn0m0mmmm',
//       rule: 'VADDL_VADDW',
//       safety: [inst(15:12)(0)=1 ||
//            (inst(8)=1 &&
//            inst(19:16)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=000x
//    = {Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterDifferentLength_I8_16_32,
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), op(8)],
//       op: op(8),
//       pattern: 1111001u1dssnnnndddd000pn0m0mmmm,
//       rule: VADDL_VADDW,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vd(0)=1 ||
//            (op(8)=1 &&
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterDifferentLength_I8_16_32Tester_Case11_TestCase11) {
  VectorBinary3RegisterDifferentLength_I8_16_32Tester_Case11 tester;
  tester.Test("1111001u1dssnnnndddd000pn0m0mmmm");
}

// Neutral case:
// inst(11:8)=001x
//    = {actual: 'VectorBinary3RegisterDifferentLength_I8_16_32',
//       baseline: 'VectorBinary3RegisterDifferentLength_I8_16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001u1dssnnnndddd001pn0m0mmmm',
//       rule: 'VSUBL_VSUBW',
//       safety: [inst(15:12)(0)=1 ||
//            (inst(8)=1 &&
//            inst(19:16)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=001x
//    = {Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterDifferentLength_I8_16_32,
//       baseline: VectorBinary3RegisterDifferentLength_I8_16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), op(8)],
//       op: op(8),
//       pattern: 1111001u1dssnnnndddd001pn0m0mmmm,
//       rule: VSUBL_VSUBW,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         Vd(0)=1 ||
//            (op(8)=1 &&
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterDifferentLength_I8_16_32Tester_Case12_TestCase12) {
  VectorBinary3RegisterDifferentLength_I8_16_32Tester_Case12 tester;
  tester.Test("1111001u1dssnnnndddd001pn0m0mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
