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
// inst(11:8)=0000 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0000 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase0
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase0(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000000) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0000 & inst(4)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0000 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase1
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase1(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000000) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase2
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase2(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=0 & inst(21:20)=00
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=00
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase3
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase3(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~00
  if ((inst.Bits() & 0x00300000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=0 & inst(21:20)=01
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=01
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase4
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase4(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~01
  if ((inst.Bits() & 0x00300000)  !=
          0x00100000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=0 & inst(21:20)=10
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=10
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase5
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase5(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~10
  if ((inst.Bits() & 0x00300000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=0 & inst(21:20)=11
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=11
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase6
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase6(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~11
  if ((inst.Bits() & 0x00300000)  !=
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=1 & inst(21:20)=00
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=00
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase7
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase7(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~00
  if ((inst.Bits() & 0x00300000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=1 & inst(21:20)=01
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=01
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase8
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase8(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~01
  if ((inst.Bits() & 0x00300000)  !=
          0x00100000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=1 & inst(21:20)=10
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=10
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase9
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase9(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~10
  if ((inst.Bits() & 0x00300000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=1 & inst(21:20)=11
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=11
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase10
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase10(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000100) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~11
  if ((inst.Bits() & 0x00300000)  !=
          0x00300000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0010 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0010 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase11
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase11(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000200) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0010 & inst(4)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0010 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase12
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase12(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000200) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0011 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0011 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase13
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase13(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000300) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0011 & inst(4)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0011 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase14
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase14(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000300) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0100 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0100 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase15
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase15(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000400) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase15
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0100 & inst(4)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0100 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase16
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase16(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000400) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0101 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0101 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase17
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase17(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000500) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase17
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0101 & inst(4)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0101 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase18
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase18(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000500) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase18
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0110 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0110 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase19
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase19(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000600) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase19
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0110 & inst(4)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0110 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase20
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase20(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase20
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000600) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase20
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0111 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0111 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase21
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase21(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase21
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000700) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase21
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=0111 & inst(4)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0111 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase22
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase22(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase22
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~0111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000700) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase22
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1000 & inst(4)=0 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1000 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase23
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase23(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase23
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase23
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1000 & inst(4)=0 & inst(24)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1000 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthTesterCase24
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase24(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase24
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase24
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1000 & inst(4)=1 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1000 & B(4)=1 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase25
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase25(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase25
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase25
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1000 & inst(4)=1 & inst(24)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1000 & B(4)=1 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase26
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase26(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase26
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase26
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1001 & inst(4)=0 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1001 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase27
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase27(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase27
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase27
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1001 & inst(4)=0 & inst(24)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1001 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase28
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase28(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase28
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase28
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1001 & inst(4)=1 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1001 & B(4)=1 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase29
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase29(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase29
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase29
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1001 & inst(4)=1 & inst(24)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8P',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=~00 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1001 & B(4)=1 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=~00 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase30
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase30(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase30
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase30
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(21:20)=~00 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  ==
          0x00000000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1010 & inst(4)=0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {baseline: 'VectorBinary3RegisterSameLengthDI',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1010 & B(4)=0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase31
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase31(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase31
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000A00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase31
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: Q(6)=1 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000040)  !=
          0x00000040);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1010 & inst(4)=1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {baseline: 'VectorBinary3RegisterSameLengthDI',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1010 & B(4)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase32
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase32(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase32
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000A00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase32
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: Q(6)=1 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000040)  !=
          0x00000040);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1011 & inst(4)=0 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI16_32',
//       constraints: ,
//       defs: {},
//       safety: [(inst(21:20)=11 ||
//            inst(21:20)=00) => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1011 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         (size(21:20)=11 ||
//            size(21:20)=00) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase33
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase33(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase33
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000B00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase33
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: (size(21:20)=11 ||
  //       size(21:20)=00) => UNDEFINED
  EXPECT_TRUE((!(((inst.Bits() & 0x00300000)  ==
          0x00300000) ||
       ((inst.Bits() & 0x00300000)  ==
          0x00000000))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1011 & inst(4)=0 & inst(24)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI16_32',
//       constraints: ,
//       defs: {},
//       safety: [(inst(21:20)=11 ||
//            inst(21:20)=00) => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1011 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         (size(21:20)=11 ||
//            size(21:20)=00) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase34
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase34(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase34
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000B00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase34
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: (size(21:20)=11 ||
  //       size(21:20)=00) => UNDEFINED
  EXPECT_TRUE((!(((inst.Bits() & 0x00300000)  ==
          0x00300000) ||
       ((inst.Bits() & 0x00300000)  ==
          0x00000000))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1011 & inst(4)=1 & inst(24)=0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {baseline: 'VectorBinary3RegisterSameLengthDI',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1011 & B(4)=1 & U(24)=0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase35
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase35(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase35
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000B00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase35
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(21:20)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00300000)  !=
          0x00300000);

  // safety: Q(6)=1 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x00000040)  !=
          0x00000040);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1100 & inst(4)=1 & inst(24)=0 & inst(21:20)=0x & inst(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1100 & B(4)=1 & U(24)=0 & C(21:20)=0x & $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase36
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase36(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase36
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000C00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x00100000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase36
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1100 & inst(4)=1 & inst(24)=0 & inst(21:20)=1x & inst(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1100 & B(4)=1 & U(24)=0 & C(21:20)=1x & $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase37
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase37(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase37
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000C00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // $pattern(31:0)=~xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x00100000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase37
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1101 & inst(4)=0 & inst(24)=0 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase38
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase38(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase38
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase38
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1101 & inst(4)=0 & inst(24)=0 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101 & B(4)=0 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase39
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase39(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase39
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase39
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1101 & inst(4)=0 & inst(24)=1 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32P',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 ||
//            inst(6)=1 => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase40
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase40(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase40
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase40
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(0)=1 ||
  //       Q(6)=1 => UNDEFINED
  EXPECT_TRUE(!(((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  ==
          0x00000001) ||
       ((inst.Bits() & 0x00000040)  ==
          0x00000040)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1101 & inst(4)=0 & inst(24)=1 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase41
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase41(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase41
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase41
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1101 & inst(4)=1 & inst(24)=0 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101 & B(4)=1 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase42
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase42(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase42
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase42
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1101 & inst(4)=1 & inst(24)=0 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101 & B(4)=1 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase43
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase43(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase43
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase43
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1101 & inst(4)=1 & inst(24)=1 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101 & B(4)=1 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase44
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase44(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase44
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase44
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1110 & inst(4)=0 & inst(24)=0 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1110 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase45
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase45(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase45
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000E00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase45
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1110 & inst(4)=0 & inst(24)=1 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1110 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase46
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase46(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase46
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000E00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase46
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1110 & inst(4)=0 & inst(24)=1 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1110 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase47
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase47(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase47
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000E00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase47
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1110 & inst(4)=1 & inst(24)=1 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1110 & B(4)=1 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase48
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase48(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase48
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000E00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase48
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1110 & inst(4)=1 & inst(24)=1 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1110 & B(4)=1 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase49
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase49(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase49
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000E00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase49
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1111 & inst(4)=0 & inst(24)=0 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1111 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase50
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase50(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase50
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase50
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1111 & inst(4)=0 & inst(24)=0 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1111 & B(4)=0 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase51
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase51(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase51
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase51
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1111 & inst(4)=0 & inst(24)=1 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32P',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 ||
//            inst(6)=1 => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1111 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase52
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase52(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase52
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase52
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(0)=1 ||
  //       Q(6)=1 => UNDEFINED
  EXPECT_TRUE(!(((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  ==
          0x00000001) ||
       ((inst.Bits() & 0x00000040)  ==
          0x00000040)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1111 & inst(4)=0 & inst(24)=1 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32P',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 ||
//            inst(6)=1 => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1111 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase53
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase53(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase53
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;
  // B(4)=~0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // U(24)=~1
  if ((inst.Bits() & 0x01000000)  !=
          0x01000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase53
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(0)=1 ||
  //       Q(6)=1 => UNDEFINED
  EXPECT_TRUE(!(((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  ==
          0x00000001) ||
       ((inst.Bits() & 0x00000040)  ==
          0x00000040)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1111 & inst(4)=1 & inst(24)=0 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1111 & B(4)=1 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase54
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase54(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase54
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~0x
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase54
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(11:8)=1111 & inst(4)=1 & inst(24)=0 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1111 & B(4)=1 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthTesterCase55
    : public VectorBinary3RegisterSameLengthTester {
 public:
  VectorBinary3RegisterSameLengthTesterCase55(const NamedClassDecoder& decoder)
    : VectorBinary3RegisterSameLengthTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VectorBinary3RegisterSameLengthTesterCase55
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(11:8)=~1111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;
  // B(4)=~1
  if ((inst.Bits() & 0x00000010)  !=
          0x00000010) return false;
  // U(24)=~0
  if ((inst.Bits() & 0x01000000)  !=
          0x00000000) return false;
  // C(21:20)=~1x
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorBinary3RegisterSameLengthTester::
      PassesParsePreconditions(inst, decoder);
}

bool VectorBinary3RegisterSameLengthTesterCase55
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VectorBinary3RegisterSameLengthTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vn(0)=1 ||
  //       Vd(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(0)=1 => UNDEFINED
  EXPECT_TRUE((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(11:8)=0000 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VHADD',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0000 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VHADD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case0
    : public VectorBinary3RegisterSameLengthTesterCase0 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case0()
    : VectorBinary3RegisterSameLengthTesterCase0(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VHADD_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0000 & inst(4)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VQADD',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0000 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VQADD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case1
    : public VectorBinary3RegisterSameLengthTesterCase1 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case1()
    : VectorBinary3RegisterSameLengthTesterCase1(
      state_.VectorBinary3RegisterSameLengthDQ_VQADD_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0001 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VRHADD',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0001 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VRHADD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case2
    : public VectorBinary3RegisterSameLengthTesterCase2 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case2()
    : VectorBinary3RegisterSameLengthTesterCase2(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VRHADD_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=0 & inst(21:20)=00
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VAND_register',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=00
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VAND_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case3
    : public VectorBinary3RegisterSameLengthTesterCase3 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case3()
    : VectorBinary3RegisterSameLengthTesterCase3(
      state_.VectorBinary3RegisterSameLengthDQ_VAND_register_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=0 & inst(21:20)=01
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VBIC_register',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=01
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VBIC_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case4
    : public VectorBinary3RegisterSameLengthTesterCase4 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case4()
    : VectorBinary3RegisterSameLengthTesterCase4(
      state_.VectorBinary3RegisterSameLengthDQ_VBIC_register_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=0 & inst(21:20)=10
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VORR_register_or_VMOV_register_A1',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=10
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VORR_register_or_VMOV_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case5
    : public VectorBinary3RegisterSameLengthTesterCase5 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case5()
    : VectorBinary3RegisterSameLengthTesterCase5(
      state_.VectorBinary3RegisterSameLengthDQ_VORR_register_or_VMOV_register_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=0 & inst(21:20)=11
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VORN_register',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=11
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VORN_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case6
    : public VectorBinary3RegisterSameLengthTesterCase6 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case6()
    : VectorBinary3RegisterSameLengthTesterCase6(
      state_.VectorBinary3RegisterSameLengthDQ_VORN_register_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=1 & inst(21:20)=00
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VEOR',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=00
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VEOR,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case7
    : public VectorBinary3RegisterSameLengthTesterCase7 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case7()
    : VectorBinary3RegisterSameLengthTesterCase7(
      state_.VectorBinary3RegisterSameLengthDQ_VEOR_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=1 & inst(21:20)=01
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VBSL',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=01
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VBSL,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case8
    : public VectorBinary3RegisterSameLengthTesterCase8 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case8()
    : VectorBinary3RegisterSameLengthTesterCase8(
      state_.VectorBinary3RegisterSameLengthDQ_VBSL_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=1 & inst(21:20)=10
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VBIT',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=10
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VBIT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case9
    : public VectorBinary3RegisterSameLengthTesterCase9 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case9()
    : VectorBinary3RegisterSameLengthTesterCase9(
      state_.VectorBinary3RegisterSameLengthDQ_VBIT_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=1 & inst(21:20)=11
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VBIF',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=11
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VBIF,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case10
    : public VectorBinary3RegisterSameLengthTesterCase10 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case10()
    : VectorBinary3RegisterSameLengthTesterCase10(
      state_.VectorBinary3RegisterSameLengthDQ_VBIF_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0010 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VHSUB',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0010 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VHSUB,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case11
    : public VectorBinary3RegisterSameLengthTesterCase11 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case11()
    : VectorBinary3RegisterSameLengthTesterCase11(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VHSUB_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0010 & inst(4)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VQSUB',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0010 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VQSUB,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case12
    : public VectorBinary3RegisterSameLengthTesterCase12 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case12()
    : VectorBinary3RegisterSameLengthTesterCase12(
      state_.VectorBinary3RegisterSameLengthDQ_VQSUB_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0011 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VCGT_register_A1',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0011 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VCGT_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case13
    : public VectorBinary3RegisterSameLengthTesterCase13 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case13()
    : VectorBinary3RegisterSameLengthTesterCase13(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VCGT_register_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0011 & inst(4)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VCGE_register_A1',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0011 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VCGE_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case14
    : public VectorBinary3RegisterSameLengthTesterCase14 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case14()
    : VectorBinary3RegisterSameLengthTesterCase14(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VCGE_register_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0100 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VSHL_register',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0100 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VSHL_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case15
    : public VectorBinary3RegisterSameLengthTesterCase15 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case15()
    : VectorBinary3RegisterSameLengthTesterCase15(
      state_.VectorBinary3RegisterSameLengthDQ_VSHL_register_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0100 & inst(4)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VQSHL_register',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0100 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VQSHL_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case16
    : public VectorBinary3RegisterSameLengthTesterCase16 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case16()
    : VectorBinary3RegisterSameLengthTesterCase16(
      state_.VectorBinary3RegisterSameLengthDQ_VQSHL_register_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0101 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VRSHL',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0101 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VRSHL,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case17
    : public VectorBinary3RegisterSameLengthTesterCase17 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case17()
    : VectorBinary3RegisterSameLengthTesterCase17(
      state_.VectorBinary3RegisterSameLengthDQ_VRSHL_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0101 & inst(4)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VQRSHL',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0101 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VQRSHL,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case18
    : public VectorBinary3RegisterSameLengthTesterCase18 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case18()
    : VectorBinary3RegisterSameLengthTesterCase18(
      state_.VectorBinary3RegisterSameLengthDQ_VQRSHL_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0110 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VMAX',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0110 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VMAX,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case19
    : public VectorBinary3RegisterSameLengthTesterCase19 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case19()
    : VectorBinary3RegisterSameLengthTesterCase19(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VMAX_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0110 & inst(4)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VMIN',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0110 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VMIN,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case20
    : public VectorBinary3RegisterSameLengthTesterCase20 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case20()
    : VectorBinary3RegisterSameLengthTesterCase20(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VMIN_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0111 & inst(4)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VABD',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0111 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VABD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case21
    : public VectorBinary3RegisterSameLengthTesterCase21 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case21()
    : VectorBinary3RegisterSameLengthTesterCase21(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VABD_instance_)
  {}
};

// Neutral case:
// inst(11:8)=0111 & inst(4)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VABA',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=0111 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VABA,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case22
    : public VectorBinary3RegisterSameLengthTesterCase22 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case22()
    : VectorBinary3RegisterSameLengthTesterCase22(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VABA_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1000 & inst(4)=0 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VADD_integer',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1000 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VADD_integer,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case23
    : public VectorBinary3RegisterSameLengthTesterCase23 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case23()
    : VectorBinary3RegisterSameLengthTesterCase23(
      state_.VectorBinary3RegisterSameLengthDQ_VADD_integer_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1000 & inst(4)=0 & inst(24)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       rule: 'VSUB_integer',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1000 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       rule: VSUB_integer,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
class VectorBinary3RegisterSameLengthDQTester_Case24
    : public VectorBinary3RegisterSameLengthTesterCase24 {
 public:
  VectorBinary3RegisterSameLengthDQTester_Case24()
    : VectorBinary3RegisterSameLengthTesterCase24(
      state_.VectorBinary3RegisterSameLengthDQ_VSUB_integer_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1000 & inst(4)=1 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VTST',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1000 & B(4)=1 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VTST,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case25
    : public VectorBinary3RegisterSameLengthTesterCase25 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case25()
    : VectorBinary3RegisterSameLengthTesterCase25(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VTST_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1000 & inst(4)=1 & inst(24)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VCEQ_register_A1',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1000 & B(4)=1 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VCEQ_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case26
    : public VectorBinary3RegisterSameLengthTesterCase26 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case26()
    : VectorBinary3RegisterSameLengthTesterCase26(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VCEQ_register_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1001 & inst(4)=0 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VMLA_integer_A1',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1001 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VMLA_integer_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case27
    : public VectorBinary3RegisterSameLengthTesterCase27 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case27()
    : VectorBinary3RegisterSameLengthTesterCase27(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VMLA_integer_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1001 & inst(4)=0 & inst(24)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VMLS_integer_A1',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1001 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VMLS_integer_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case28
    : public VectorBinary3RegisterSameLengthTesterCase28 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case28()
    : VectorBinary3RegisterSameLengthTesterCase28(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VMLS_integer_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1001 & inst(4)=1 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VMUL_integer_A1',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1001 & B(4)=1 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VMUL_integer_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case29
    : public VectorBinary3RegisterSameLengthTesterCase29 {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case29()
    : VectorBinary3RegisterSameLengthTesterCase29(
      state_.VectorBinary3RegisterSameLengthDQI8_16_32_VMUL_integer_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1001 & inst(4)=1 & inst(24)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI8P',
//       constraints: ,
//       defs: {},
//       rule: 'VMUL_polynomial_A1',
//       safety: [inst(21:20)=~00 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1001 & B(4)=1 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI8P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VMUL_polynomial_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=~00 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI8PTester_Case30
    : public VectorBinary3RegisterSameLengthTesterCase30 {
 public:
  VectorBinary3RegisterSameLengthDQI8PTester_Case30()
    : VectorBinary3RegisterSameLengthTesterCase30(
      state_.VectorBinary3RegisterSameLengthDQI8P_VMUL_polynomial_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1010 & inst(4)=0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {baseline: 'VectorBinary3RegisterSameLengthDI',
//       constraints: ,
//       defs: {},
//       rule: 'VPMAX',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 => UNDEFINED]}
//
// Representative case:
// A(11:8)=1010 & B(4)=0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       rule: VPMAX,
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDITester_Case31
    : public VectorBinary3RegisterSameLengthTesterCase31 {
 public:
  VectorBinary3RegisterSameLengthDITester_Case31()
    : VectorBinary3RegisterSameLengthTesterCase31(
      state_.VectorBinary3RegisterSameLengthDI_VPMAX_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1010 & inst(4)=1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {baseline: 'VectorBinary3RegisterSameLengthDI',
//       constraints: ,
//       defs: {},
//       rule: 'VPMIN',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 => UNDEFINED]}
//
// Representative case:
// A(11:8)=1010 & B(4)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       rule: VPMIN,
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDITester_Case32
    : public VectorBinary3RegisterSameLengthTesterCase32 {
 public:
  VectorBinary3RegisterSameLengthDITester_Case32()
    : VectorBinary3RegisterSameLengthTesterCase32(
      state_.VectorBinary3RegisterSameLengthDI_VPMIN_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1011 & inst(4)=0 & inst(24)=0
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VQDMULH_A1',
//       safety: [(inst(21:20)=11 ||
//            inst(21:20)=00) => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1011 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VQDMULH_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         (size(21:20)=11 ||
//            size(21:20)=00) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI16_32Tester_Case33
    : public VectorBinary3RegisterSameLengthTesterCase33 {
 public:
  VectorBinary3RegisterSameLengthDQI16_32Tester_Case33()
    : VectorBinary3RegisterSameLengthTesterCase33(
      state_.VectorBinary3RegisterSameLengthDQI16_32_VQDMULH_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1011 & inst(4)=0 & inst(24)=1
//    = {baseline: 'VectorBinary3RegisterSameLengthDQI16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VQRDMULH_A1',
//       safety: [(inst(21:20)=11 ||
//            inst(21:20)=00) => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1011 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLengthDQI16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VQRDMULH_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         (size(21:20)=11 ||
//            size(21:20)=00) => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDQI16_32Tester_Case34
    : public VectorBinary3RegisterSameLengthTesterCase34 {
 public:
  VectorBinary3RegisterSameLengthDQI16_32Tester_Case34()
    : VectorBinary3RegisterSameLengthTesterCase34(
      state_.VectorBinary3RegisterSameLengthDQI16_32_VQRDMULH_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1011 & inst(4)=1 & inst(24)=0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {baseline: 'VectorBinary3RegisterSameLengthDI',
//       constraints: ,
//       defs: {},
//       rule: 'VPADD_integer',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 => UNDEFINED]}
//
// Representative case:
// A(11:8)=1011 & B(4)=1 & U(24)=0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       rule: VPADD_integer,
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLengthDITester_Case35
    : public VectorBinary3RegisterSameLengthTesterCase35 {
 public:
  VectorBinary3RegisterSameLengthDITester_Case35()
    : VectorBinary3RegisterSameLengthTesterCase35(
      state_.VectorBinary3RegisterSameLengthDI_VPADD_integer_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1100 & inst(4)=1 & inst(24)=0 & inst(21:20)=0x & inst(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VFMA_A1',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1100 & B(4)=1 & U(24)=0 & C(21:20)=0x & $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VFMA_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case36
    : public VectorBinary3RegisterSameLengthTesterCase36 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case36()
    : VectorBinary3RegisterSameLengthTesterCase36(
      state_.VectorBinary3RegisterSameLength32_DQ_VFMA_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1100 & inst(4)=1 & inst(24)=0 & inst(21:20)=1x & inst(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VFMS_A1',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1100 & B(4)=1 & U(24)=0 & C(21:20)=1x & $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VFMS_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case37
    : public VectorBinary3RegisterSameLengthTesterCase37 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case37()
    : VectorBinary3RegisterSameLengthTesterCase37(
      state_.VectorBinary3RegisterSameLength32_DQ_VFMS_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1101 & inst(4)=0 & inst(24)=0 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VADD_floating_point_A1',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1101 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VADD_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case38
    : public VectorBinary3RegisterSameLengthTesterCase38 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case38()
    : VectorBinary3RegisterSameLengthTesterCase38(
      state_.VectorBinary3RegisterSameLength32_DQ_VADD_floating_point_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1101 & inst(4)=0 & inst(24)=0 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VSUB_floating_point_A1',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1101 & B(4)=0 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VSUB_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case39
    : public VectorBinary3RegisterSameLengthTesterCase39 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case39()
    : VectorBinary3RegisterSameLengthTesterCase39(
      state_.VectorBinary3RegisterSameLength32_DQ_VSUB_floating_point_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1101 & inst(4)=0 & inst(24)=1 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32P',
//       constraints: ,
//       defs: {},
//       rule: 'VPADD_floating_point',
//       safety: [inst(21:20)(0)=1 ||
//            inst(6)=1 => UNDEFINED]}
//
// Representative case:
// A(11:8)=1101 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       rule: VPADD_floating_point,
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32PTester_Case40
    : public VectorBinary3RegisterSameLengthTesterCase40 {
 public:
  VectorBinary3RegisterSameLength32PTester_Case40()
    : VectorBinary3RegisterSameLengthTesterCase40(
      state_.VectorBinary3RegisterSameLength32P_VPADD_floating_point_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1101 & inst(4)=0 & inst(24)=1 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VABD_floating_point',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1101 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VABD_floating_point,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case41
    : public VectorBinary3RegisterSameLengthTesterCase41 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case41()
    : VectorBinary3RegisterSameLengthTesterCase41(
      state_.VectorBinary3RegisterSameLength32_DQ_VABD_floating_point_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1101 & inst(4)=1 & inst(24)=0 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VMLA_floating_point_A1',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1101 & B(4)=1 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VMLA_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case42
    : public VectorBinary3RegisterSameLengthTesterCase42 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case42()
    : VectorBinary3RegisterSameLengthTesterCase42(
      state_.VectorBinary3RegisterSameLength32_DQ_VMLA_floating_point_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1101 & inst(4)=1 & inst(24)=0 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VMLS_floating_point_A1',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1101 & B(4)=1 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VMLS_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case43
    : public VectorBinary3RegisterSameLengthTesterCase43 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case43()
    : VectorBinary3RegisterSameLengthTesterCase43(
      state_.VectorBinary3RegisterSameLength32_DQ_VMLS_floating_point_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1101 & inst(4)=1 & inst(24)=1 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VMUL_floating_point_A1',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1101 & B(4)=1 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VMUL_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case44
    : public VectorBinary3RegisterSameLengthTesterCase44 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case44()
    : VectorBinary3RegisterSameLengthTesterCase44(
      state_.VectorBinary3RegisterSameLength32_DQ_VMUL_floating_point_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1110 & inst(4)=0 & inst(24)=0 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VCEQ_register_A2',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1110 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VCEQ_register_A2,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case45
    : public VectorBinary3RegisterSameLengthTesterCase45 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case45()
    : VectorBinary3RegisterSameLengthTesterCase45(
      state_.VectorBinary3RegisterSameLength32_DQ_VCEQ_register_A2_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1110 & inst(4)=0 & inst(24)=1 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VCGE_register_A2',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1110 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VCGE_register_A2,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case46
    : public VectorBinary3RegisterSameLengthTesterCase46 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case46()
    : VectorBinary3RegisterSameLengthTesterCase46(
      state_.VectorBinary3RegisterSameLength32_DQ_VCGE_register_A2_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1110 & inst(4)=0 & inst(24)=1 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VCGT_register_A2',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1110 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VCGT_register_A2,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case47
    : public VectorBinary3RegisterSameLengthTesterCase47 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case47()
    : VectorBinary3RegisterSameLengthTesterCase47(
      state_.VectorBinary3RegisterSameLength32_DQ_VCGT_register_A2_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1110 & inst(4)=1 & inst(24)=1 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VACGE',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1110 & B(4)=1 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VACGE,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case48
    : public VectorBinary3RegisterSameLengthTesterCase48 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case48()
    : VectorBinary3RegisterSameLengthTesterCase48(
      state_.VectorBinary3RegisterSameLength32_DQ_VACGE_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1110 & inst(4)=1 & inst(24)=1 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VACGT',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1110 & B(4)=1 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VACGT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case49
    : public VectorBinary3RegisterSameLengthTesterCase49 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case49()
    : VectorBinary3RegisterSameLengthTesterCase49(
      state_.VectorBinary3RegisterSameLength32_DQ_VACGT_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1111 & inst(4)=0 & inst(24)=0 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VMAX_floating_point',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1111 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VMAX_floating_point,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case50
    : public VectorBinary3RegisterSameLengthTesterCase50 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case50()
    : VectorBinary3RegisterSameLengthTesterCase50(
      state_.VectorBinary3RegisterSameLength32_DQ_VMAX_floating_point_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1111 & inst(4)=0 & inst(24)=0 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VMIN_floating_point',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1111 & B(4)=0 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VMIN_floating_point,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case51
    : public VectorBinary3RegisterSameLengthTesterCase51 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case51()
    : VectorBinary3RegisterSameLengthTesterCase51(
      state_.VectorBinary3RegisterSameLength32_DQ_VMIN_floating_point_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1111 & inst(4)=0 & inst(24)=1 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32P',
//       constraints: ,
//       defs: {},
//       rule: 'VPMAX',
//       safety: [inst(21:20)(0)=1 ||
//            inst(6)=1 => UNDEFINED]}
//
// Representative case:
// A(11:8)=1111 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       rule: VPMAX,
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32PTester_Case52
    : public VectorBinary3RegisterSameLengthTesterCase52 {
 public:
  VectorBinary3RegisterSameLength32PTester_Case52()
    : VectorBinary3RegisterSameLengthTesterCase52(
      state_.VectorBinary3RegisterSameLength32P_VPMAX_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1111 & inst(4)=0 & inst(24)=1 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32P',
//       constraints: ,
//       defs: {},
//       rule: 'VPMIN',
//       safety: [inst(21:20)(0)=1 ||
//            inst(6)=1 => UNDEFINED]}
//
// Representative case:
// A(11:8)=1111 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       rule: VPMIN,
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32PTester_Case53
    : public VectorBinary3RegisterSameLengthTesterCase53 {
 public:
  VectorBinary3RegisterSameLength32PTester_Case53()
    : VectorBinary3RegisterSameLengthTesterCase53(
      state_.VectorBinary3RegisterSameLength32P_VPMIN_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1111 & inst(4)=1 & inst(24)=0 & inst(21:20)=0x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VRECPS',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1111 & B(4)=1 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VRECPS,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case54
    : public VectorBinary3RegisterSameLengthTesterCase54 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case54()
    : VectorBinary3RegisterSameLengthTesterCase54(
      state_.VectorBinary3RegisterSameLength32_DQ_VRECPS_instance_)
  {}
};

// Neutral case:
// inst(11:8)=1111 & inst(4)=1 & inst(24)=0 & inst(21:20)=1x
//    = {baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       rule: 'VRSQRTS',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(11:8)=1111 & B(4)=1 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       rule: VRSQRTS,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
class VectorBinary3RegisterSameLength32_DQTester_Case55
    : public VectorBinary3RegisterSameLengthTesterCase55 {
 public:
  VectorBinary3RegisterSameLength32_DQTester_Case55()
    : VectorBinary3RegisterSameLengthTesterCase55(
      state_.VectorBinary3RegisterSameLength32_DQ_VRSQRTS_instance_)
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
// inst(11:8)=0000 & inst(4)=0
//    = {actual: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd0000nqm0mmmm',
//       rule: 'VHADD',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0000 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI8_16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd0000nqm0mmmm,
//       rule: VHADD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case0_TestCase0) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case0 tester;
  tester.Test("1111001u0dssnnnndddd0000nqm0mmmm");
}

// Neutral case:
// inst(11:8)=0000 & inst(4)=1
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd0000nqm1mmmm',
//       rule: 'VQADD',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0000 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd0000nqm1mmmm,
//       rule: VQADD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case1_TestCase1) {
  VectorBinary3RegisterSameLengthDQTester_Case1 tester;
  tester.Test("1111001u0dssnnnndddd0000nqm1mmmm");
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=0
//    = {actual: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd0001nqm0mmmm',
//       rule: 'VRHADD',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI8_16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd0001nqm0mmmm,
//       rule: VRHADD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case2_TestCase2) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case2 tester;
  tester.Test("1111001u0dssnnnndddd0001nqm0mmmm");
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=0 & inst(21:20)=00
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100d00nnnndddd0001nqm1mmmm',
//       rule: 'VAND_register',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=00
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100d00nnnndddd0001nqm1mmmm,
//       rule: VAND_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case3_TestCase3) {
  VectorBinary3RegisterSameLengthDQTester_Case3 tester;
  tester.Test("111100100d00nnnndddd0001nqm1mmmm");
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=0 & inst(21:20)=01
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100d01nnnndddd0001nqm1mmmm',
//       rule: 'VBIC_register',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=01
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100d01nnnndddd0001nqm1mmmm,
//       rule: VBIC_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case4_TestCase4) {
  VectorBinary3RegisterSameLengthDQTester_Case4 tester;
  tester.Test("111100100d01nnnndddd0001nqm1mmmm");
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=0 & inst(21:20)=10
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100d10nnnndddd0001nqm1mmmm',
//       rule: 'VORR_register_or_VMOV_register_A1',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=10
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100d10nnnndddd0001nqm1mmmm,
//       rule: VORR_register_or_VMOV_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case5_TestCase5) {
  VectorBinary3RegisterSameLengthDQTester_Case5 tester;
  tester.Test("111100100d10nnnndddd0001nqm1mmmm");
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=0 & inst(21:20)=11
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100d11nnnndddd0001nqm1mmmm',
//       rule: 'VORN_register',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=0 & C(21:20)=11
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100d11nnnndddd0001nqm1mmmm,
//       rule: VORN_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case6_TestCase6) {
  VectorBinary3RegisterSameLengthDQTester_Case6 tester;
  tester.Test("111100100d11nnnndddd0001nqm1mmmm");
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=1 & inst(21:20)=00
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100110d00nnnndddd0001nqm1mmmm',
//       rule: 'VEOR',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=00
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100110d00nnnndddd0001nqm1mmmm,
//       rule: VEOR,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case7_TestCase7) {
  VectorBinary3RegisterSameLengthDQTester_Case7 tester;
  tester.Test("111100110d00nnnndddd0001nqm1mmmm");
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=1 & inst(21:20)=01
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100110d01nnnndddd0001nqm1mmmm',
//       rule: 'VBSL',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=01
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100110d01nnnndddd0001nqm1mmmm,
//       rule: VBSL,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case8_TestCase8) {
  VectorBinary3RegisterSameLengthDQTester_Case8 tester;
  tester.Test("111100110d01nnnndddd0001nqm1mmmm");
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=1 & inst(21:20)=10
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100110d10nnnndddd0001nqm1mmmm',
//       rule: 'VBIT',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=10
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100110d10nnnndddd0001nqm1mmmm,
//       rule: VBIT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case9_TestCase9) {
  VectorBinary3RegisterSameLengthDQTester_Case9 tester;
  tester.Test("111100110d10nnnndddd0001nqm1mmmm");
}

// Neutral case:
// inst(11:8)=0001 & inst(4)=1 & inst(24)=1 & inst(21:20)=11
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100110d11nnnndddd0001nqm1mmmm',
//       rule: 'VBIF',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0001 & B(4)=1 & U(24)=1 & C(21:20)=11
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100110d11nnnndddd0001nqm1mmmm,
//       rule: VBIF,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case10_TestCase10) {
  VectorBinary3RegisterSameLengthDQTester_Case10 tester;
  tester.Test("111100110d11nnnndddd0001nqm1mmmm");
}

// Neutral case:
// inst(11:8)=0010 & inst(4)=0
//    = {actual: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd0010nqm0mmmm',
//       rule: 'VHSUB',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0010 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI8_16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd0010nqm0mmmm,
//       rule: VHSUB,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case11_TestCase11) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case11 tester;
  tester.Test("1111001u0dssnnnndddd0010nqm0mmmm");
}

// Neutral case:
// inst(11:8)=0010 & inst(4)=1
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd0010nqm1mmmm',
//       rule: 'VQSUB',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0010 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd0010nqm1mmmm,
//       rule: VQSUB,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case12_TestCase12) {
  VectorBinary3RegisterSameLengthDQTester_Case12 tester;
  tester.Test("1111001u0dssnnnndddd0010nqm1mmmm");
}

// Neutral case:
// inst(11:8)=0011 & inst(4)=0
//    = {actual: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd0011nqm0mmmm',
//       rule: 'VCGT_register_A1',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0011 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI8_16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd0011nqm0mmmm,
//       rule: VCGT_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case13_TestCase13) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case13 tester;
  tester.Test("1111001u0dssnnnndddd0011nqm0mmmm");
}

// Neutral case:
// inst(11:8)=0011 & inst(4)=1
//    = {actual: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd0011nqm1mmmm',
//       rule: 'VCGE_register_A1',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0011 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI8_16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd0011nqm1mmmm,
//       rule: VCGE_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case14_TestCase14) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case14 tester;
  tester.Test("1111001u0dssnnnndddd0011nqm1mmmm");
}

// Neutral case:
// inst(11:8)=0100 & inst(4)=0
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd0100nqm0mmmm',
//       rule: 'VSHL_register',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0100 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd0100nqm0mmmm,
//       rule: VSHL_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case15_TestCase15) {
  VectorBinary3RegisterSameLengthDQTester_Case15 tester;
  tester.Test("1111001u0dssnnnndddd0100nqm0mmmm");
}

// Neutral case:
// inst(11:8)=0100 & inst(4)=1
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd0100nqm1mmmm',
//       rule: 'VQSHL_register',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0100 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd0100nqm1mmmm,
//       rule: VQSHL_register,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case16_TestCase16) {
  VectorBinary3RegisterSameLengthDQTester_Case16 tester;
  tester.Test("1111001u0dssnnnndddd0100nqm1mmmm");
}

// Neutral case:
// inst(11:8)=0101 & inst(4)=0
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd0101nqm0mmmm',
//       rule: 'VRSHL',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0101 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd0101nqm0mmmm,
//       rule: VRSHL,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case17_TestCase17) {
  VectorBinary3RegisterSameLengthDQTester_Case17 tester;
  tester.Test("1111001u0dssnnnndddd0101nqm0mmmm");
}

// Neutral case:
// inst(11:8)=0101 & inst(4)=1
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd0101nqm1mmmm',
//       rule: 'VQRSHL',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0101 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd0101nqm1mmmm,
//       rule: VQRSHL,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case18_TestCase18) {
  VectorBinary3RegisterSameLengthDQTester_Case18 tester;
  tester.Test("1111001u0dssnnnndddd0101nqm1mmmm");
}

// Neutral case:
// inst(11:8)=0110 & inst(4)=0
//    = {actual: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd0110nqm0mmmm',
//       rule: 'VMAX',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0110 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI8_16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd0110nqm0mmmm,
//       rule: VMAX,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case19_TestCase19) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case19 tester;
  tester.Test("1111001u0dssnnnndddd0110nqm0mmmm");
}

// Neutral case:
// inst(11:8)=0110 & inst(4)=1
//    = {actual: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd0110nqm1mmmm',
//       rule: 'VMIN',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0110 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI8_16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd0110nqm1mmmm,
//       rule: VMIN,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case20_TestCase20) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case20 tester;
  tester.Test("1111001u0dssnnnndddd0110nqm1mmmm");
}

// Neutral case:
// inst(11:8)=0111 & inst(4)=0
//    = {actual: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd0111nqm0mmmm',
//       rule: 'VABD',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0111 & B(4)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI8_16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd0111nqm0mmmm,
//       rule: VABD,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case21_TestCase21) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case21 tester;
  tester.Test("1111001u0dssnnnndddd0111nqm0mmmm");
}

// Neutral case:
// inst(11:8)=0111 & inst(4)=1
//    = {actual: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd0111nqm1mmmm',
//       rule: 'VABA',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=0111 & B(4)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI8_16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd0111nqm1mmmm,
//       rule: VABA,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case22_TestCase22) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case22 tester;
  tester.Test("1111001u0dssnnnndddd0111nqm1mmmm");
}

// Neutral case:
// inst(11:8)=1000 & inst(4)=0 & inst(24)=0
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100dssnnnndddd1000nqm0mmmm',
//       rule: 'VADD_integer',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1000 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100dssnnnndddd1000nqm0mmmm,
//       rule: VADD_integer,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case23_TestCase23) {
  VectorBinary3RegisterSameLengthDQTester_Case23 tester;
  tester.Test("111100100dssnnnndddd1000nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1000 & inst(4)=0 & inst(24)=1
//    = {actual: 'VectorBinary3RegisterSameLengthDQ',
//       baseline: 'VectorBinary3RegisterSameLengthDQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100110dssnnnndddd1000nqm0mmmm',
//       rule: 'VSUB_integer',
//       safety: [inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1000 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQ,
//       baseline: VectorBinary3RegisterSameLengthDQ,
//       constraints: ,
//       defs: {},
//       fields: [Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100110dssnnnndddd1000nqm0mmmm,
//       rule: VSUB_integer,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED]}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQTester_Case24_TestCase24) {
  VectorBinary3RegisterSameLengthDQTester_Case24 tester;
  tester.Test("111100110dssnnnndddd1000nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1000 & inst(4)=1 & inst(24)=0
//    = {actual: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100100dssnnnndddd1000nqm1mmmm',
//       rule: 'VTST',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1000 & B(4)=1 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI8_16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100dssnnnndddd1000nqm1mmmm,
//       rule: VTST,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case25_TestCase25) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case25 tester;
  tester.Test("111100100dssnnnndddd1000nqm1mmmm");
}

// Neutral case:
// inst(11:8)=1000 & inst(4)=1 & inst(24)=1
//    = {actual: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100110dssnnnndddd1000nqm1mmmm',
//       rule: 'VCEQ_register_A1',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1000 & B(4)=1 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI8_16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100110dssnnnndddd1000nqm1mmmm,
//       rule: VCEQ_register_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case26_TestCase26) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case26 tester;
  tester.Test("111100110dssnnnndddd1000nqm1mmmm");
}

// Neutral case:
// inst(11:8)=1001 & inst(4)=0 & inst(24)=0
//    = {actual: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd1001nqm0mmmm',
//       rule: 'VMLA_integer_A1',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1001 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI8_16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd1001nqm0mmmm,
//       rule: VMLA_integer_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case27_TestCase27) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case27 tester;
  tester.Test("1111001u0dssnnnndddd1001nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1001 & inst(4)=0 & inst(24)=1
//    = {actual: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd1001nqm0mmmm',
//       rule: 'VMLS_integer_A1',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1001 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI8_16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd1001nqm0mmmm,
//       rule: VMLS_integer_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case28_TestCase28) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case28 tester;
  tester.Test("1111001u0dssnnnndddd1001nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1001 & inst(4)=1 & inst(24)=0
//    = {actual: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd1001nqm1mmmm',
//       rule: 'VMUL_integer_A1',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1001 & B(4)=1 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI8_16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd1001nqm1mmmm,
//       rule: VMUL_integer_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=11 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case29_TestCase29) {
  VectorBinary3RegisterSameLengthDQI8_16_32Tester_Case29 tester;
  tester.Test("1111001u0dssnnnndddd1001nqm1mmmm");
}

// Neutral case:
// inst(11:8)=1001 & inst(4)=1 & inst(24)=1
//    = {actual: 'VectorBinary3RegisterSameLengthDQI8P',
//       baseline: 'VectorBinary3RegisterSameLengthDQI8P',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd1001nqm1mmmm',
//       rule: 'VMUL_polynomial_A1',
//       safety: [inst(21:20)=~00 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1001 & B(4)=1 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI8P,
//       baseline: VectorBinary3RegisterSameLengthDQI8P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 1111001u0dssnnnndddd1001nqm1mmmm,
//       rule: VMUL_polynomial_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(21:20)=~00 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI8PTester_Case30_TestCase30) {
  VectorBinary3RegisterSameLengthDQI8PTester_Case30 tester;
  tester.Test("1111001u0dssnnnndddd1001nqm1mmmm");
}

// Neutral case:
// inst(11:8)=1010 & inst(4)=0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {actual: 'VectorBinary3RegisterSameLengthDI',
//       baseline: 'VectorBinary3RegisterSameLengthDI',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd1010n0m0mmmm',
//       rule: 'VPMAX',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1010 & B(4)=0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       actual: VectorBinary3RegisterSameLengthDI,
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       pattern: 1111001u0dssnnnndddd1010n0m0mmmm,
//       rule: VPMAX,
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDITester_Case31_TestCase31) {
  VectorBinary3RegisterSameLengthDITester_Case31 tester;
  tester.Test("1111001u0dssnnnndddd1010n0m0mmmm");
}

// Neutral case:
// inst(11:8)=1010 & inst(4)=1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {actual: 'VectorBinary3RegisterSameLengthDI',
//       baseline: 'VectorBinary3RegisterSameLengthDI',
//       constraints: ,
//       defs: {},
//       pattern: '1111001u0dssnnnndddd1010n0m1mmmm',
//       rule: 'VPMIN',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1010 & B(4)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       actual: VectorBinary3RegisterSameLengthDI,
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       pattern: 1111001u0dssnnnndddd1010n0m1mmmm,
//       rule: VPMIN,
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDITester_Case32_TestCase32) {
  VectorBinary3RegisterSameLengthDITester_Case32 tester;
  tester.Test("1111001u0dssnnnndddd1010n0m1mmmm");
}

// Neutral case:
// inst(11:8)=1011 & inst(4)=0 & inst(24)=0
//    = {actual: 'VectorBinary3RegisterSameLengthDQI16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100100dssnnnndddd1011nqm0mmmm',
//       rule: 'VQDMULH_A1',
//       safety: [(inst(21:20)=11 ||
//            inst(21:20)=00) => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1011 & B(4)=0 & U(24)=0
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100dssnnnndddd1011nqm0mmmm,
//       rule: VQDMULH_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         (size(21:20)=11 ||
//            size(21:20)=00) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI16_32Tester_Case33_TestCase33) {
  VectorBinary3RegisterSameLengthDQI16_32Tester_Case33 tester;
  tester.Test("111100100dssnnnndddd1011nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1011 & inst(4)=0 & inst(24)=1
//    = {actual: 'VectorBinary3RegisterSameLengthDQI16_32',
//       baseline: 'VectorBinary3RegisterSameLengthDQI16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100110dssnnnndddd1011nqm0mmmm',
//       rule: 'VQRDMULH_A1',
//       safety: [(inst(21:20)=11 ||
//            inst(21:20)=00) => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1011 & B(4)=0 & U(24)=1
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLengthDQI16_32,
//       baseline: VectorBinary3RegisterSameLengthDQI16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100110dssnnnndddd1011nqm0mmmm,
//       rule: VQRDMULH_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         (size(21:20)=11 ||
//            size(21:20)=00) => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDQI16_32Tester_Case34_TestCase34) {
  VectorBinary3RegisterSameLengthDQI16_32Tester_Case34 tester;
  tester.Test("111100110dssnnnndddd1011nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1011 & inst(4)=1 & inst(24)=0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {actual: 'VectorBinary3RegisterSameLengthDI',
//       baseline: 'VectorBinary3RegisterSameLengthDI',
//       constraints: ,
//       defs: {},
//       pattern: '111100100dssnnnndddd1011n0m1mmmm',
//       rule: 'VPADD_integer',
//       safety: [inst(21:20)=11 => UNDEFINED,
//         inst(6)=1 => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1011 & B(4)=1 & U(24)=0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx0xxxxxx
//    = {Q: Q(6),
//       actual: VectorBinary3RegisterSameLengthDI,
//       baseline: VectorBinary3RegisterSameLengthDI,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       pattern: 111100100dssnnnndddd1011n0m1mmmm,
//       rule: VPADD_integer,
//       safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLengthDITester_Case35_TestCase35) {
  VectorBinary3RegisterSameLengthDITester_Case35 tester;
  tester.Test("111100100dssnnnndddd1011n0m1mmmm");
}

// Neutral case:
// inst(11:8)=1100 & inst(4)=1 & inst(24)=0 & inst(21:20)=0x & inst(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100d00nnnndddd1100nqm1mmmm',
//       rule: 'VFMA_A1',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1100 & B(4)=1 & U(24)=0 & C(21:20)=0x & $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100d00nnnndddd1100nqm1mmmm,
//       rule: VFMA_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case36_TestCase36) {
  VectorBinary3RegisterSameLength32_DQTester_Case36 tester;
  tester.Test("111100100d00nnnndddd1100nqm1mmmm");
}

// Neutral case:
// inst(11:8)=1100 & inst(4)=1 & inst(24)=0 & inst(21:20)=1x & inst(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100d10nnnndddd1100nqm1mmmm',
//       rule: 'VFMS_A1',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1100 & B(4)=1 & U(24)=0 & C(21:20)=1x & $pattern(31:0)=xxxxxxxxxxx0xxxxxxxxxxxxxxxxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100d10nnnndddd1100nqm1mmmm,
//       rule: VFMS_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case37_TestCase37) {
  VectorBinary3RegisterSameLength32_DQTester_Case37 tester;
  tester.Test("111100100d10nnnndddd1100nqm1mmmm");
}

// Neutral case:
// inst(11:8)=1101 & inst(4)=0 & inst(24)=0 & inst(21:20)=0x
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100d0snnnndddd1101nqm0mmmm',
//       rule: 'VADD_floating_point_A1',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100d0snnnndddd1101nqm0mmmm,
//       rule: VADD_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case38_TestCase38) {
  VectorBinary3RegisterSameLength32_DQTester_Case38 tester;
  tester.Test("111100100d0snnnndddd1101nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1101 & inst(4)=0 & inst(24)=0 & inst(21:20)=1x
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100d1snnnndddd1101nqm0mmmm',
//       rule: 'VSUB_floating_point_A1',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101 & B(4)=0 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100d1snnnndddd1101nqm0mmmm,
//       rule: VSUB_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case39_TestCase39) {
  VectorBinary3RegisterSameLength32_DQTester_Case39 tester;
  tester.Test("111100100d1snnnndddd1101nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1101 & inst(4)=0 & inst(24)=1 & inst(21:20)=0x
//    = {actual: 'VectorBinary3RegisterSameLength32P',
//       baseline: 'VectorBinary3RegisterSameLength32P',
//       constraints: ,
//       defs: {},
//       pattern: '111100110d0snnnndddd1101nqm0mmmm',
//       rule: 'VPADD_floating_point',
//       safety: [inst(21:20)(0)=1 ||
//            inst(6)=1 => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       actual: VectorBinary3RegisterSameLength32P,
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       pattern: 111100110d0snnnndddd1101nqm0mmmm,
//       rule: VPADD_floating_point,
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32PTester_Case40_TestCase40) {
  VectorBinary3RegisterSameLength32PTester_Case40 tester;
  tester.Test("111100110d0snnnndddd1101nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1101 & inst(4)=0 & inst(24)=1 & inst(21:20)=1x
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100110d1snnnndddd1101nqm0mmmm',
//       rule: 'VABD_floating_point',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100110d1snnnndddd1101nqm0mmmm,
//       rule: VABD_floating_point,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case41_TestCase41) {
  VectorBinary3RegisterSameLength32_DQTester_Case41 tester;
  tester.Test("111100110d1snnnndddd1101nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1101 & inst(4)=1 & inst(24)=0 & inst(21:20)=0x
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100dpsnnnndddd1101nqm1mmmm',
//       rule: 'VMLA_floating_point_A1',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101 & B(4)=1 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100dpsnnnndddd1101nqm1mmmm,
//       rule: VMLA_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case42_TestCase42) {
  VectorBinary3RegisterSameLength32_DQTester_Case42 tester;
  tester.Test("111100100dpsnnnndddd1101nqm1mmmm");
}

// Neutral case:
// inst(11:8)=1101 & inst(4)=1 & inst(24)=0 & inst(21:20)=1x
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100dpsnnnndddd1101nqm1mmmm',
//       rule: 'VMLS_floating_point_A1',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101 & B(4)=1 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100dpsnnnndddd1101nqm1mmmm,
//       rule: VMLS_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case43_TestCase43) {
  VectorBinary3RegisterSameLength32_DQTester_Case43 tester;
  tester.Test("111100100dpsnnnndddd1101nqm1mmmm");
}

// Neutral case:
// inst(11:8)=1101 & inst(4)=1 & inst(24)=1 & inst(21:20)=0x
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100110d0snnnndddd1101nqm1mmmm',
//       rule: 'VMUL_floating_point_A1',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1101 & B(4)=1 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100110d0snnnndddd1101nqm1mmmm,
//       rule: VMUL_floating_point_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case44_TestCase44) {
  VectorBinary3RegisterSameLength32_DQTester_Case44 tester;
  tester.Test("111100110d0snnnndddd1101nqm1mmmm");
}

// Neutral case:
// inst(11:8)=1110 & inst(4)=0 & inst(24)=0 & inst(21:20)=0x
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100d0snnnndddd1110nqm0mmmm',
//       rule: 'VCEQ_register_A2',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1110 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100d0snnnndddd1110nqm0mmmm,
//       rule: VCEQ_register_A2,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case45_TestCase45) {
  VectorBinary3RegisterSameLength32_DQTester_Case45 tester;
  tester.Test("111100100d0snnnndddd1110nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1110 & inst(4)=0 & inst(24)=1 & inst(21:20)=0x
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100110d0snnnndddd1110nqm0mmmm',
//       rule: 'VCGE_register_A2',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1110 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100110d0snnnndddd1110nqm0mmmm,
//       rule: VCGE_register_A2,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case46_TestCase46) {
  VectorBinary3RegisterSameLength32_DQTester_Case46 tester;
  tester.Test("111100110d0snnnndddd1110nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1110 & inst(4)=0 & inst(24)=1 & inst(21:20)=1x
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100110d1snnnndddd1110nqm0mmmm',
//       rule: 'VCGT_register_A2',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1110 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100110d1snnnndddd1110nqm0mmmm,
//       rule: VCGT_register_A2,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case47_TestCase47) {
  VectorBinary3RegisterSameLength32_DQTester_Case47 tester;
  tester.Test("111100110d1snnnndddd1110nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1110 & inst(4)=1 & inst(24)=1 & inst(21:20)=0x
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100110dssnnnndddd1110nqm1mmmm',
//       rule: 'VACGE',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1110 & B(4)=1 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100110dssnnnndddd1110nqm1mmmm,
//       rule: VACGE,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case48_TestCase48) {
  VectorBinary3RegisterSameLength32_DQTester_Case48 tester;
  tester.Test("111100110dssnnnndddd1110nqm1mmmm");
}

// Neutral case:
// inst(11:8)=1110 & inst(4)=1 & inst(24)=1 & inst(21:20)=1x
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100110dssnnnndddd1110nqm1mmmm',
//       rule: 'VACGT',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1110 & B(4)=1 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100110dssnnnndddd1110nqm1mmmm,
//       rule: VACGT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case49_TestCase49) {
  VectorBinary3RegisterSameLength32_DQTester_Case49 tester;
  tester.Test("111100110dssnnnndddd1110nqm1mmmm");
}

// Neutral case:
// inst(11:8)=1111 & inst(4)=0 & inst(24)=0 & inst(21:20)=0x
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100dssnnnndddd1111nqm0mmmm',
//       rule: 'VMAX_floating_point',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1111 & B(4)=0 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100dssnnnndddd1111nqm0mmmm,
//       rule: VMAX_floating_point,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case50_TestCase50) {
  VectorBinary3RegisterSameLength32_DQTester_Case50 tester;
  tester.Test("111100100dssnnnndddd1111nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1111 & inst(4)=0 & inst(24)=0 & inst(21:20)=1x
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100dssnnnndddd1111nqm0mmmm',
//       rule: 'VMIN_floating_point',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1111 & B(4)=0 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100dssnnnndddd1111nqm0mmmm,
//       rule: VMIN_floating_point,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case51_TestCase51) {
  VectorBinary3RegisterSameLength32_DQTester_Case51 tester;
  tester.Test("111100100dssnnnndddd1111nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1111 & inst(4)=0 & inst(24)=1 & inst(21:20)=0x
//    = {actual: 'VectorBinary3RegisterSameLength32P',
//       baseline: 'VectorBinary3RegisterSameLength32P',
//       constraints: ,
//       defs: {},
//       pattern: '111100110dssnnnndddd1111nqm0mmmm',
//       rule: 'VPMAX',
//       safety: [inst(21:20)(0)=1 ||
//            inst(6)=1 => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1111 & B(4)=0 & U(24)=1 & C(21:20)=0x
//    = {Q: Q(6),
//       actual: VectorBinary3RegisterSameLength32P,
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       pattern: 111100110dssnnnndddd1111nqm0mmmm,
//       rule: VPMAX,
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32PTester_Case52_TestCase52) {
  VectorBinary3RegisterSameLength32PTester_Case52 tester;
  tester.Test("111100110dssnnnndddd1111nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1111 & inst(4)=0 & inst(24)=1 & inst(21:20)=1x
//    = {actual: 'VectorBinary3RegisterSameLength32P',
//       baseline: 'VectorBinary3RegisterSameLength32P',
//       constraints: ,
//       defs: {},
//       pattern: '111100110dssnnnndddd1111nqm0mmmm',
//       rule: 'VPMIN',
//       safety: [inst(21:20)(0)=1 ||
//            inst(6)=1 => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1111 & B(4)=0 & U(24)=1 & C(21:20)=1x
//    = {Q: Q(6),
//       actual: VectorBinary3RegisterSameLength32P,
//       baseline: VectorBinary3RegisterSameLength32P,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Q(6)],
//       pattern: 111100110dssnnnndddd1111nqm0mmmm,
//       rule: VPMIN,
//       safety: [size(0)=1 ||
//            Q(6)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32PTester_Case53_TestCase53) {
  VectorBinary3RegisterSameLength32PTester_Case53 tester;
  tester.Test("111100110dssnnnndddd1111nqm0mmmm");
}

// Neutral case:
// inst(11:8)=1111 & inst(4)=1 & inst(24)=0 & inst(21:20)=0x
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100d0snnnndddd1111nqm1mmmm',
//       rule: 'VRECPS',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1111 & B(4)=1 & U(24)=0 & C(21:20)=0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100d0snnnndddd1111nqm1mmmm,
//       rule: VRECPS,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case54_TestCase54) {
  VectorBinary3RegisterSameLength32_DQTester_Case54 tester;
  tester.Test("111100100d0snnnndddd1111nqm1mmmm");
}

// Neutral case:
// inst(11:8)=1111 & inst(4)=1 & inst(24)=0 & inst(21:20)=1x
//    = {actual: 'VectorBinary3RegisterSameLength32_DQ',
//       baseline: 'VectorBinary3RegisterSameLength32_DQ',
//       constraints: ,
//       defs: {},
//       pattern: '111100100d1snnnndddd1111nqm1mmmm',
//       rule: 'VRSQRTS',
//       safety: [inst(21:20)(0)=1 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(19:16)(0)=1 ||
//            inst(15:12)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(11:8)=1111 & B(4)=1 & U(24)=0 & C(21:20)=1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vn: Vn(19:16),
//       actual: VectorBinary3RegisterSameLength32_DQ,
//       baseline: VectorBinary3RegisterSameLength32_DQ,
//       constraints: ,
//       defs: {},
//       fields: [size(21:20), Vn(19:16), Vd(15:12), Q(6)],
//       pattern: 111100100d1snnnndddd1111nqm1mmmm,
//       rule: VRSQRTS,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vn(0)=1 ||
//            Vd(0)=1) => UNDEFINED,
//         size(0)=1 => UNDEFINED],
//       size: size(21:20)}
TEST_F(Arm32DecoderStateTests,
       VectorBinary3RegisterSameLength32_DQTester_Case55_TestCase55) {
  VectorBinary3RegisterSameLength32_DQTester_Case55 tester;
  tester.Test("111100100d1snnnndddd1111nqm1mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
