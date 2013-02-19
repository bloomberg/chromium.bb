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
#include "native_client/src/trusted/validator_arm/baseline_vs_baseline.h"
#include "native_client/src/trusted/validator_arm/baseline_classes.h"
#include "native_client/src/trusted/validator_arm/inst_classes_testers.h"
#include "native_client/src/trusted/validator_arm/arm_helpers.h"
#include "native_client/src/trusted/validator_arm/gen/arm32_decode_named_bases.h"

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


// L(20)=0 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {None: 32,
//       Pc: 15,
//       Rt: Rt(15:12),
//       actual: Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1,
//       baseline: MoveVfpRegisterOp,
//       constraints: ,
//       defs: {Rt
//            if to_arm_register
//            else None},
//       fields: [op(20), Rt(15:12)],
//       generated_baseline: VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_0,
//       op: op(20),
//       pattern: cccc1110000onnnntttt1010n0010000,
//       rule: VMOV_between_ARM_core_register_and_single_precision_register,
//       safety: [t  ==
//               Pc => UNPREDICTABLE],
//       t: Rt,
//       to_arm_register: op(20)=1,
//       uses: {Rt
//            if not to_arm_register
//            else None}}
class MoveVfpRegisterOpTesterCase0
    : public MoveVfpRegisterOpTester {
 public:
  MoveVfpRegisterOpTesterCase0(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool MoveVfpRegisterOpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(20)=~0
  if ((inst.Bits() & 0x00100000)  !=
          0x00000000) return false;
  // C(8)=~0
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;
  // A(23:21)=~000
  if ((inst.Bits() & 0x00E00000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
  if ((inst.Bits() & 0x0000006F)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool MoveVfpRegisterOpTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(MoveVfpRegisterOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: t  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // defs: {Rt
  //       if to_arm_register
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? ((inst.Bits() & 0x0000F000) >> 12)
       : 32)))));

  return true;
}

// L(20)=0 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       actual: Actual_VMSR_cccc111011100001tttt101000010000_case_1,
//       baseline: VfpUsesRegOp,
//       constraints: ,
//       defs: {},
//       fields: [Rt(15:12)],
//       generated_baseline: VMSR_cccc111011100001tttt101000010000_case_0,
//       pattern: cccc111011100001tttt101000010000,
//       rule: VMSR,
//       safety: [t  ==
//               Pc => UNPREDICTABLE],
//       t: Rt,
//       uses: {Rt}}
class VfpUsesRegOpTesterCase1
    : public VfpUsesRegOpTester {
 public:
  VfpUsesRegOpTesterCase1(const NamedClassDecoder& decoder)
    : VfpUsesRegOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VfpUsesRegOpTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(20)=~0
  if ((inst.Bits() & 0x00100000)  !=
          0x00000000) return false;
  // C(8)=~0
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;
  // A(23:21)=~111
  if ((inst.Bits() & 0x00E00000)  !=
          0x00E00000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0001xxxxxxxx000x0000
  if ((inst.Bits() & 0x000F00EF)  !=
          0x00010000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return VfpUsesRegOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool VfpUsesRegOpTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VfpUsesRegOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: t  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// L(20)=0 & C(8)=1 & A(23:21)=0xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       actual: Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1,
//       baseline: MoveVfpRegisterOpWithTypeSel,
//       constraints: ,
//       defs: {},
//       fields: [opc1(22:21), Rt(15:12), opc2(6:5)],
//       generated_baseline: VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_0,
//       opc1: opc1(22:21),
//       opc2: opc2(6:5),
//       pattern: cccc11100ii0ddddtttt1011dii10000,
//       rule: VMOV_ARM_core_register_to_scalar,
//       safety: [opc1:opc2(3:0)=0x10 => UNDEFINED,
//         t  ==
//               Pc => UNPREDICTABLE],
//       t: Rt,
//       uses: {Rt}}
class MoveVfpRegisterOpWithTypeSelTesterCase2
    : public MoveVfpRegisterOpWithTypeSelTester {
 public:
  MoveVfpRegisterOpWithTypeSelTesterCase2(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpWithTypeSelTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool MoveVfpRegisterOpWithTypeSelTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(20)=~0
  if ((inst.Bits() & 0x00100000)  !=
          0x00000000) return false;
  // C(8)=~1
  if ((inst.Bits() & 0x00000100)  !=
          0x00000100) return false;
  // A(23:21)=~0xx
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
  if ((inst.Bits() & 0x0000000F)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpWithTypeSelTester::
      PassesParsePreconditions(inst, decoder);
}

bool MoveVfpRegisterOpWithTypeSelTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(MoveVfpRegisterOpWithTypeSelTester::
               ApplySanityChecks(inst, decoder));

  // safety: opc1:opc2(3:0)=0x10 => UNDEFINED
  EXPECT_TRUE(((((((inst.Bits() & 0x00600000) >> 21)) << 2) | ((inst.Bits() & 0x00000060) >> 5)) & 0x0000000B)  !=
          0x00000002);

  // safety: t  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// L(20)=0 & C(8)=1 & A(23:21)=1xx & B(6:5)=0x & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {B: B(22),
//       E: E(5),
//       Pc: 15,
//       Q: Q(21),
//       Rt: Rt(15:12),
//       Vd: Vd(19:16),
//       actual: Actual_VDUP_ARM_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1,
//       baseline: DuplicateToAdvSIMDRegisters,
//       cond: cond(31:28),
//       cond_AL: 14,
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28), B(22), Q(21), Vd(19:16), Rt(15:12), E(5)],
//       generated_baseline: VDUP_ARM_core_register_cccc11101bq0ddddtttt1011d0e10000_case_0,
//       pattern: cccc11101bq0ddddtttt1011d0e10000,
//       rule: VDUP_ARM_core_register,
//       safety: [cond  !=
//               cond_AL => DEPRECATED,
//         Q(21)=1 &&
//            Vd(0)=1 => UNDEFINED,
//         B:E(1:0)=11 => UNDEFINED,
//         t  ==
//               Pc => UNPREDICTABLE],
//       t: Rt,
//       uses: {Rt}}
class DuplicateToAdvSIMDRegistersTesterCase3
    : public DuplicateToAdvSIMDRegistersTester {
 public:
  DuplicateToAdvSIMDRegistersTesterCase3(const NamedClassDecoder& decoder)
    : DuplicateToAdvSIMDRegistersTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool DuplicateToAdvSIMDRegistersTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(20)=~0
  if ((inst.Bits() & 0x00100000)  !=
          0x00000000) return false;
  // C(8)=~1
  if ((inst.Bits() & 0x00000100)  !=
          0x00000100) return false;
  // A(23:21)=~1xx
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(6:5)=~0x
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
  if ((inst.Bits() & 0x0000000F)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return DuplicateToAdvSIMDRegistersTester::
      PassesParsePreconditions(inst, decoder);
}

bool DuplicateToAdvSIMDRegistersTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(DuplicateToAdvSIMDRegistersTester::
               ApplySanityChecks(inst, decoder));

  // safety: cond  !=
  //          cond_AL => DEPRECATED
  EXPECT_TRUE(((((inst.Bits() & 0xF0000000) >> 28)) == (14)));

  // safety: Q(21)=1 &&
  //       Vd(0)=1 => UNDEFINED
  EXPECT_TRUE(!(((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001)));

  // safety: B:E(1:0)=11 => UNDEFINED
  EXPECT_TRUE(((((((inst.Bits() & 0x00400000) >> 22)) << 1) | ((inst.Bits() & 0x00000020) >> 5)) & 0x00000003)  !=
          0x00000003);

  // safety: t  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// L(20)=1 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {None: 32,
//       Pc: 15,
//       Rt: Rt(15:12),
//       actual: Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1,
//       baseline: MoveVfpRegisterOp,
//       constraints: ,
//       defs: {Rt
//            if to_arm_register
//            else None},
//       fields: [op(20), Rt(15:12)],
//       generated_baseline: VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000xnnnntttt1010n0010000_case_0,
//       op: op(20),
//       pattern: cccc1110000xnnnntttt1010n0010000,
//       rule: VMOV_between_ARM_core_register_and_single_precision_register,
//       safety: [t  ==
//               Pc => UNPREDICTABLE],
//       t: Rt,
//       to_arm_register: op(20)=1,
//       uses: {Rt
//            if not to_arm_register
//            else None}}
class MoveVfpRegisterOpTesterCase4
    : public MoveVfpRegisterOpTester {
 public:
  MoveVfpRegisterOpTesterCase4(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool MoveVfpRegisterOpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(20)=~1
  if ((inst.Bits() & 0x00100000)  !=
          0x00100000) return false;
  // C(8)=~0
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;
  // A(23:21)=~000
  if ((inst.Bits() & 0x00E00000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
  if ((inst.Bits() & 0x0000006F)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool MoveVfpRegisterOpTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(MoveVfpRegisterOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: t  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // defs: {Rt
  //       if to_arm_register
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? ((inst.Bits() & 0x0000F000) >> 12)
       : 32)))));

  return true;
}

// L(20)=1 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {NZCV: 16,
//       Pc: 15,
//       Rt: Rt(15:12),
//       actual: Actual_VMRS_cccc111011110001tttt101000010000_case_1,
//       baseline: VfpMrsOp,
//       constraints: ,
//       defs: {NZCV
//            if t  ==
//               Pc
//            else Rt},
//       fields: [Rt(15:12)],
//       generated_baseline: VMRS_cccc111011110001tttt101000010000_case_0,
//       pattern: cccc111011110001tttt101000010000,
//       rule: VMRS,
//       t: Rt}
class VfpMrsOpTesterCase5
    : public VfpMrsOpTester {
 public:
  VfpMrsOpTesterCase5(const NamedClassDecoder& decoder)
    : VfpMrsOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool VfpMrsOpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(20)=~1
  if ((inst.Bits() & 0x00100000)  !=
          0x00100000) return false;
  // C(8)=~0
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;
  // A(23:21)=~111
  if ((inst.Bits() & 0x00E00000)  !=
          0x00E00000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0001xxxxxxxx000x0000
  if ((inst.Bits() & 0x000F00EF)  !=
          0x00010000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return VfpMrsOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool VfpMrsOpTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(VfpMrsOpTester::
               ApplySanityChecks(inst, decoder));

  // defs: {NZCV
  //       if t  ==
  //          Pc
  //       else Rt};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register((((((inst.Bits() & 0x0000F000) >> 12)) == (15))
       ? 16
       : ((inst.Bits() & 0x0000F000) >> 12))))));

  return true;
}

// L(20)=1 & C(8)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       U: U(23),
//       actual: Actual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1,
//       baseline: MoveVfpRegisterOpWithTypeSel,
//       constraints: ,
//       defs: {Rt},
//       fields: [U(23), opc1(22:21), Rt(15:12), opc2(6:5)],
//       generated_baseline: MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_0,
//       opc1: opc1(22:21),
//       opc2: opc2(6:5),
//       pattern: cccc1110iii1nnnntttt1011nii10000,
//       rule: MOVE_scalar_to_ARM_core_register,
//       safety: [sel in bitset {'10x00', 'x0x10'} => UNDEFINED,
//         t  ==
//               Pc => UNPREDICTABLE],
//       sel: U:opc1:opc2,
//       t: Rt}
class MoveVfpRegisterOpWithTypeSelTesterCase6
    : public MoveVfpRegisterOpWithTypeSelTester {
 public:
  MoveVfpRegisterOpWithTypeSelTesterCase6(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpWithTypeSelTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool MoveVfpRegisterOpWithTypeSelTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(20)=~1
  if ((inst.Bits() & 0x00100000)  !=
          0x00100000) return false;
  // C(8)=~1
  if ((inst.Bits() & 0x00000100)  !=
          0x00000100) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
  if ((inst.Bits() & 0x0000000F)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpWithTypeSelTester::
      PassesParsePreconditions(inst, decoder);
}

bool MoveVfpRegisterOpWithTypeSelTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(MoveVfpRegisterOpWithTypeSelTester::
               ApplySanityChecks(inst, decoder));

  // safety: sel in bitset {'10x00', 'x0x10'} => UNDEFINED
  EXPECT_TRUE(!(((((((((((inst.Bits() & 0x00800000) >> 23)) << 2) | ((inst.Bits() & 0x00600000) >> 21))) << 2) | ((inst.Bits() & 0x00000060) >> 5)) & 0x0000001B)  ==
          0x00000010) ||
       ((((((((((inst.Bits() & 0x00800000) >> 23)) << 2) | ((inst.Bits() & 0x00600000) >> 21))) << 2) | ((inst.Bits() & 0x00000060) >> 5)) & 0x0000000B)  ==
          0x00000002)));

  // safety: t  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // defs: {Rt};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// L(20)=0 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {None: 32,
//       Pc: 15,
//       Rt: Rt(15:12),
//       actual: Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1,
//       baseline: MoveVfpRegisterOp,
//       constraints: ,
//       defs: {Rt
//            if to_arm_register
//            else None},
//       fields: [op(20), Rt(15:12)],
//       generated_baseline: VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_0,
//       op: op(20),
//       pattern: cccc1110000onnnntttt1010n0010000,
//       rule: VMOV_between_ARM_core_register_and_single_precision_register,
//       safety: [t  ==
//               Pc => UNPREDICTABLE],
//       t: Rt,
//       to_arm_register: op(20)=1,
//       uses: {Rt
//            if not to_arm_register
//            else None}}
class MoveVfpRegisterOpTester_Case0
    : public MoveVfpRegisterOpTesterCase0 {
 public:
  MoveVfpRegisterOpTester_Case0()
    : MoveVfpRegisterOpTesterCase0(
      state_.MoveVfpRegisterOp_VMOV_between_ARM_core_register_and_single_precision_register_instance_)
  {}
};

// L(20)=0 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       actual: Actual_VMSR_cccc111011100001tttt101000010000_case_1,
//       baseline: VfpUsesRegOp,
//       constraints: ,
//       defs: {},
//       fields: [Rt(15:12)],
//       generated_baseline: VMSR_cccc111011100001tttt101000010000_case_0,
//       pattern: cccc111011100001tttt101000010000,
//       rule: VMSR,
//       safety: [t  ==
//               Pc => UNPREDICTABLE],
//       t: Rt,
//       uses: {Rt}}
class VfpUsesRegOpTester_Case1
    : public VfpUsesRegOpTesterCase1 {
 public:
  VfpUsesRegOpTester_Case1()
    : VfpUsesRegOpTesterCase1(
      state_.VfpUsesRegOp_VMSR_instance_)
  {}
};

// L(20)=0 & C(8)=1 & A(23:21)=0xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       actual: Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1,
//       baseline: MoveVfpRegisterOpWithTypeSel,
//       constraints: ,
//       defs: {},
//       fields: [opc1(22:21), Rt(15:12), opc2(6:5)],
//       generated_baseline: VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_0,
//       opc1: opc1(22:21),
//       opc2: opc2(6:5),
//       pattern: cccc11100ii0ddddtttt1011dii10000,
//       rule: VMOV_ARM_core_register_to_scalar,
//       safety: [opc1:opc2(3:0)=0x10 => UNDEFINED,
//         t  ==
//               Pc => UNPREDICTABLE],
//       t: Rt,
//       uses: {Rt}}
class MoveVfpRegisterOpWithTypeSelTester_Case2
    : public MoveVfpRegisterOpWithTypeSelTesterCase2 {
 public:
  MoveVfpRegisterOpWithTypeSelTester_Case2()
    : MoveVfpRegisterOpWithTypeSelTesterCase2(
      state_.MoveVfpRegisterOpWithTypeSel_VMOV_ARM_core_register_to_scalar_instance_)
  {}
};

// L(20)=0 & C(8)=1 & A(23:21)=1xx & B(6:5)=0x & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {B: B(22),
//       E: E(5),
//       Pc: 15,
//       Q: Q(21),
//       Rt: Rt(15:12),
//       Vd: Vd(19:16),
//       actual: Actual_VDUP_ARM_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1,
//       baseline: DuplicateToAdvSIMDRegisters,
//       cond: cond(31:28),
//       cond_AL: 14,
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28), B(22), Q(21), Vd(19:16), Rt(15:12), E(5)],
//       generated_baseline: VDUP_ARM_core_register_cccc11101bq0ddddtttt1011d0e10000_case_0,
//       pattern: cccc11101bq0ddddtttt1011d0e10000,
//       rule: VDUP_ARM_core_register,
//       safety: [cond  !=
//               cond_AL => DEPRECATED,
//         Q(21)=1 &&
//            Vd(0)=1 => UNDEFINED,
//         B:E(1:0)=11 => UNDEFINED,
//         t  ==
//               Pc => UNPREDICTABLE],
//       t: Rt,
//       uses: {Rt}}
class DuplicateToAdvSIMDRegistersTester_Case3
    : public DuplicateToAdvSIMDRegistersTesterCase3 {
 public:
  DuplicateToAdvSIMDRegistersTester_Case3()
    : DuplicateToAdvSIMDRegistersTesterCase3(
      state_.DuplicateToAdvSIMDRegisters_VDUP_ARM_core_register_instance_)
  {}
};

// L(20)=1 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {None: 32,
//       Pc: 15,
//       Rt: Rt(15:12),
//       actual: Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1,
//       baseline: MoveVfpRegisterOp,
//       constraints: ,
//       defs: {Rt
//            if to_arm_register
//            else None},
//       fields: [op(20), Rt(15:12)],
//       generated_baseline: VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000xnnnntttt1010n0010000_case_0,
//       op: op(20),
//       pattern: cccc1110000xnnnntttt1010n0010000,
//       rule: VMOV_between_ARM_core_register_and_single_precision_register,
//       safety: [t  ==
//               Pc => UNPREDICTABLE],
//       t: Rt,
//       to_arm_register: op(20)=1,
//       uses: {Rt
//            if not to_arm_register
//            else None}}
class MoveVfpRegisterOpTester_Case4
    : public MoveVfpRegisterOpTesterCase4 {
 public:
  MoveVfpRegisterOpTester_Case4()
    : MoveVfpRegisterOpTesterCase4(
      state_.MoveVfpRegisterOp_VMOV_between_ARM_core_register_and_single_precision_register_instance_)
  {}
};

// L(20)=1 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {NZCV: 16,
//       Pc: 15,
//       Rt: Rt(15:12),
//       actual: Actual_VMRS_cccc111011110001tttt101000010000_case_1,
//       baseline: VfpMrsOp,
//       constraints: ,
//       defs: {NZCV
//            if t  ==
//               Pc
//            else Rt},
//       fields: [Rt(15:12)],
//       generated_baseline: VMRS_cccc111011110001tttt101000010000_case_0,
//       pattern: cccc111011110001tttt101000010000,
//       rule: VMRS,
//       t: Rt}
class VfpMrsOpTester_Case5
    : public VfpMrsOpTesterCase5 {
 public:
  VfpMrsOpTester_Case5()
    : VfpMrsOpTesterCase5(
      state_.VfpMrsOp_VMRS_instance_)
  {}
};

// L(20)=1 & C(8)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       U: U(23),
//       actual: Actual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1,
//       baseline: MoveVfpRegisterOpWithTypeSel,
//       constraints: ,
//       defs: {Rt},
//       fields: [U(23), opc1(22:21), Rt(15:12), opc2(6:5)],
//       generated_baseline: MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_0,
//       opc1: opc1(22:21),
//       opc2: opc2(6:5),
//       pattern: cccc1110iii1nnnntttt1011nii10000,
//       rule: MOVE_scalar_to_ARM_core_register,
//       safety: [sel in bitset {'10x00', 'x0x10'} => UNDEFINED,
//         t  ==
//               Pc => UNPREDICTABLE],
//       sel: U:opc1:opc2,
//       t: Rt}
class MoveVfpRegisterOpWithTypeSelTester_Case6
    : public MoveVfpRegisterOpWithTypeSelTesterCase6 {
 public:
  MoveVfpRegisterOpWithTypeSelTester_Case6()
    : MoveVfpRegisterOpWithTypeSelTesterCase6(
      state_.MoveVfpRegisterOpWithTypeSel_MOVE_scalar_to_ARM_core_register_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// L(20)=0 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {None: 32,
//       Pc: 15,
//       Rt: Rt(15:12),
//       actual: Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1,
//       baseline: MoveVfpRegisterOp,
//       constraints: ,
//       defs: {Rt
//            if to_arm_register
//            else None},
//       fields: [op(20), Rt(15:12)],
//       generated_baseline: VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_0,
//       op: op(20),
//       pattern: cccc1110000onnnntttt1010n0010000,
//       rule: VMOV_between_ARM_core_register_and_single_precision_register,
//       safety: [t  ==
//               Pc => UNPREDICTABLE],
//       t: Rt,
//       to_arm_register: op(20)=1,
//       uses: {Rt
//            if not to_arm_register
//            else None}}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpTester_Case0_TestCase0) {
  MoveVfpRegisterOpTester_Case0 baseline_tester;
  NamedActual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1_VMOV_between_ARM_core_register_and_single_precision_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1110000onnnntttt1010n0010000");
}

// L(20)=0 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       actual: Actual_VMSR_cccc111011100001tttt101000010000_case_1,
//       baseline: VfpUsesRegOp,
//       constraints: ,
//       defs: {},
//       fields: [Rt(15:12)],
//       generated_baseline: VMSR_cccc111011100001tttt101000010000_case_0,
//       pattern: cccc111011100001tttt101000010000,
//       rule: VMSR,
//       safety: [t  ==
//               Pc => UNPREDICTABLE],
//       t: Rt,
//       uses: {Rt}}
TEST_F(Arm32DecoderStateTests,
       VfpUsesRegOpTester_Case1_TestCase1) {
  VfpUsesRegOpTester_Case1 baseline_tester;
  NamedActual_VMSR_cccc111011100001tttt101000010000_case_1_VMSR actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc111011100001tttt101000010000");
}

// L(20)=0 & C(8)=1 & A(23:21)=0xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       actual: Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1,
//       baseline: MoveVfpRegisterOpWithTypeSel,
//       constraints: ,
//       defs: {},
//       fields: [opc1(22:21), Rt(15:12), opc2(6:5)],
//       generated_baseline: VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_0,
//       opc1: opc1(22:21),
//       opc2: opc2(6:5),
//       pattern: cccc11100ii0ddddtttt1011dii10000,
//       rule: VMOV_ARM_core_register_to_scalar,
//       safety: [opc1:opc2(3:0)=0x10 => UNDEFINED,
//         t  ==
//               Pc => UNPREDICTABLE],
//       t: Rt,
//       uses: {Rt}}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpWithTypeSelTester_Case2_TestCase2) {
  MoveVfpRegisterOpWithTypeSelTester_Case2 baseline_tester;
  NamedActual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1_VMOV_ARM_core_register_to_scalar actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100ii0ddddtttt1011dii10000");
}

// L(20)=0 & C(8)=1 & A(23:21)=1xx & B(6:5)=0x & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {B: B(22),
//       E: E(5),
//       Pc: 15,
//       Q: Q(21),
//       Rt: Rt(15:12),
//       Vd: Vd(19:16),
//       actual: Actual_VDUP_ARM_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1,
//       baseline: DuplicateToAdvSIMDRegisters,
//       cond: cond(31:28),
//       cond_AL: 14,
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28), B(22), Q(21), Vd(19:16), Rt(15:12), E(5)],
//       generated_baseline: VDUP_ARM_core_register_cccc11101bq0ddddtttt1011d0e10000_case_0,
//       pattern: cccc11101bq0ddddtttt1011d0e10000,
//       rule: VDUP_ARM_core_register,
//       safety: [cond  !=
//               cond_AL => DEPRECATED,
//         Q(21)=1 &&
//            Vd(0)=1 => UNDEFINED,
//         B:E(1:0)=11 => UNDEFINED,
//         t  ==
//               Pc => UNPREDICTABLE],
//       t: Rt,
//       uses: {Rt}}
TEST_F(Arm32DecoderStateTests,
       DuplicateToAdvSIMDRegistersTester_Case3_TestCase3) {
  DuplicateToAdvSIMDRegistersTester_Case3 baseline_tester;
  NamedActual_VDUP_ARM_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1_VDUP_ARM_core_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101bq0ddddtttt1011d0e10000");
}

// L(20)=1 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {None: 32,
//       Pc: 15,
//       Rt: Rt(15:12),
//       actual: Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1,
//       baseline: MoveVfpRegisterOp,
//       constraints: ,
//       defs: {Rt
//            if to_arm_register
//            else None},
//       fields: [op(20), Rt(15:12)],
//       generated_baseline: VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000xnnnntttt1010n0010000_case_0,
//       op: op(20),
//       pattern: cccc1110000xnnnntttt1010n0010000,
//       rule: VMOV_between_ARM_core_register_and_single_precision_register,
//       safety: [t  ==
//               Pc => UNPREDICTABLE],
//       t: Rt,
//       to_arm_register: op(20)=1,
//       uses: {Rt
//            if not to_arm_register
//            else None}}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpTester_Case4_TestCase4) {
  MoveVfpRegisterOpTester_Case4 baseline_tester;
  NamedActual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1_VMOV_between_ARM_core_register_and_single_precision_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1110000xnnnntttt1010n0010000");
}

// L(20)=1 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {NZCV: 16,
//       Pc: 15,
//       Rt: Rt(15:12),
//       actual: Actual_VMRS_cccc111011110001tttt101000010000_case_1,
//       baseline: VfpMrsOp,
//       constraints: ,
//       defs: {NZCV
//            if t  ==
//               Pc
//            else Rt},
//       fields: [Rt(15:12)],
//       generated_baseline: VMRS_cccc111011110001tttt101000010000_case_0,
//       pattern: cccc111011110001tttt101000010000,
//       rule: VMRS,
//       t: Rt}
TEST_F(Arm32DecoderStateTests,
       VfpMrsOpTester_Case5_TestCase5) {
  VfpMrsOpTester_Case5 baseline_tester;
  NamedActual_VMRS_cccc111011110001tttt101000010000_case_1_VMRS actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc111011110001tttt101000010000");
}

// L(20)=1 & C(8)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {Pc: 15,
//       Rt: Rt(15:12),
//       U: U(23),
//       actual: Actual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1,
//       baseline: MoveVfpRegisterOpWithTypeSel,
//       constraints: ,
//       defs: {Rt},
//       fields: [U(23), opc1(22:21), Rt(15:12), opc2(6:5)],
//       generated_baseline: MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_0,
//       opc1: opc1(22:21),
//       opc2: opc2(6:5),
//       pattern: cccc1110iii1nnnntttt1011nii10000,
//       rule: MOVE_scalar_to_ARM_core_register,
//       safety: [sel in bitset {'10x00', 'x0x10'} => UNDEFINED,
//         t  ==
//               Pc => UNPREDICTABLE],
//       sel: U:opc1:opc2,
//       t: Rt}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpWithTypeSelTester_Case6_TestCase6) {
  MoveVfpRegisterOpWithTypeSelTester_Case6 baseline_tester;
  NamedActual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1_MOVE_scalar_to_ARM_core_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1110iii1nnnntttt1011nii10000");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
