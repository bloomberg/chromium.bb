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
// inst(17:16)=00 & inst(10:6)=0000x
//    = {baseline: 'Vector2RegisterMiscellaneous_RG',
//       constraints: ,
//       defs: {},
//       safety: [3  <
//               inst(8:7) + inst(19:18) => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=0000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       op: op(8:7),
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase0
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase0(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~0000x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: op + size  >=
  //          3 => UNDEFINED
  EXPECT_TRUE(((((inst.Bits() & 0x00000180) >> 7) + ((inst.Bits() & 0x000C0000) >> 18)) < (3)));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=0001x
//    = {baseline: 'Vector2RegisterMiscellaneous_RG',
//       constraints: ,
//       defs: {},
//       safety: [3  <
//               inst(8:7) + inst(19:18) => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=0001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       op: op(8:7),
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase1
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase1(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~0001x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000080) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: op + size  >=
  //          3 => UNDEFINED
  EXPECT_TRUE(((((inst.Bits() & 0x00000180) >> 7) + ((inst.Bits() & 0x000C0000) >> 18)) < (3)));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=0010x
//    = {baseline: 'Vector2RegisterMiscellaneous_RG',
//       constraints: ,
//       defs: {},
//       safety: [3  <
//               inst(8:7) + inst(19:18) => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=0010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       op: op(8:7),
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase2
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase2(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~0010x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: op + size  >=
  //          3 => UNDEFINED
  EXPECT_TRUE(((((inst.Bits() & 0x00000180) >> 7) + ((inst.Bits() & 0x000C0000) >> 18)) < (3)));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1000x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=1000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase3
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase3(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~1000x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000400) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x000C0000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1001x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=1001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase4
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase4(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~1001x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000480) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x000C0000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1010x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=~00 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=1010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase5
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase5(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~1010x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000500) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=~00 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  ==
          0x00000000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1011x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=~00 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=1011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase6
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase6(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~1011x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000580) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=~00 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  ==
          0x00000000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1110x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=1110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase7
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase7(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~1110x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000700) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x000C0000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1111x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=1111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase8
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase8(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~1111x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000780) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x000C0000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=010xx & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=010xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase9
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase9(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~010xx
  if ((inst.Bits() & 0x00000700)  !=
          0x00000200) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x000C0000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=110xx & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=110xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase10
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase10(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // B(10:6)=~110xx
  if ((inst.Bits() & 0x00000700)  !=
          0x00000600) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x000C0000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0000x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=0000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase11
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase11(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~0000x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x000C0000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0001x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=0001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase12
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase12(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~0001x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000080) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x000C0000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0010x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=0010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase13
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase13(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~0010x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x000C0000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0011x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=0011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase14
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase14(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~0011x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000180) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x000C0000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0100x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=0100x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase15
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase15(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~0100x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000200) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase15
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x000C0000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0110x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=0110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase16
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase16(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~0110x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000300) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x000C0000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0111x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=0111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase17
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase17(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~0111x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000380) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase17
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x000C0000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1000x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=1000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase18
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase18(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~1000x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000400) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase18
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(19:18)=~10 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  ==
          0x00080000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1001x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=1001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase19
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase19(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~1001x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000480) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase19
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(19:18)=~10 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  ==
          0x00080000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1010x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=1010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase20
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase20(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase20
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~1010x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000500) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase20
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(19:18)=~10 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  ==
          0x00080000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1011x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=1011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase21
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase21(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase21
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~1011x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000580) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase21
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(19:18)=~10 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  ==
          0x00080000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1100x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=1100x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase22
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase22(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase22
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~1100x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000600) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase22
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(19:18)=~10 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  ==
          0x00080000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1110x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=1110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase23
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase23(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase23
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~1110x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000700) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase23
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(19:18)=~10 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  ==
          0x00080000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1111x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=1111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase24
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase24(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase24
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // B(10:6)=~1111x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000780) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase24
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(19:18)=~10 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  ==
          0x00080000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=01000
//    = {baseline: 'Vector2RegisterMiscellaneous_V16_32_64N',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(3:0)(0)=1 => UNDEFINED]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=01000
//    = {Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V16_32_64N,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vm(3:0)],
//       safety: [size(19:18)=11 => UNDEFINED, Vm(0)=1 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase25
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase25(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase25
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~01000
  if ((inst.Bits() & 0x000007C0)  !=
          0x00000200) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase25
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x000C0000);

  // safety: Vm(0)=1 => UNDEFINED
  EXPECT_TRUE(((inst.Bits() & 0x0000000F) & 0x00000001)  !=
          0x00000001);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=01001
//    = {baseline: 'Vector2RegisterMiscellaneous_I16_32_64N',
//       constraints: ,
//       safety: [inst(19:18)=11 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(7:6)=00 => DECODER_ERROR]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=01001
//    = {Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//       constraints: ,
//       fields: [size(19:18), op(7:6), Vm(3:0)],
//       op: op(7:6),
//       safety: [op(7:6)=00 => DECODER_ERROR,
//         size(19:18)=11 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_I16_32_64NTesterCase26
    : public Vector2RegisterMiscellaneous_I16_32_64NTester {
 public:
  Vector2RegisterMiscellaneous_I16_32_64NTesterCase26(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneous_I16_32_64NTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_I16_32_64NTesterCase26
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~01001
  if ((inst.Bits() & 0x000007C0)  !=
          0x00000240) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneous_I16_32_64NTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneous_I16_32_64NTesterCase26
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneous_I16_32_64NTester::
               ApplySanityChecks(inst, decoder));

  // safety: op(7:6)=00 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000000C0)  !=
          0x00000000);

  // safety: size(19:18)=11 ||
  //       Vm(0)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x000C0000)  ==
          0x000C0000) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)));

  return true;
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=01100
//    = {baseline: 'Vector2RegisterMiscellaneous_I8_16_32L',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 ||
//            inst(15:12)(0)=1 => UNDEFINED]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=01100
//    = {Vd: Vd(15:12),
//       baseline: Vector2RegisterMiscellaneous_I8_16_32L,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12)],
//       safety: [size(19:18)=11 ||
//            Vd(0)=1 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase27
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase27(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase27
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~01100
  if ((inst.Bits() & 0x000007C0)  !=
          0x00000300) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase27
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=11 ||
  //       Vd(0)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x000C0000)  ==
          0x000C0000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=11x00
//    = {baseline: 'Vector2RegisterMiscellaneous_CVT_H2S',
//       constraints: ,
//       safety: [inst(19:18)=~01 => UNDEFINED,
//         inst(8)=1 &&
//            inst(15:12)(0)=1 => UNDEFINED,
//         not inst(8)=1 &&
//            inst(3:0)(0)=1 => UNDEFINED]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=11x00
//    = {Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_CVT_H2S,
//       constraints: ,
//       fields: [size(19:18), Vd(15:12), op(8), Vm(3:0)],
//       half_to_single: op(8)=1,
//       op: op(8),
//       safety: [size(19:18)=~01 => UNDEFINED,
//         half_to_single &&
//            Vd(0)=1 => UNDEFINED,
//         not half_to_single &&
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_CVT_H2STesterCase28
    : public Vector2RegisterMiscellaneous_CVT_H2STester {
 public:
  Vector2RegisterMiscellaneous_CVT_H2STesterCase28(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneous_CVT_H2STester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_CVT_H2STesterCase28
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~11x00
  if ((inst.Bits() & 0x000006C0)  !=
          0x00000600) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneous_CVT_H2STester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneous_CVT_H2STesterCase28
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneous_CVT_H2STester::
               ApplySanityChecks(inst, decoder));

  // safety: size(19:18)=~01 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  ==
          0x00040000);

  // safety: half_to_single &&
  //       Vd(0)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000100)  ==
          0x00000100) &&
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)));

  // safety: not half_to_single &&
  //       Vm(0)=1 => UNDEFINED
  EXPECT_TRUE(!((!((inst.Bits() & 0x00000100)  ==
          0x00000100)) &&
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)));

  return true;
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=0000x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8S',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=~00 => UNDEFINED,
//         inst(22):inst(15:12)  ==
//               inst(5):inst(3:0) => UNKNOWN,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=0000x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8S,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       m: M:Vm,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase29
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase29(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase29
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~0000x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase29
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: d  ==
  //          m => UNKNOWN
  EXPECT_TRUE((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12))) != ((((((inst.Bits() & 0x00000020) >> 5)) << 4) | (inst.Bits() & 0x0000000F)))));

  // safety: size(19:18)=~00 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  ==
          0x00000000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=0001x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32T',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(22):inst(15:12)  ==
//               inst(5):inst(3:0) => UNKNOWN,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=0001x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32T,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       m: M:Vm,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase30
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase30(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase30
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~0001x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000080) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase30
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: d  ==
  //          m => UNKNOWN
  EXPECT_TRUE((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12))) != ((((((inst.Bits() & 0x00000020) >> 5)) << 4) | (inst.Bits() & 0x0000000F)))));

  // safety: size(19:18)=11 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  !=
          0x000C0000);

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=0010x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32I',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 ||
//            (inst(6)=0 &&
//            inst(19:18)=10) => UNDEFINED,
//         inst(22):inst(15:12)  ==
//               inst(5):inst(3:0) => UNKNOWN,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=0010x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32I,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       m: M:Vm,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 ||
//            (Q(6)=0 &&
//            size(19:18)=10) => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase31
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase31(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase31
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~0010x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase31
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: d  ==
  //          m => UNKNOWN
  EXPECT_TRUE((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12))) != ((((((inst.Bits() & 0x00000020) >> 5)) << 4) | (inst.Bits() & 0x0000000F)))));

  // safety: size(19:18)=11 ||
  //       (Q(6)=0 &&
  //       size(19:18)=10) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x000C0000)  ==
          0x000C0000) ||
       ((((inst.Bits() & 0x00000040)  ==
          0x00000000) &&
       ((inst.Bits() & 0x000C0000)  ==
          0x00080000)))));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=0011x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32I',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=11 ||
//            (inst(6)=0 &&
//            inst(19:18)=10) => UNDEFINED,
//         inst(22):inst(15:12)  ==
//               inst(5):inst(3:0) => UNKNOWN,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=0011x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32I,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       m: M:Vm,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 ||
//            (Q(6)=0 &&
//            size(19:18)=10) => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase32
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase32(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase32
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~0011x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000180) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase32
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: d  ==
  //          m => UNKNOWN
  EXPECT_TRUE((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12))) != ((((((inst.Bits() & 0x00000020) >> 5)) << 4) | (inst.Bits() & 0x0000000F)))));

  // safety: size(19:18)=11 ||
  //       (Q(6)=0 &&
  //       size(19:18)=10) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x000C0000)  ==
          0x000C0000) ||
       ((((inst.Bits() & 0x00000040)  ==
          0x00000000) &&
       ((inst.Bits() & 0x000C0000)  ==
          0x00080000)))));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=0101x
//    = {baseline: 'Vector2RegisterMiscellaneous_I16_32_64N',
//       constraints: ,
//       safety: [inst(19:18)=11 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(7:6)=00 => DECODER_ERROR]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=0101x
//    = {Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//       constraints: ,
//       fields: [size(19:18), op(7:6), Vm(3:0)],
//       op: op(7:6),
//       safety: [op(7:6)=00 => DECODER_ERROR,
//         size(19:18)=11 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_I16_32_64NTesterCase33
    : public Vector2RegisterMiscellaneous_I16_32_64NTester {
 public:
  Vector2RegisterMiscellaneous_I16_32_64NTesterCase33(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneous_I16_32_64NTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneous_I16_32_64NTesterCase33
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~10
  if ((inst.Bits() & 0x00030000)  !=
          0x00020000) return false;
  // B(10:6)=~0101x
  if ((inst.Bits() & 0x00000780)  !=
          0x00000280) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneous_I16_32_64NTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneous_I16_32_64NTesterCase33
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneous_I16_32_64NTester::
               ApplySanityChecks(inst, decoder));

  // safety: op(7:6)=00 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000000C0)  !=
          0x00000000);

  // safety: size(19:18)=11 ||
  //       Vm(0)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x000C0000)  ==
          0x000C0000) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)));

  return true;
}

// Neutral case:
// inst(17:16)=11 & inst(10:6)=10x0x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=11 & B(10:6)=10x0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase34
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase34(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase34
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~11
  if ((inst.Bits() & 0x00030000)  !=
          0x00030000) return false;
  // B(10:6)=~10x0x
  if ((inst.Bits() & 0x00000680)  !=
          0x00000400) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase34
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(19:18)=~10 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  ==
          0x00080000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=11 & inst(10:6)=10x1x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=11 & B(10:6)=10x1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase35
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase35(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase35
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~11
  if ((inst.Bits() & 0x00030000)  !=
          0x00030000) return false;
  // B(10:6)=~10x1x
  if ((inst.Bits() & 0x00000680)  !=
          0x00000480) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase35
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(19:18)=~10 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  ==
          0x00080000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// Neutral case:
// inst(17:16)=11 & inst(10:6)=11xxx
//    = {baseline: 'Vector2RegisterMiscellaneous_CVT_F2I',
//       constraints: ,
//       defs: {},
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=11 & B(10:6)=11xxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_CVT_F2I,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneousTesterCase36
    : public Vector2RegisterMiscellaneousTester {
 public:
  Vector2RegisterMiscellaneousTesterCase36(const NamedClassDecoder& decoder)
    : Vector2RegisterMiscellaneousTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Vector2RegisterMiscellaneousTesterCase36
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // A(17:16)=~11
  if ((inst.Bits() & 0x00030000)  !=
          0x00030000) return false;
  // B(10:6)=~11xxx
  if ((inst.Bits() & 0x00000600)  !=
          0x00000600) return false;

  // Check other preconditions defined for the base decoder.
  return Vector2RegisterMiscellaneousTester::
      PassesParsePreconditions(inst, decoder);
}

bool Vector2RegisterMiscellaneousTesterCase36
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Vector2RegisterMiscellaneousTester::
               ApplySanityChecks(inst, decoder));

  // safety: Q(6)=1 &&
  //       (Vd(0)=1 ||
  //       Vm(0)=1) => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)))));

  // safety: size(19:18)=~10 => UNDEFINED
  EXPECT_TRUE((inst.Bits() & 0x000C0000)  ==
          0x00080000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(17:16)=00 & inst(10:6)=0000x
//    = {baseline: 'Vector2RegisterMiscellaneous_RG',
//       constraints: ,
//       defs: {},
//       rule: 'VREV64',
//       safety: [3  <
//               inst(8:7) + inst(19:18) => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=00 & B(10:6)=0000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       op: op(8:7),
//       rule: VREV64,
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_RGTester_Case0
    : public Vector2RegisterMiscellaneousTesterCase0 {
 public:
  Vector2RegisterMiscellaneous_RGTester_Case0()
    : Vector2RegisterMiscellaneousTesterCase0(
      state_.Vector2RegisterMiscellaneous_RG_VREV64_instance_)
  {}
};

// Neutral case:
// inst(17:16)=00 & inst(10:6)=0001x
//    = {baseline: 'Vector2RegisterMiscellaneous_RG',
//       constraints: ,
//       defs: {},
//       rule: 'VREV32',
//       safety: [3  <
//               inst(8:7) + inst(19:18) => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=00 & B(10:6)=0001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       op: op(8:7),
//       rule: VREV32,
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_RGTester_Case1
    : public Vector2RegisterMiscellaneousTesterCase1 {
 public:
  Vector2RegisterMiscellaneous_RGTester_Case1()
    : Vector2RegisterMiscellaneousTesterCase1(
      state_.Vector2RegisterMiscellaneous_RG_VREV32_instance_)
  {}
};

// Neutral case:
// inst(17:16)=00 & inst(10:6)=0010x
//    = {baseline: 'Vector2RegisterMiscellaneous_RG',
//       constraints: ,
//       defs: {},
//       rule: 'VREV16',
//       safety: [3  <
//               inst(8:7) + inst(19:18) => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=00 & B(10:6)=0010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       op: op(8:7),
//       rule: VREV16,
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_RGTester_Case2
    : public Vector2RegisterMiscellaneousTesterCase2 {
 public:
  Vector2RegisterMiscellaneous_RGTester_Case2()
    : Vector2RegisterMiscellaneousTesterCase2(
      state_.Vector2RegisterMiscellaneous_RG_VREV16_instance_)
  {}
};

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1000x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VCLS',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=00 & B(10:6)=1000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VCLS,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case3
    : public Vector2RegisterMiscellaneousTesterCase3 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case3()
    : Vector2RegisterMiscellaneousTesterCase3(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VCLS_instance_)
  {}
};

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1001x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VCLZ',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=00 & B(10:6)=1001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VCLZ,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case4
    : public Vector2RegisterMiscellaneousTesterCase4 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case4()
    : Vector2RegisterMiscellaneousTesterCase4(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VCLZ_instance_)
  {}
};

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1010x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8',
//       constraints: ,
//       defs: {},
//       rule: 'VCNT',
//       safety: [inst(19:18)=~00 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=00 & B(10:6)=1010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VCNT,
//       safety: [size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8Tester_Case5
    : public Vector2RegisterMiscellaneousTesterCase5 {
 public:
  Vector2RegisterMiscellaneous_V8Tester_Case5()
    : Vector2RegisterMiscellaneousTesterCase5(
      state_.Vector2RegisterMiscellaneous_V8_VCNT_instance_)
  {}
};

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1011x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8',
//       constraints: ,
//       defs: {},
//       rule: 'VMVN_register',
//       safety: [inst(19:18)=~00 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=00 & B(10:6)=1011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VMVN_register,
//       safety: [size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8Tester_Case6
    : public Vector2RegisterMiscellaneousTesterCase6 {
 public:
  Vector2RegisterMiscellaneous_V8Tester_Case6()
    : Vector2RegisterMiscellaneousTesterCase6(
      state_.Vector2RegisterMiscellaneous_V8_VMVN_register_instance_)
  {}
};

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1110x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VQABS',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=00 & B(10:6)=1110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VQABS,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case7
    : public Vector2RegisterMiscellaneousTesterCase7 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case7()
    : Vector2RegisterMiscellaneousTesterCase7(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VQABS_instance_)
  {}
};

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1111x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VQNEG',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=00 & B(10:6)=1111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VQNEG,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case8
    : public Vector2RegisterMiscellaneousTesterCase8 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case8()
    : Vector2RegisterMiscellaneousTesterCase8(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VQNEG_instance_)
  {}
};

// Neutral case:
// inst(17:16)=00 & inst(10:6)=010xx & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VPADDL',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=00 & B(10:6)=010xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VPADDL,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case9
    : public Vector2RegisterMiscellaneousTesterCase9 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case9()
    : Vector2RegisterMiscellaneousTesterCase9(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VPADDL_instance_)
  {}
};

// Neutral case:
// inst(17:16)=00 & inst(10:6)=110xx & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VPADAL',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=00 & B(10:6)=110xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VPADAL,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case10
    : public Vector2RegisterMiscellaneousTesterCase10 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case10()
    : Vector2RegisterMiscellaneousTesterCase10(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VPADAL_instance_)
  {}
};

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0000x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VCGT_immediate_0',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=01 & B(10:6)=0000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VCGT_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case11
    : public Vector2RegisterMiscellaneousTesterCase11 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case11()
    : Vector2RegisterMiscellaneousTesterCase11(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VCGT_immediate_0_instance_)
  {}
};

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0001x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VCGE_immediate_0',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=01 & B(10:6)=0001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VCGE_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case12
    : public Vector2RegisterMiscellaneousTesterCase12 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case12()
    : Vector2RegisterMiscellaneousTesterCase12(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VCGE_immediate_0_instance_)
  {}
};

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0010x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VCEQ_immediate_0',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=01 & B(10:6)=0010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VCEQ_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case13
    : public Vector2RegisterMiscellaneousTesterCase13 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case13()
    : Vector2RegisterMiscellaneousTesterCase13(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VCEQ_immediate_0_instance_)
  {}
};

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0011x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VCLE_immediate_0',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=01 & B(10:6)=0011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VCLE_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case14
    : public Vector2RegisterMiscellaneousTesterCase14 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case14()
    : Vector2RegisterMiscellaneousTesterCase14(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VCLE_immediate_0_instance_)
  {}
};

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0100x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VCLT_immediate_0',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=01 & B(10:6)=0100x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VCLT_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case15
    : public Vector2RegisterMiscellaneousTesterCase15 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case15()
    : Vector2RegisterMiscellaneousTesterCase15(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VCLT_immediate_0_instance_)
  {}
};

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0110x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VABS_A1',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=01 & B(10:6)=0110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VABS_A1,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case16
    : public Vector2RegisterMiscellaneousTesterCase16 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case16()
    : Vector2RegisterMiscellaneousTesterCase16(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VABS_A1_instance_)
  {}
};

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0111x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       rule: 'VNEG',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=01 & B(10:6)=0111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VNEG,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32Tester_Case17
    : public Vector2RegisterMiscellaneousTesterCase17 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case17()
    : Vector2RegisterMiscellaneousTesterCase17(
      state_.Vector2RegisterMiscellaneous_V8_16_32_VNEG_instance_)
  {}
};

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1000x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       rule: 'VCGT_immediate_0',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=01 & B(10:6)=1000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VCGT_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_F32Tester_Case18
    : public Vector2RegisterMiscellaneousTesterCase18 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case18()
    : Vector2RegisterMiscellaneousTesterCase18(
      state_.Vector2RegisterMiscellaneous_F32_VCGT_immediate_0_instance_)
  {}
};

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1001x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       rule: 'VCGE_immediate_0',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=01 & B(10:6)=1001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VCGE_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_F32Tester_Case19
    : public Vector2RegisterMiscellaneousTesterCase19 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case19()
    : Vector2RegisterMiscellaneousTesterCase19(
      state_.Vector2RegisterMiscellaneous_F32_VCGE_immediate_0_instance_)
  {}
};

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1010x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       rule: 'VCEQ_immediate_0',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=01 & B(10:6)=1010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VCEQ_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_F32Tester_Case20
    : public Vector2RegisterMiscellaneousTesterCase20 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case20()
    : Vector2RegisterMiscellaneousTesterCase20(
      state_.Vector2RegisterMiscellaneous_F32_VCEQ_immediate_0_instance_)
  {}
};

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1011x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       rule: 'VCLE_immediate_0',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=01 & B(10:6)=1011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VCLE_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_F32Tester_Case21
    : public Vector2RegisterMiscellaneousTesterCase21 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case21()
    : Vector2RegisterMiscellaneousTesterCase21(
      state_.Vector2RegisterMiscellaneous_F32_VCLE_immediate_0_instance_)
  {}
};

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1100x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       rule: 'VCLT_immediate_0',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=01 & B(10:6)=1100x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VCLT_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_F32Tester_Case22
    : public Vector2RegisterMiscellaneousTesterCase22 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case22()
    : Vector2RegisterMiscellaneousTesterCase22(
      state_.Vector2RegisterMiscellaneous_F32_VCLT_immediate_0_instance_)
  {}
};

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1110x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       rule: 'VABS_A1',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=01 & B(10:6)=1110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VABS_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_F32Tester_Case23
    : public Vector2RegisterMiscellaneousTesterCase23 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case23()
    : Vector2RegisterMiscellaneousTesterCase23(
      state_.Vector2RegisterMiscellaneous_F32_VABS_A1_instance_)
  {}
};

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1111x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       rule: 'VNEG',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=01 & B(10:6)=1111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VNEG,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_F32Tester_Case24
    : public Vector2RegisterMiscellaneousTesterCase24 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case24()
    : Vector2RegisterMiscellaneousTesterCase24(
      state_.Vector2RegisterMiscellaneous_F32_VNEG_instance_)
  {}
};

// Neutral case:
// inst(17:16)=10 & inst(10:6)=01000
//    = {baseline: 'Vector2RegisterMiscellaneous_V16_32_64N',
//       constraints: ,
//       defs: {},
//       rule: 'VMOVN',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(3:0)(0)=1 => UNDEFINED]}
//
// Representative case:
// A(17:16)=10 & B(10:6)=01000
//    = {Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V16_32_64N,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vm(3:0)],
//       rule: VMOVN,
//       safety: [size(19:18)=11 => UNDEFINED, Vm(0)=1 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V16_32_64NTester_Case25
    : public Vector2RegisterMiscellaneousTesterCase25 {
 public:
  Vector2RegisterMiscellaneous_V16_32_64NTester_Case25()
    : Vector2RegisterMiscellaneousTesterCase25(
      state_.Vector2RegisterMiscellaneous_V16_32_64N_VMOVN_instance_)
  {}
};

// Neutral case:
// inst(17:16)=10 & inst(10:6)=01001
//    = {baseline: 'Vector2RegisterMiscellaneous_I16_32_64N',
//       constraints: ,
//       rule: 'VQMOVUN',
//       safety: [inst(19:18)=11 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(7:6)=00 => DECODER_ERROR]}
//
// Representative case:
// A(17:16)=10 & B(10:6)=01001
//    = {Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//       constraints: ,
//       fields: [size(19:18), op(7:6), Vm(3:0)],
//       op: op(7:6),
//       rule: VQMOVUN,
//       safety: [op(7:6)=00 => DECODER_ERROR,
//         size(19:18)=11 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_I16_32_64NTester_Case26
    : public Vector2RegisterMiscellaneous_I16_32_64NTesterCase26 {
 public:
  Vector2RegisterMiscellaneous_I16_32_64NTester_Case26()
    : Vector2RegisterMiscellaneous_I16_32_64NTesterCase26(
      state_.Vector2RegisterMiscellaneous_I16_32_64N_VQMOVUN_instance_)
  {}
};

// Neutral case:
// inst(17:16)=10 & inst(10:6)=01100
//    = {baseline: 'Vector2RegisterMiscellaneous_I8_16_32L',
//       constraints: ,
//       defs: {},
//       rule: 'VSHLL_A2',
//       safety: [inst(19:18)=11 ||
//            inst(15:12)(0)=1 => UNDEFINED]}
//
// Representative case:
// A(17:16)=10 & B(10:6)=01100
//    = {Vd: Vd(15:12),
//       baseline: Vector2RegisterMiscellaneous_I8_16_32L,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12)],
//       rule: VSHLL_A2,
//       safety: [size(19:18)=11 ||
//            Vd(0)=1 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_I8_16_32LTester_Case27
    : public Vector2RegisterMiscellaneousTesterCase27 {
 public:
  Vector2RegisterMiscellaneous_I8_16_32LTester_Case27()
    : Vector2RegisterMiscellaneousTesterCase27(
      state_.Vector2RegisterMiscellaneous_I8_16_32L_VSHLL_A2_instance_)
  {}
};

// Neutral case:
// inst(17:16)=10 & inst(10:6)=11x00
//    = {baseline: 'Vector2RegisterMiscellaneous_CVT_H2S',
//       constraints: ,
//       rule: 'CVT_between_half_precision_and_single_precision',
//       safety: [inst(19:18)=~01 => UNDEFINED,
//         inst(8)=1 &&
//            inst(15:12)(0)=1 => UNDEFINED,
//         not inst(8)=1 &&
//            inst(3:0)(0)=1 => UNDEFINED]}
//
// Representative case:
// A(17:16)=10 & B(10:6)=11x00
//    = {Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_CVT_H2S,
//       constraints: ,
//       fields: [size(19:18), Vd(15:12), op(8), Vm(3:0)],
//       half_to_single: op(8)=1,
//       op: op(8),
//       rule: CVT_between_half_precision_and_single_precision,
//       safety: [size(19:18)=~01 => UNDEFINED,
//         half_to_single &&
//            Vd(0)=1 => UNDEFINED,
//         not half_to_single &&
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_CVT_H2STester_Case28
    : public Vector2RegisterMiscellaneous_CVT_H2STesterCase28 {
 public:
  Vector2RegisterMiscellaneous_CVT_H2STester_Case28()
    : Vector2RegisterMiscellaneous_CVT_H2STesterCase28(
      state_.Vector2RegisterMiscellaneous_CVT_H2S_CVT_between_half_precision_and_single_precision_instance_)
  {}
};

// Neutral case:
// inst(17:16)=10 & inst(10:6)=0000x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8S',
//       constraints: ,
//       defs: {},
//       rule: 'VSWP',
//       safety: [inst(19:18)=~00 => UNDEFINED,
//         inst(22):inst(15:12)  ==
//               inst(5):inst(3:0) => UNKNOWN,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=10 & B(10:6)=0000x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8S,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       m: M:Vm,
//       rule: VSWP,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8STester_Case29
    : public Vector2RegisterMiscellaneousTesterCase29 {
 public:
  Vector2RegisterMiscellaneous_V8STester_Case29()
    : Vector2RegisterMiscellaneousTesterCase29(
      state_.Vector2RegisterMiscellaneous_V8S_VSWP_instance_)
  {}
};

// Neutral case:
// inst(17:16)=10 & inst(10:6)=0001x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32T',
//       constraints: ,
//       defs: {},
//       rule: 'VTRN',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(22):inst(15:12)  ==
//               inst(5):inst(3:0) => UNKNOWN,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=10 & B(10:6)=0001x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32T,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       m: M:Vm,
//       rule: VTRN,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32TTester_Case30
    : public Vector2RegisterMiscellaneousTesterCase30 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32TTester_Case30()
    : Vector2RegisterMiscellaneousTesterCase30(
      state_.Vector2RegisterMiscellaneous_V8_16_32T_VTRN_instance_)
  {}
};

// Neutral case:
// inst(17:16)=10 & inst(10:6)=0010x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32I',
//       constraints: ,
//       defs: {},
//       rule: 'VUZP',
//       safety: [inst(19:18)=11 ||
//            (inst(6)=0 &&
//            inst(19:18)=10) => UNDEFINED,
//         inst(22):inst(15:12)  ==
//               inst(5):inst(3:0) => UNKNOWN,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=10 & B(10:6)=0010x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32I,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       m: M:Vm,
//       rule: VUZP,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 ||
//            (Q(6)=0 &&
//            size(19:18)=10) => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32ITester_Case31
    : public Vector2RegisterMiscellaneousTesterCase31 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32ITester_Case31()
    : Vector2RegisterMiscellaneousTesterCase31(
      state_.Vector2RegisterMiscellaneous_V8_16_32I_VUZP_instance_)
  {}
};

// Neutral case:
// inst(17:16)=10 & inst(10:6)=0011x
//    = {baseline: 'Vector2RegisterMiscellaneous_V8_16_32I',
//       constraints: ,
//       defs: {},
//       rule: 'VZIP',
//       safety: [inst(19:18)=11 ||
//            (inst(6)=0 &&
//            inst(19:18)=10) => UNDEFINED,
//         inst(22):inst(15:12)  ==
//               inst(5):inst(3:0) => UNKNOWN,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=10 & B(10:6)=0011x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_V8_16_32I,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       m: M:Vm,
//       rule: VZIP,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 ||
//            (Q(6)=0 &&
//            size(19:18)=10) => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_V8_16_32ITester_Case32
    : public Vector2RegisterMiscellaneousTesterCase32 {
 public:
  Vector2RegisterMiscellaneous_V8_16_32ITester_Case32()
    : Vector2RegisterMiscellaneousTesterCase32(
      state_.Vector2RegisterMiscellaneous_V8_16_32I_VZIP_instance_)
  {}
};

// Neutral case:
// inst(17:16)=10 & inst(10:6)=0101x
//    = {baseline: 'Vector2RegisterMiscellaneous_I16_32_64N',
//       constraints: ,
//       rule: 'VQMOVN',
//       safety: [inst(19:18)=11 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(7:6)=00 => DECODER_ERROR]}
//
// Representative case:
// A(17:16)=10 & B(10:6)=0101x
//    = {Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//       constraints: ,
//       fields: [size(19:18), op(7:6), Vm(3:0)],
//       op: op(7:6),
//       rule: VQMOVN,
//       safety: [op(7:6)=00 => DECODER_ERROR,
//         size(19:18)=11 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_I16_32_64NTester_Case33
    : public Vector2RegisterMiscellaneous_I16_32_64NTesterCase33 {
 public:
  Vector2RegisterMiscellaneous_I16_32_64NTester_Case33()
    : Vector2RegisterMiscellaneous_I16_32_64NTesterCase33(
      state_.Vector2RegisterMiscellaneous_I16_32_64N_VQMOVN_instance_)
  {}
};

// Neutral case:
// inst(17:16)=11 & inst(10:6)=10x0x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       rule: 'VRECPE',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=11 & B(10:6)=10x0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VRECPE,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_F32Tester_Case34
    : public Vector2RegisterMiscellaneousTesterCase34 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case34()
    : Vector2RegisterMiscellaneousTesterCase34(
      state_.Vector2RegisterMiscellaneous_F32_VRECPE_instance_)
  {}
};

// Neutral case:
// inst(17:16)=11 & inst(10:6)=10x1x
//    = {baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       rule: 'VRSQRTE',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=11 & B(10:6)=10x1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VRSQRTE,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_F32Tester_Case35
    : public Vector2RegisterMiscellaneousTesterCase35 {
 public:
  Vector2RegisterMiscellaneous_F32Tester_Case35()
    : Vector2RegisterMiscellaneousTesterCase35(
      state_.Vector2RegisterMiscellaneous_F32_VRSQRTE_instance_)
  {}
};

// Neutral case:
// inst(17:16)=11 & inst(10:6)=11xxx
//    = {baseline: 'Vector2RegisterMiscellaneous_CVT_F2I',
//       constraints: ,
//       defs: {},
//       rule: 'VCVT',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representative case:
// A(17:16)=11 & B(10:6)=11xxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       baseline: Vector2RegisterMiscellaneous_CVT_F2I,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       rule: VCVT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
class Vector2RegisterMiscellaneous_CVT_F2ITester_Case36
    : public Vector2RegisterMiscellaneousTesterCase36 {
 public:
  Vector2RegisterMiscellaneous_CVT_F2ITester_Case36()
    : Vector2RegisterMiscellaneousTesterCase36(
      state_.Vector2RegisterMiscellaneous_CVT_F2I_VCVT_instance_)
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
// inst(17:16)=00 & inst(10:6)=0000x
//    = {actual: 'Vector2RegisterMiscellaneous_RG',
//       baseline: 'Vector2RegisterMiscellaneous_RG',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss00dddd000ppqm0mmmm',
//       rule: 'VREV64',
//       safety: [3  <
//               inst(8:7) + inst(19:18) => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=0000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_RG,
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       op: op(8:7),
//       pattern: 111100111d11ss00dddd000ppqm0mmmm,
//       rule: VREV64,
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_RGTester_Case0_TestCase0) {
  Vector2RegisterMiscellaneous_RGTester_Case0 tester;
  tester.Test("111100111d11ss00dddd000ppqm0mmmm");
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=0001x
//    = {actual: 'Vector2RegisterMiscellaneous_RG',
//       baseline: 'Vector2RegisterMiscellaneous_RG',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss00dddd000ppqm0mmmm',
//       rule: 'VREV32',
//       safety: [3  <
//               inst(8:7) + inst(19:18) => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=0001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_RG,
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       op: op(8:7),
//       pattern: 111100111d11ss00dddd000ppqm0mmmm,
//       rule: VREV32,
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_RGTester_Case1_TestCase1) {
  Vector2RegisterMiscellaneous_RGTester_Case1 tester;
  tester.Test("111100111d11ss00dddd000ppqm0mmmm");
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=0010x
//    = {actual: 'Vector2RegisterMiscellaneous_RG',
//       baseline: 'Vector2RegisterMiscellaneous_RG',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss00dddd000ppqm0mmmm',
//       rule: 'VREV16',
//       safety: [3  <
//               inst(8:7) + inst(19:18) => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=0010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_RG,
//       baseline: Vector2RegisterMiscellaneous_RG,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), op(8:7), Q(6), Vm(3:0)],
//       op: op(8:7),
//       pattern: 111100111d11ss00dddd000ppqm0mmmm,
//       rule: VREV16,
//       safety: [op + size  >=
//               3 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_RGTester_Case2_TestCase2) {
  Vector2RegisterMiscellaneous_RGTester_Case2 tester;
  tester.Test("111100111d11ss00dddd000ppqm0mmmm");
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1000x
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss00dddd01000qm0mmmm',
//       rule: 'VCLS',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=1000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss00dddd01000qm0mmmm,
//       rule: VCLS,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case3_TestCase3) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case3 tester;
  tester.Test("111100111d11ss00dddd01000qm0mmmm");
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1001x
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss00dddd01001qm0mmmm',
//       rule: 'VCLZ',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=1001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss00dddd01001qm0mmmm,
//       rule: VCLZ,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case4_TestCase4) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case4 tester;
  tester.Test("111100111d11ss00dddd01001qm0mmmm");
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1010x
//    = {actual: 'Vector2RegisterMiscellaneous_V8',
//       baseline: 'Vector2RegisterMiscellaneous_V8',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss00dddd01010qm0mmmm',
//       rule: 'VCNT',
//       safety: [inst(19:18)=~00 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=1010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8,
//       baseline: Vector2RegisterMiscellaneous_V8,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss00dddd01010qm0mmmm,
//       rule: VCNT,
//       safety: [size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8Tester_Case5_TestCase5) {
  Vector2RegisterMiscellaneous_V8Tester_Case5 tester;
  tester.Test("111100111d11ss00dddd01010qm0mmmm");
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1011x
//    = {actual: 'Vector2RegisterMiscellaneous_V8',
//       baseline: 'Vector2RegisterMiscellaneous_V8',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss00dddd01011qm0mmmm',
//       rule: 'VMVN_register',
//       safety: [inst(19:18)=~00 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=1011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8,
//       baseline: Vector2RegisterMiscellaneous_V8,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss00dddd01011qm0mmmm,
//       rule: VMVN_register,
//       safety: [size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8Tester_Case6_TestCase6) {
  Vector2RegisterMiscellaneous_V8Tester_Case6 tester;
  tester.Test("111100111d11ss00dddd01011qm0mmmm");
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1110x
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss00dddd01110qm0mmmm',
//       rule: 'VQABS',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=1110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss00dddd01110qm0mmmm,
//       rule: VQABS,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case7_TestCase7) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case7 tester;
  tester.Test("111100111d11ss00dddd01110qm0mmmm");
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=1111x
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss00dddd01111qm0mmmm',
//       rule: 'VQNEG',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=1111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss00dddd01111qm0mmmm,
//       rule: VQNEG,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case8_TestCase8) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case8 tester;
  tester.Test("111100111d11ss00dddd01111qm0mmmm");
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=010xx & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss00dddd0010p1m0mmmm',
//       rule: 'VPADDL',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=010xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss00dddd0010p1m0mmmm,
//       rule: VPADDL,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case9_TestCase9) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case9 tester;
  tester.Test("111100111d11ss00dddd0010p1m0mmmm");
}

// Neutral case:
// inst(17:16)=00 & inst(10:6)=110xx & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss00dddd0110p1m0mmmm',
//       rule: 'VPADAL',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=00 & B(10:6)=110xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx1xxxxxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss00dddd0110p1m0mmmm,
//       rule: VPADAL,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case10_TestCase10) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case10 tester;
  tester.Test("111100111d11ss00dddd0110p1m0mmmm");
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0000x
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss01dddd0f000qm0mmmm',
//       rule: 'VCGT_immediate_0',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=0000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss01dddd0f000qm0mmmm,
//       rule: VCGT_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case11_TestCase11) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case11 tester;
  tester.Test("111100111d11ss01dddd0f000qm0mmmm");
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0001x
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss01dddd0f001qm0mmmm',
//       rule: 'VCGE_immediate_0',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=0001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss01dddd0f001qm0mmmm,
//       rule: VCGE_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case12_TestCase12) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case12 tester;
  tester.Test("111100111d11ss01dddd0f001qm0mmmm");
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0010x
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss01dddd0f010qm0mmmm',
//       rule: 'VCEQ_immediate_0',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=0010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss01dddd0f010qm0mmmm,
//       rule: VCEQ_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case13_TestCase13) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case13 tester;
  tester.Test("111100111d11ss01dddd0f010qm0mmmm");
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0011x
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss01dddd0f011qm0mmmm',
//       rule: 'VCLE_immediate_0',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=0011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss01dddd0f011qm0mmmm,
//       rule: VCLE_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case14_TestCase14) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case14 tester;
  tester.Test("111100111d11ss01dddd0f011qm0mmmm");
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0100x
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss01dddd0f100qm0mmmm',
//       rule: 'VCLT_immediate_0',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=0100x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss01dddd0f100qm0mmmm,
//       rule: VCLT_immediate_0,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case15_TestCase15) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case15 tester;
  tester.Test("111100111d11ss01dddd0f100qm0mmmm");
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0110x
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss01dddd0f110qm0mmmm',
//       rule: 'VABS_A1',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=0110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss01dddd0f110qm0mmmm,
//       rule: VABS_A1,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case16_TestCase16) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case16 tester;
  tester.Test("111100111d11ss01dddd0f110qm0mmmm");
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=0111x
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss01dddd0f111qm0mmmm',
//       rule: 'VNEG',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=0111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss01dddd0f111qm0mmmm,
//       rule: VNEG,
//       safety: [size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32Tester_Case17_TestCase17) {
  Vector2RegisterMiscellaneous_V8_16_32Tester_Case17 tester;
  tester.Test("111100111d11ss01dddd0f111qm0mmmm");
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1000x
//    = {actual: 'Vector2RegisterMiscellaneous_F32',
//       baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss01dddd0f000qm0mmmm',
//       rule: 'VCGT_immediate_0',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=1000x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_F32,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss01dddd0f000qm0mmmm,
//       rule: VCGT_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case18_TestCase18) {
  Vector2RegisterMiscellaneous_F32Tester_Case18 tester;
  tester.Test("111100111d11ss01dddd0f000qm0mmmm");
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1001x
//    = {actual: 'Vector2RegisterMiscellaneous_F32',
//       baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss01dddd0f001qm0mmmm',
//       rule: 'VCGE_immediate_0',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=1001x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_F32,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss01dddd0f001qm0mmmm,
//       rule: VCGE_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case19_TestCase19) {
  Vector2RegisterMiscellaneous_F32Tester_Case19 tester;
  tester.Test("111100111d11ss01dddd0f001qm0mmmm");
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1010x
//    = {actual: 'Vector2RegisterMiscellaneous_F32',
//       baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss01dddd0f010qm0mmmm',
//       rule: 'VCEQ_immediate_0',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=1010x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_F32,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss01dddd0f010qm0mmmm,
//       rule: VCEQ_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case20_TestCase20) {
  Vector2RegisterMiscellaneous_F32Tester_Case20 tester;
  tester.Test("111100111d11ss01dddd0f010qm0mmmm");
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1011x
//    = {actual: 'Vector2RegisterMiscellaneous_F32',
//       baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss01dddd0f011qm0mmmm',
//       rule: 'VCLE_immediate_0',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=1011x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_F32,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss01dddd0f011qm0mmmm,
//       rule: VCLE_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case21_TestCase21) {
  Vector2RegisterMiscellaneous_F32Tester_Case21 tester;
  tester.Test("111100111d11ss01dddd0f011qm0mmmm");
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1100x
//    = {actual: 'Vector2RegisterMiscellaneous_F32',
//       baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss01dddd0f100qm0mmmm',
//       rule: 'VCLT_immediate_0',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=1100x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_F32,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss01dddd0f100qm0mmmm,
//       rule: VCLT_immediate_0,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case22_TestCase22) {
  Vector2RegisterMiscellaneous_F32Tester_Case22 tester;
  tester.Test("111100111d11ss01dddd0f100qm0mmmm");
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1110x
//    = {actual: 'Vector2RegisterMiscellaneous_F32',
//       baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss01dddd0f110qm0mmmm',
//       rule: 'VABS_A1',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=1110x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_F32,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss01dddd0f110qm0mmmm,
//       rule: VABS_A1,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case23_TestCase23) {
  Vector2RegisterMiscellaneous_F32Tester_Case23 tester;
  tester.Test("111100111d11ss01dddd0f110qm0mmmm");
}

// Neutral case:
// inst(17:16)=01 & inst(10:6)=1111x
//    = {actual: 'Vector2RegisterMiscellaneous_F32',
//       baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss01dddd0f111qm0mmmm',
//       rule: 'VNEG',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=01 & B(10:6)=1111x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_F32,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss01dddd0f111qm0mmmm,
//       rule: VNEG,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case24_TestCase24) {
  Vector2RegisterMiscellaneous_F32Tester_Case24 tester;
  tester.Test("111100111d11ss01dddd0f111qm0mmmm");
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=01000
//    = {actual: 'Vector2RegisterMiscellaneous_V16_32_64N',
//       baseline: 'Vector2RegisterMiscellaneous_V16_32_64N',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss10dddd001000m0mmmm',
//       rule: 'VMOVN',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(3:0)(0)=1 => UNDEFINED]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=01000
//    = {Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V16_32_64N,
//       baseline: Vector2RegisterMiscellaneous_V16_32_64N,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vm(3:0)],
//       pattern: 111100111d11ss10dddd001000m0mmmm,
//       rule: VMOVN,
//       safety: [size(19:18)=11 => UNDEFINED, Vm(0)=1 => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V16_32_64NTester_Case25_TestCase25) {
  Vector2RegisterMiscellaneous_V16_32_64NTester_Case25 tester;
  tester.Test("111100111d11ss10dddd001000m0mmmm");
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=01001
//    = {actual: 'Vector2RegisterMiscellaneous_I16_32_64N',
//       baseline: 'Vector2RegisterMiscellaneous_I16_32_64N',
//       constraints: ,
//       pattern: '111100111d11ss10dddd0010ppm0mmmm',
//       rule: 'VQMOVUN',
//       safety: [inst(19:18)=11 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(7:6)=00 => DECODER_ERROR]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=01001
//    = {Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_I16_32_64N,
//       baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//       constraints: ,
//       fields: [size(19:18), op(7:6), Vm(3:0)],
//       op: op(7:6),
//       pattern: 111100111d11ss10dddd0010ppm0mmmm,
//       rule: VQMOVUN,
//       safety: [op(7:6)=00 => DECODER_ERROR,
//         size(19:18)=11 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_I16_32_64NTester_Case26_TestCase26) {
  Vector2RegisterMiscellaneous_I16_32_64NTester_Case26 tester;
  tester.Test("111100111d11ss10dddd0010ppm0mmmm");
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=01100
//    = {actual: 'Vector2RegisterMiscellaneous_I8_16_32L',
//       baseline: 'Vector2RegisterMiscellaneous_I8_16_32L',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss10dddd001100m0mmmm',
//       rule: 'VSHLL_A2',
//       safety: [inst(19:18)=11 ||
//            inst(15:12)(0)=1 => UNDEFINED]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=01100
//    = {Vd: Vd(15:12),
//       actual: Vector2RegisterMiscellaneous_I8_16_32L,
//       baseline: Vector2RegisterMiscellaneous_I8_16_32L,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12)],
//       pattern: 111100111d11ss10dddd001100m0mmmm,
//       rule: VSHLL_A2,
//       safety: [size(19:18)=11 ||
//            Vd(0)=1 => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_I8_16_32LTester_Case27_TestCase27) {
  Vector2RegisterMiscellaneous_I8_16_32LTester_Case27 tester;
  tester.Test("111100111d11ss10dddd001100m0mmmm");
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=11x00
//    = {actual: 'Vector2RegisterMiscellaneous_CVT_H2S',
//       baseline: 'Vector2RegisterMiscellaneous_CVT_H2S',
//       constraints: ,
//       pattern: '111100111d11ss10dddd011p00m0mmmm',
//       rule: 'CVT_between_half_precision_and_single_precision',
//       safety: [inst(19:18)=~01 => UNDEFINED,
//         inst(8)=1 &&
//            inst(15:12)(0)=1 => UNDEFINED,
//         not inst(8)=1 &&
//            inst(3:0)(0)=1 => UNDEFINED]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=11x00
//    = {Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_CVT_H2S,
//       baseline: Vector2RegisterMiscellaneous_CVT_H2S,
//       constraints: ,
//       fields: [size(19:18), Vd(15:12), op(8), Vm(3:0)],
//       half_to_single: op(8)=1,
//       op: op(8),
//       pattern: 111100111d11ss10dddd011p00m0mmmm,
//       rule: CVT_between_half_precision_and_single_precision,
//       safety: [size(19:18)=~01 => UNDEFINED,
//         half_to_single &&
//            Vd(0)=1 => UNDEFINED,
//         not half_to_single &&
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_CVT_H2STester_Case28_TestCase28) {
  Vector2RegisterMiscellaneous_CVT_H2STester_Case28 tester;
  tester.Test("111100111d11ss10dddd011p00m0mmmm");
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=0000x
//    = {actual: 'Vector2RegisterMiscellaneous_V8S',
//       baseline: 'Vector2RegisterMiscellaneous_V8S',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss10dddd00000qm0mmmm',
//       rule: 'VSWP',
//       safety: [inst(19:18)=~00 => UNDEFINED,
//         inst(22):inst(15:12)  ==
//               inst(5):inst(3:0) => UNKNOWN,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=0000x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8S,
//       baseline: Vector2RegisterMiscellaneous_V8S,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00000qm0mmmm,
//       rule: VSWP,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=~00 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8STester_Case29_TestCase29) {
  Vector2RegisterMiscellaneous_V8STester_Case29 tester;
  tester.Test("111100111d11ss10dddd00000qm0mmmm");
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=0001x
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32T',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32T',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss10dddd00001qm0mmmm',
//       rule: 'VTRN',
//       safety: [inst(19:18)=11 => UNDEFINED,
//         inst(22):inst(15:12)  ==
//               inst(5):inst(3:0) => UNKNOWN,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=0001x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32T,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32T,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00001qm0mmmm,
//       rule: VTRN,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32TTester_Case30_TestCase30) {
  Vector2RegisterMiscellaneous_V8_16_32TTester_Case30 tester;
  tester.Test("111100111d11ss10dddd00001qm0mmmm");
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=0010x
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32I',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32I',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss10dddd00010qm0mmmm',
//       rule: 'VUZP',
//       safety: [inst(19:18)=11 ||
//            (inst(6)=0 &&
//            inst(19:18)=10) => UNDEFINED,
//         inst(22):inst(15:12)  ==
//               inst(5):inst(3:0) => UNKNOWN,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=0010x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32I,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32I,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00010qm0mmmm,
//       rule: VUZP,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 ||
//            (Q(6)=0 &&
//            size(19:18)=10) => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32ITester_Case31_TestCase31) {
  Vector2RegisterMiscellaneous_V8_16_32ITester_Case31 tester;
  tester.Test("111100111d11ss10dddd00010qm0mmmm");
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=0011x
//    = {actual: 'Vector2RegisterMiscellaneous_V8_16_32I',
//       baseline: 'Vector2RegisterMiscellaneous_V8_16_32I',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss10dddd00011qm0mmmm',
//       rule: 'VZIP',
//       safety: [inst(19:18)=11 ||
//            (inst(6)=0 &&
//            inst(19:18)=10) => UNDEFINED,
//         inst(22):inst(15:12)  ==
//               inst(5):inst(3:0) => UNKNOWN,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=0011x
//    = {D: D(22),
//       M: M(5),
//       Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_V8_16_32I,
//       baseline: Vector2RegisterMiscellaneous_V8_16_32I,
//       constraints: ,
//       d: D:Vd,
//       defs: {},
//       fields: [D(22), size(19:18), Vd(15:12), Q(6), M(5), Vm(3:0)],
//       m: M:Vm,
//       pattern: 111100111d11ss10dddd00011qm0mmmm,
//       rule: VZIP,
//       safety: [d  ==
//               m => UNKNOWN,
//         size(19:18)=11 ||
//            (Q(6)=0 &&
//            size(19:18)=10) => UNDEFINED,
//         Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_V8_16_32ITester_Case32_TestCase32) {
  Vector2RegisterMiscellaneous_V8_16_32ITester_Case32 tester;
  tester.Test("111100111d11ss10dddd00011qm0mmmm");
}

// Neutral case:
// inst(17:16)=10 & inst(10:6)=0101x
//    = {actual: 'Vector2RegisterMiscellaneous_I16_32_64N',
//       baseline: 'Vector2RegisterMiscellaneous_I16_32_64N',
//       constraints: ,
//       pattern: '111100111d11ss10dddd0010ppm0mmmm',
//       rule: 'VQMOVN',
//       safety: [inst(19:18)=11 ||
//            inst(3:0)(0)=1 => UNDEFINED,
//         inst(7:6)=00 => DECODER_ERROR]}
//
// Representaive case:
// A(17:16)=10 & B(10:6)=0101x
//    = {Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_I16_32_64N,
//       baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//       constraints: ,
//       fields: [size(19:18), op(7:6), Vm(3:0)],
//       op: op(7:6),
//       pattern: 111100111d11ss10dddd0010ppm0mmmm,
//       rule: VQMOVN,
//       safety: [op(7:6)=00 => DECODER_ERROR,
//         size(19:18)=11 ||
//            Vm(0)=1 => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_I16_32_64NTester_Case33_TestCase33) {
  Vector2RegisterMiscellaneous_I16_32_64NTester_Case33 tester;
  tester.Test("111100111d11ss10dddd0010ppm0mmmm");
}

// Neutral case:
// inst(17:16)=11 & inst(10:6)=10x0x
//    = {actual: 'Vector2RegisterMiscellaneous_F32',
//       baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss11dddd010f0qm0mmmm',
//       rule: 'VRECPE',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=11 & B(10:6)=10x0x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_F32,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss11dddd010f0qm0mmmm,
//       rule: VRECPE,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case34_TestCase34) {
  Vector2RegisterMiscellaneous_F32Tester_Case34 tester;
  tester.Test("111100111d11ss11dddd010f0qm0mmmm");
}

// Neutral case:
// inst(17:16)=11 & inst(10:6)=10x1x
//    = {actual: 'Vector2RegisterMiscellaneous_F32',
//       baseline: 'Vector2RegisterMiscellaneous_F32',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss11dddd010f1qm0mmmm',
//       rule: 'VRSQRTE',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=11 & B(10:6)=10x1x
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_F32,
//       baseline: Vector2RegisterMiscellaneous_F32,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss11dddd010f1qm0mmmm,
//       rule: VRSQRTE,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_F32Tester_Case35_TestCase35) {
  Vector2RegisterMiscellaneous_F32Tester_Case35 tester;
  tester.Test("111100111d11ss11dddd010f1qm0mmmm");
}

// Neutral case:
// inst(17:16)=11 & inst(10:6)=11xxx
//    = {actual: 'Vector2RegisterMiscellaneous_CVT_F2I',
//       baseline: 'Vector2RegisterMiscellaneous_CVT_F2I',
//       constraints: ,
//       defs: {},
//       pattern: '111100111d11ss11dddd011ppqm0mmmm',
//       rule: 'VCVT',
//       safety: [inst(19:18)=~10 => UNDEFINED,
//         inst(6)=1 &&
//            (inst(15:12)(0)=1 ||
//            inst(3:0)(0)=1) => UNDEFINED]}
//
// Representaive case:
// A(17:16)=11 & B(10:6)=11xxx
//    = {Q: Q(6),
//       Vd: Vd(15:12),
//       Vm: Vm(3:0),
//       actual: Vector2RegisterMiscellaneous_CVT_F2I,
//       baseline: Vector2RegisterMiscellaneous_CVT_F2I,
//       constraints: ,
//       defs: {},
//       fields: [size(19:18), Vd(15:12), Q(6), Vm(3:0)],
//       pattern: 111100111d11ss11dddd011ppqm0mmmm,
//       rule: VCVT,
//       safety: [Q(6)=1 &&
//            (Vd(0)=1 ||
//            Vm(0)=1) => UNDEFINED,
//         size(19:18)=~10 => UNDEFINED],
//       size: size(19:18)}
TEST_F(Arm32DecoderStateTests,
       Vector2RegisterMiscellaneous_CVT_F2ITester_Case36_TestCase36) {
  Vector2RegisterMiscellaneous_CVT_F2ITester_Case36 tester;
  tester.Test("111100111d11ss11dddd011ppqm0mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
