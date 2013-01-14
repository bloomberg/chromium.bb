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
// inst(11:8)=0000
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(21:20)=00 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0000
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_I16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalarTesterCase0
    : public VectorBinary2RegisterScalarTester {
 public:
  VectorBinary2RegisterScalarTesterCase0(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterScalarTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterScalarTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000000) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterScalarTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary2RegisterScalarTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary2RegisterScalarTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: size(21:20)=00 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00000000);

  // safety: Q(24)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0001
//    = {baseline: 'VectorBinary2RegisterScalar_F32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [(inst(21:20)=00 ||
//            inst(21:20)=01) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_F32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            size(21:20)=01) => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalarTesterCase1
    : public VectorBinary2RegisterScalarTester {
 public:
  VectorBinary2RegisterScalarTesterCase1(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterScalarTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterScalarTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterScalarTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary2RegisterScalarTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary2RegisterScalarTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: (size(21:20)=00 ||
  //       size(21:20)=01) => UNDEFINED
  EXPECT_TRUE((!(((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00300000)  ==
          0x00100000))));

  // safety: Q(24)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0010
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0010
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalarTesterCase2
    : public VectorBinary2RegisterScalarTester {
 public:
  VectorBinary2RegisterScalarTesterCase2(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterScalarTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterScalarTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000200) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterScalarTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary2RegisterScalarTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary2RegisterScalarTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: (size(21:20)=00 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE((!(((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0011 & inst(24)=0
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0011 & U(24)=0
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalarTesterCase3
    : public VectorBinary2RegisterScalarTester {
 public:
  VectorBinary2RegisterScalarTesterCase3(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterScalarTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterScalarTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000300) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterScalarTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary2RegisterScalarTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary2RegisterScalarTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: (size(21:20)=00 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE((!(((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0100
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(21:20)=00 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0100
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_I16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalarTesterCase4
    : public VectorBinary2RegisterScalarTester {
 public:
  VectorBinary2RegisterScalarTesterCase4(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterScalarTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterScalarTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000400) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterScalarTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary2RegisterScalarTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary2RegisterScalarTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: size(21:20)=00 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00000000);

  // safety: Q(24)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0101
//    = {baseline: 'VectorBinary2RegisterScalar_F32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [(inst(21:20)=00 ||
//            inst(21:20)=01) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0101
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_F32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            size(21:20)=01) => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalarTesterCase5
    : public VectorBinary2RegisterScalarTester {
 public:
  VectorBinary2RegisterScalarTesterCase5(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterScalarTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterScalarTesterCase5
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
  return VectorBinary2RegisterScalarTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary2RegisterScalarTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary2RegisterScalarTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: (size(21:20)=00 ||
  //       size(21:20)=01) => UNDEFINED
  EXPECT_TRUE((!(((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00300000)  ==
          0x00100000))));

  // safety: Q(24)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0110
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0110
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalarTesterCase6
    : public VectorBinary2RegisterScalarTester {
 public:
  VectorBinary2RegisterScalarTesterCase6(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterScalarTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterScalarTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000600) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterScalarTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary2RegisterScalarTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary2RegisterScalarTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: (size(21:20)=00 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE((!(((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0111 & inst(24)=0
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0111 & U(24)=0
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalarTesterCase7
    : public VectorBinary2RegisterScalarTester {
 public:
  VectorBinary2RegisterScalarTesterCase7(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterScalarTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterScalarTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000700) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterScalarTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary2RegisterScalarTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary2RegisterScalarTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: (size(21:20)=00 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE((!(((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1000
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(21:20)=00 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1000
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_I16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalarTesterCase8
    : public VectorBinary2RegisterScalarTester {
 public:
  VectorBinary2RegisterScalarTesterCase8(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterScalarTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterScalarTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterScalarTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary2RegisterScalarTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary2RegisterScalarTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: size(21:20)=00 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00000000);

  // safety: Q(24)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1001
//    = {baseline: 'VectorBinary2RegisterScalar_F32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [(inst(21:20)=00 ||
//            inst(21:20)=01) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1001
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_F32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            size(21:20)=01) => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalarTesterCase9
    : public VectorBinary2RegisterScalarTester {
 public:
  VectorBinary2RegisterScalarTesterCase9(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterScalarTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterScalarTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterScalarTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary2RegisterScalarTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary2RegisterScalarTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: (size(21:20)=00 ||
  //       size(21:20)=01) => UNDEFINED
  EXPECT_TRUE((!(((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00300000)  ==
          0x00100000))));

  // safety: Q(24)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1010
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=1010
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalarTesterCase10
    : public VectorBinary2RegisterScalarTester {
 public:
  VectorBinary2RegisterScalarTesterCase10(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterScalarTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterScalarTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000A00) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterScalarTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary2RegisterScalarTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary2RegisterScalarTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: (size(21:20)=00 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE((!(((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1011 & inst(24)=0
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=1011 & U(24)=0
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalarTesterCase11
    : public VectorBinary2RegisterScalarTester {
 public:
  VectorBinary2RegisterScalarTesterCase11(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterScalarTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterScalarTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000B00) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterScalarTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary2RegisterScalarTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary2RegisterScalarTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: (size(21:20)=00 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE((!(((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1100
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(21:20)=00 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1100
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_I16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalarTesterCase12
    : public VectorBinary2RegisterScalarTester {
 public:
  VectorBinary2RegisterScalarTesterCase12(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterScalarTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterScalarTesterCase12
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
  return VectorBinary2RegisterScalarTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary2RegisterScalarTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary2RegisterScalarTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: size(21:20)=00 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00000000);

  // safety: Q(24)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1101
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       safety: [inst(21:20)=00 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_I16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalarTesterCase13
    : public VectorBinary2RegisterScalarTester {
 public:
  VectorBinary2RegisterScalarTesterCase13(const NamedClassDecoder& decoder)
    : VectorBinary2RegisterScalarTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary2RegisterScalarTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;

  // Check pattern restrictions of row.
  // size(21:20)=11
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary2RegisterScalarTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary2RegisterScalarTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary2RegisterScalarTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: size(21:20)=00 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00000000);

  // safety: Q(24)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
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
// inst(11:8)=0000
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VMLA_by_scalar_A1',
//       safety: [inst(21:20)=00 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0000
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_I16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       rule: VMLA_by_scalar_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalar_I16_32Tester_Case0
    : public VectorBinary2RegisterScalarTesterCase0 {
 public:
  VectorBinary2RegisterScalar_I16_32Tester_Case0()
    : VectorBinary2RegisterScalarTesterCase0(
      state_.VectorBinary2RegisterScalar_I16_32_VMLA_by_scalar_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0001
//    = {baseline: 'VectorBinary2RegisterScalar_F32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VMLA_by_scalar_A1',
//       safety: [(inst(21:20)=00 ||
//            inst(21:20)=01) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0001
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_F32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       rule: VMLA_by_scalar_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            size(21:20)=01) => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalar_F32Tester_Case1
    : public VectorBinary2RegisterScalarTesterCase1 {
 public:
  VectorBinary2RegisterScalar_F32Tester_Case1()
    : VectorBinary2RegisterScalarTesterCase1(
      state_.VectorBinary2RegisterScalar_F32_VMLA_by_scalar_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0010
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VMLAL_by_scalar_A2',
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=0010
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       rule: VMLAL_by_scalar_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalar_I16_32LTester_Case2
    : public VectorBinary2RegisterScalarTesterCase2 {
 public:
  VectorBinary2RegisterScalar_I16_32LTester_Case2()
    : VectorBinary2RegisterScalarTesterCase2(
      state_.VectorBinary2RegisterScalar_I16_32L_VMLAL_by_scalar_A2_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0011 & inst(24)=0
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VQDMLAL_A1',
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=0011 & U(24)=0
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       rule: VQDMLAL_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalar_I16_32LTester_Case3
    : public VectorBinary2RegisterScalarTesterCase3 {
 public:
  VectorBinary2RegisterScalar_I16_32LTester_Case3()
    : VectorBinary2RegisterScalarTesterCase3(
      state_.VectorBinary2RegisterScalar_I16_32L_VQDMLAL_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0100
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VMLS_by_scalar_A1',
//       safety: [inst(21:20)=00 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0100
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_I16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       rule: VMLS_by_scalar_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalar_I16_32Tester_Case4
    : public VectorBinary2RegisterScalarTesterCase4 {
 public:
  VectorBinary2RegisterScalar_I16_32Tester_Case4()
    : VectorBinary2RegisterScalarTesterCase4(
      state_.VectorBinary2RegisterScalar_I16_32_VMLS_by_scalar_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0101
//    = {baseline: 'VectorBinary2RegisterScalar_F32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VMLS_by_scalar_A1',
//       safety: [(inst(21:20)=00 ||
//            inst(21:20)=01) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0101
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_F32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       rule: VMLS_by_scalar_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            size(21:20)=01) => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalar_F32Tester_Case5
    : public VectorBinary2RegisterScalarTesterCase5 {
 public:
  VectorBinary2RegisterScalar_F32Tester_Case5()
    : VectorBinary2RegisterScalarTesterCase5(
      state_.VectorBinary2RegisterScalar_F32_VMLS_by_scalar_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0110
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VMLSL_by_scalar_A2',
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=0110
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       rule: VMLSL_by_scalar_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalar_I16_32LTester_Case6
    : public VectorBinary2RegisterScalarTesterCase6 {
 public:
  VectorBinary2RegisterScalar_I16_32LTester_Case6()
    : VectorBinary2RegisterScalarTesterCase6(
      state_.VectorBinary2RegisterScalar_I16_32L_VMLSL_by_scalar_A2_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0111 & inst(24)=0
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VQDMLSL_A1',
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=0111 & U(24)=0
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       rule: VQDMLSL_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalar_I16_32LTester_Case7
    : public VectorBinary2RegisterScalarTesterCase7 {
 public:
  VectorBinary2RegisterScalar_I16_32LTester_Case7()
    : VectorBinary2RegisterScalarTesterCase7(
      state_.VectorBinary2RegisterScalar_I16_32L_VQDMLSL_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1000
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VMUL_by_scalar_A1',
//       safety: [inst(21:20)=00 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1000
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_I16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       rule: VMUL_by_scalar_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalar_I16_32Tester_Case8
    : public VectorBinary2RegisterScalarTesterCase8 {
 public:
  VectorBinary2RegisterScalar_I16_32Tester_Case8()
    : VectorBinary2RegisterScalarTesterCase8(
      state_.VectorBinary2RegisterScalar_I16_32_VMUL_by_scalar_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1001
//    = {baseline: 'VectorBinary2RegisterScalar_F32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VMUL_by_scalar_A1',
//       safety: [(inst(21:20)=00 ||
//            inst(21:20)=01) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1001
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_F32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       rule: VMUL_by_scalar_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            size(21:20)=01) => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalar_F32Tester_Case9
    : public VectorBinary2RegisterScalarTesterCase9 {
 public:
  VectorBinary2RegisterScalar_F32Tester_Case9()
    : VectorBinary2RegisterScalarTesterCase9(
      state_.VectorBinary2RegisterScalar_F32_VMUL_by_scalar_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1010
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VMULL_by_scalar_A2',
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=1010
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       rule: VMULL_by_scalar_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalar_I16_32LTester_Case10
    : public VectorBinary2RegisterScalarTesterCase10 {
 public:
  VectorBinary2RegisterScalar_I16_32LTester_Case10()
    : VectorBinary2RegisterScalarTesterCase10(
      state_.VectorBinary2RegisterScalar_I16_32L_VMULL_by_scalar_A2_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1011 & inst(24)=0
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VQDMULL_A2',
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representative case:
// A(11:8)=1011 & U(24)=0
//    = {Vd: Vd(15:12),
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       rule: VQDMULL_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalar_I16_32LTester_Case11
    : public VectorBinary2RegisterScalarTesterCase11 {
 public:
  VectorBinary2RegisterScalar_I16_32LTester_Case11()
    : VectorBinary2RegisterScalarTesterCase11(
      state_.VectorBinary2RegisterScalar_I16_32L_VQDMULL_A2_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1100
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VQDMULH_A2',
//       safety: [inst(21:20)=00 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1100
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_I16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       rule: VQDMULH_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalar_I16_32Tester_Case12
    : public VectorBinary2RegisterScalarTesterCase12 {
 public:
  VectorBinary2RegisterScalar_I16_32Tester_Case12()
    : VectorBinary2RegisterScalarTesterCase12(
      state_.VectorBinary2RegisterScalar_I16_32_VQDMULH_A2_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1101
//    = {baseline: 'VectorBinary2RegisterScalar_I16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       rule: 'VQRDMULH',
//       safety: [inst(21:20)=00 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1101
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary2RegisterScalar_I16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       rule: VQRDMULH,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary2RegisterScalar_I16_32Tester_Case13
    : public VectorBinary2RegisterScalarTesterCase13 {
 public:
  VectorBinary2RegisterScalar_I16_32Tester_Case13()
    : VectorBinary2RegisterScalarTesterCase13(
      state_.VectorBinary2RegisterScalar_I16_32_VQRDMULH_instance_)
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
// inst(11:8)=0000
//    = {actual: 'VectorBinary2RegisterScalar_I16_32',
//       baseline: 'VectorBinary2RegisterScalar_I16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001q1dssnnnndddd0p0fn1m0mmmm',
//       rule: 'VMLA_by_scalar_A1',
//       safety: [inst(21:20)=00 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0000
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary2RegisterScalar_I16_32,
//       baseline: VectorBinary2RegisterScalar_I16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       pattern: 1111001q1dssnnnndddd0p0fn1m0mmmm,
//       rule: VMLA_by_scalar_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterScalar_I16_32Tester_Case0_TestCase0) {
  VectorBinary2RegisterScalar_I16_32Tester_Case0 tester;
  tester.Test("1111001q1dssnnnndddd0p0fn1m0mmmm");
}

// Neutral case:
// inst(11:8)=0001
//    = {actual: 'VectorBinary2RegisterScalar_F32',
//       baseline: 'VectorBinary2RegisterScalar_F32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001q1dssnnnndddd0p0fn1m0mmmm',
//       rule: 'VMLA_by_scalar_A1',
//       safety: [(inst(21:20)=00 ||
//            inst(21:20)=01) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary2RegisterScalar_F32,
//       baseline: VectorBinary2RegisterScalar_F32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       pattern: 1111001q1dssnnnndddd0p0fn1m0mmmm,
//       rule: VMLA_by_scalar_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            size(21:20)=01) => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterScalar_F32Tester_Case1_TestCase1) {
  VectorBinary2RegisterScalar_F32Tester_Case1 tester;
  tester.Test("1111001q1dssnnnndddd0p0fn1m0mmmm");
}

// Neutral case:
// inst(11:8)=0010
//    = {actual: 'VectorBinary2RegisterScalar_I16_32L',
//       baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001u1dssnnnndddd0p10n1m0mmmm',
//       rule: 'VMLAL_by_scalar_A2',
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0010
//    = {Vd: Vd(15:12),
//       actual: VectorBinary2RegisterScalar_I16_32L,
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       pattern: 1111001u1dssnnnndddd0p10n1m0mmmm,
//       rule: VMLAL_by_scalar_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterScalar_I16_32LTester_Case2_TestCase2) {
  VectorBinary2RegisterScalar_I16_32LTester_Case2 tester;
  tester.Test("1111001u1dssnnnndddd0p10n1m0mmmm");
}

// Neutral case:
// inst(11:8)=0011 & inst(24)=0
//    = {actual: 'VectorBinary2RegisterScalar_I16_32L',
//       baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '111100101dssnnnndddd0p11n1m0mmmm',
//       rule: 'VQDMLAL_A1',
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0011 & U(24)=0
//    = {Vd: Vd(15:12),
//       actual: VectorBinary2RegisterScalar_I16_32L,
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       pattern: 111100101dssnnnndddd0p11n1m0mmmm,
//       rule: VQDMLAL_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterScalar_I16_32LTester_Case3_TestCase3) {
  VectorBinary2RegisterScalar_I16_32LTester_Case3 tester;
  tester.Test("111100101dssnnnndddd0p11n1m0mmmm");
}

// Neutral case:
// inst(11:8)=0100
//    = {actual: 'VectorBinary2RegisterScalar_I16_32',
//       baseline: 'VectorBinary2RegisterScalar_I16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001q1dssnnnndddd0p0fn1m0mmmm',
//       rule: 'VMLS_by_scalar_A1',
//       safety: [inst(21:20)=00 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0100
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary2RegisterScalar_I16_32,
//       baseline: VectorBinary2RegisterScalar_I16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       pattern: 1111001q1dssnnnndddd0p0fn1m0mmmm,
//       rule: VMLS_by_scalar_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterScalar_I16_32Tester_Case4_TestCase4) {
  VectorBinary2RegisterScalar_I16_32Tester_Case4 tester;
  tester.Test("1111001q1dssnnnndddd0p0fn1m0mmmm");
}

// Neutral case:
// inst(11:8)=0101
//    = {actual: 'VectorBinary2RegisterScalar_F32',
//       baseline: 'VectorBinary2RegisterScalar_F32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001q1dssnnnndddd0p0fn1m0mmmm',
//       rule: 'VMLS_by_scalar_A1',
//       safety: [(inst(21:20)=00 ||
//            inst(21:20)=01) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0101
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary2RegisterScalar_F32,
//       baseline: VectorBinary2RegisterScalar_F32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       pattern: 1111001q1dssnnnndddd0p0fn1m0mmmm,
//       rule: VMLS_by_scalar_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            size(21:20)=01) => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterScalar_F32Tester_Case5_TestCase5) {
  VectorBinary2RegisterScalar_F32Tester_Case5 tester;
  tester.Test("1111001q1dssnnnndddd0p0fn1m0mmmm");
}

// Neutral case:
// inst(11:8)=0110
//    = {actual: 'VectorBinary2RegisterScalar_I16_32L',
//       baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001u1dssnnnndddd0p10n1m0mmmm',
//       rule: 'VMLSL_by_scalar_A2',
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0110
//    = {Vd: Vd(15:12),
//       actual: VectorBinary2RegisterScalar_I16_32L,
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       pattern: 1111001u1dssnnnndddd0p10n1m0mmmm,
//       rule: VMLSL_by_scalar_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterScalar_I16_32LTester_Case6_TestCase6) {
  VectorBinary2RegisterScalar_I16_32LTester_Case6 tester;
  tester.Test("1111001u1dssnnnndddd0p10n1m0mmmm");
}

// Neutral case:
// inst(11:8)=0111 & inst(24)=0
//    = {actual: 'VectorBinary2RegisterScalar_I16_32L',
//       baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '111100101dssnnnndddd0p11n1m0mmmm',
//       rule: 'VQDMLSL_A1',
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=0111 & U(24)=0
//    = {Vd: Vd(15:12),
//       actual: VectorBinary2RegisterScalar_I16_32L,
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       pattern: 111100101dssnnnndddd0p11n1m0mmmm,
//       rule: VQDMLSL_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterScalar_I16_32LTester_Case7_TestCase7) {
  VectorBinary2RegisterScalar_I16_32LTester_Case7 tester;
  tester.Test("111100101dssnnnndddd0p11n1m0mmmm");
}

// Neutral case:
// inst(11:8)=1000
//    = {actual: 'VectorBinary2RegisterScalar_I16_32',
//       baseline: 'VectorBinary2RegisterScalar_I16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001q1dssnnnndddd100fn1m0mmmm',
//       rule: 'VMUL_by_scalar_A1',
//       safety: [inst(21:20)=00 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1000
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary2RegisterScalar_I16_32,
//       baseline: VectorBinary2RegisterScalar_I16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       pattern: 1111001q1dssnnnndddd100fn1m0mmmm,
//       rule: VMUL_by_scalar_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterScalar_I16_32Tester_Case8_TestCase8) {
  VectorBinary2RegisterScalar_I16_32Tester_Case8 tester;
  tester.Test("1111001q1dssnnnndddd100fn1m0mmmm");
}

// Neutral case:
// inst(11:8)=1001
//    = {actual: 'VectorBinary2RegisterScalar_F32',
//       baseline: 'VectorBinary2RegisterScalar_F32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001q1dssnnnndddd100fn1m0mmmm',
//       rule: 'VMUL_by_scalar_A1',
//       safety: [(inst(21:20)=00 ||
//            inst(21:20)=01) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1001
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary2RegisterScalar_F32,
//       baseline: VectorBinary2RegisterScalar_F32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       pattern: 1111001q1dssnnnndddd100fn1m0mmmm,
//       rule: VMUL_by_scalar_A1,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            size(21:20)=01) => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterScalar_F32Tester_Case9_TestCase9) {
  VectorBinary2RegisterScalar_F32Tester_Case9 tester;
  tester.Test("1111001q1dssnnnndddd100fn1m0mmmm");
}

// Neutral case:
// inst(11:8)=1010
//    = {actual: 'VectorBinary2RegisterScalar_I16_32L',
//       baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001u1dssnnnndddd1010n1m0mmmm',
//       rule: 'VMULL_by_scalar_A2',
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=1010
//    = {Vd: Vd(15:12),
//       actual: VectorBinary2RegisterScalar_I16_32L,
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       pattern: 1111001u1dssnnnndddd1010n1m0mmmm,
//       rule: VMULL_by_scalar_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterScalar_I16_32LTester_Case10_TestCase10) {
  VectorBinary2RegisterScalar_I16_32LTester_Case10 tester;
  tester.Test("1111001u1dssnnnndddd1010n1m0mmmm");
}

// Neutral case:
// inst(11:8)=1011 & inst(24)=0
//    = {actual: 'VectorBinary2RegisterScalar_I16_32L',
//       baseline: 'VectorBinary2RegisterScalar_I16_32L',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '111100101dssnnnndddd1011n1m0mmmm',
//       rule: 'VQDMULL_A2',
//       safety: [(inst(21:20)=00 ||
//            inst(15:12)(0)=1) => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR]}
//
// Representaive case:
// A(11:8)=1011 & U(24)=0
//    = {Vd: Vd(15:12),
//       actual: VectorBinary2RegisterScalar_I16_32L,
//       baseline: VectorBinary2RegisterScalar_I16_32L,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [size(21:20), Vd(15:12)],
//       pattern: 111100101dssnnnndddd1011n1m0mmmm,
//       rule: VQDMULL_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         (size(21:20)=00 ||
//            Vd(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterScalar_I16_32LTester_Case11_TestCase11) {
  VectorBinary2RegisterScalar_I16_32LTester_Case11 tester;
  tester.Test("111100101dssnnnndddd1011n1m0mmmm");
}

// Neutral case:
// inst(11:8)=1100
//    = {actual: 'VectorBinary2RegisterScalar_I16_32',
//       baseline: 'VectorBinary2RegisterScalar_I16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001q1dssnnnndddd1100n1m0mmmm',
//       rule: 'VQDMULH_A2',
//       safety: [inst(21:20)=00 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1100
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary2RegisterScalar_I16_32,
//       baseline: VectorBinary2RegisterScalar_I16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       pattern: 1111001q1dssnnnndddd1100n1m0mmmm,
//       rule: VQDMULH_A2,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterScalar_I16_32Tester_Case12_TestCase12) {
  VectorBinary2RegisterScalar_I16_32Tester_Case12 tester;
  tester.Test("1111001q1dssnnnndddd1100n1m0mmmm");
}

// Neutral case:
// inst(11:8)=1101
//    = {actual: 'VectorBinary2RegisterScalar_I16_32',
//       baseline: 'VectorBinary2RegisterScalar_I16_32',
//       constraints: & inst(21:20)=~11 ,
//       defs: {},
//       pattern: '1111001q1dssnnnndddd1101n1m0mmmm',
//       rule: 'VQRDMULH',
//       safety: [inst(21:20)=00 => UNDEFINED,
//         inst(21:20)=11 => DECODER_ERROR,
//         inst(24)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101
//    = {Q: Q(24),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary2RegisterScalar_I16_32,
//       baseline: VectorBinary2RegisterScalar_I16_32,
//       constraints: & size(21:20)=~11 ,
//       defs: {},
//       fields: [Q(24), size(21:20), Vn(19:16), Vd(15:12)],
//       pattern: 1111001q1dssnnnndddd1101n1m0mmmm,
//       rule: VQRDMULH,
//       safety: [size(21:20)=11 => DECODER_ERROR,
//         size(21:20)=00 => UNDEFINED,
//         Q(24)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary2RegisterScalar_I16_32Tester_Case13_TestCase13) {
  VectorBinary2RegisterScalar_I16_32Tester_Case13 tester;
  tester.Test("1111001q1dssnnnndddd1101n1m0mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
