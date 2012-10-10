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
// inst(20)=0 & inst(8)=0 & inst(23:21)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {baseline: 'MoveVfpRegisterOp',
//       constraints: }
//
// Representaive case:
// L(20)=0 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {baseline: MoveVfpRegisterOp,
//       constraints: }
class MoveVfpRegisterOpTesterCase0
    : public MoveVfpRegisterOpTester {
 public:
  MoveVfpRegisterOpTesterCase0(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveVfpRegisterOpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00100000) != 0x00000000 /* L(20)=~0 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000000 /* C(8)=~0 */) return false;
  if ((inst.Bits() & 0x00E00000) != 0x00000000 /* A(23:21)=~000 */) return false;
  if ((inst.Bits() & 0x0000006F) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxx00x0000 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(20)=0 & inst(8)=0 & inst(23:21)=111 & inst(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {baseline: 'VfpUsesRegOp',
//       constraints: }
//
// Representaive case:
// L(20)=0 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {baseline: VfpUsesRegOp,
//       constraints: }
class VfpUsesRegOpTesterCase1
    : public VfpUsesRegOpTester {
 public:
  VfpUsesRegOpTesterCase1(const NamedClassDecoder& decoder)
    : VfpUsesRegOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VfpUsesRegOpTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00100000) != 0x00000000 /* L(20)=~0 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000000 /* C(8)=~0 */) return false;
  if ((inst.Bits() & 0x00E00000) != 0x00E00000 /* A(23:21)=~111 */) return false;
  if ((inst.Bits() & 0x000F00EF) != 0x00010000 /* $pattern(31:0)=~xxxxxxxxxxxx0001xxxxxxxx000x0000 */) return false;

  // Check other preconditions defined for the base decoder.
  return VfpUsesRegOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(20)=0 & inst(8)=1 & inst(23:21)=0xx & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {baseline: 'MoveVfpRegisterOpWithTypeSel',
//       constraints: }
//
// Representaive case:
// L(20)=0 & C(8)=1 & A(23:21)=0xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {baseline: MoveVfpRegisterOpWithTypeSel,
//       constraints: }
class MoveVfpRegisterOpWithTypeSelTesterCase2
    : public MoveVfpRegisterOpWithTypeSelTester {
 public:
  MoveVfpRegisterOpWithTypeSelTesterCase2(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpWithTypeSelTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveVfpRegisterOpWithTypeSelTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00100000) != 0x00000000 /* L(20)=~0 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000100 /* C(8)=~1 */) return false;
  if ((inst.Bits() & 0x00800000) != 0x00000000 /* A(23:21)=~0xx */) return false;
  if ((inst.Bits() & 0x0000000F) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpWithTypeSelTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(20)=0 & inst(8)=1 & inst(23:21)=1xx & inst(6:5)=0x & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {baseline: 'DuplicateToAdvSIMDRegisters',
//       constraints: }
//
// Representaive case:
// L(20)=0 & C(8)=1 & A(23:21)=1xx & B(6:5)=0x & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {baseline: DuplicateToAdvSIMDRegisters,
//       constraints: }
class DuplicateToAdvSIMDRegistersTesterCase3
    : public DuplicateToAdvSIMDRegistersTester {
 public:
  DuplicateToAdvSIMDRegistersTesterCase3(const NamedClassDecoder& decoder)
    : DuplicateToAdvSIMDRegistersTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool DuplicateToAdvSIMDRegistersTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00100000) != 0x00000000 /* L(20)=~0 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000100 /* C(8)=~1 */) return false;
  if ((inst.Bits() & 0x00800000) != 0x00800000 /* A(23:21)=~1xx */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000000 /* B(6:5)=~0x */) return false;
  if ((inst.Bits() & 0x0000000F) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */) return false;

  // Check other preconditions defined for the base decoder.
  return DuplicateToAdvSIMDRegistersTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(20)=1 & inst(8)=0 & inst(23:21)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {baseline: 'MoveVfpRegisterOp',
//       constraints: }
//
// Representaive case:
// L(20)=1 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {baseline: MoveVfpRegisterOp,
//       constraints: }
class MoveVfpRegisterOpTesterCase4
    : public MoveVfpRegisterOpTester {
 public:
  MoveVfpRegisterOpTesterCase4(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveVfpRegisterOpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00100000) != 0x00100000 /* L(20)=~1 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000000 /* C(8)=~0 */) return false;
  if ((inst.Bits() & 0x00E00000) != 0x00000000 /* A(23:21)=~000 */) return false;
  if ((inst.Bits() & 0x0000006F) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxx00x0000 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(20)=1 & inst(8)=0 & inst(23:21)=111 & inst(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {baseline: 'VfpMrsOp',
//       constraints: }
//
// Representaive case:
// L(20)=1 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {baseline: VfpMrsOp,
//       constraints: }
class VfpMrsOpTesterCase5
    : public VfpMrsOpTester {
 public:
  VfpMrsOpTesterCase5(const NamedClassDecoder& decoder)
    : VfpMrsOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VfpMrsOpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00100000) != 0x00100000 /* L(20)=~1 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000000 /* C(8)=~0 */) return false;
  if ((inst.Bits() & 0x00E00000) != 0x00E00000 /* A(23:21)=~111 */) return false;
  if ((inst.Bits() & 0x000F00EF) != 0x00010000 /* $pattern(31:0)=~xxxxxxxxxxxx0001xxxxxxxx000x0000 */) return false;

  // Check other preconditions defined for the base decoder.
  return VfpMrsOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(20)=1 & inst(8)=1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {baseline: 'MoveVfpRegisterOpWithTypeSel',
//       constraints: }
//
// Representaive case:
// L(20)=1 & C(8)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {baseline: MoveVfpRegisterOpWithTypeSel,
//       constraints: }
class MoveVfpRegisterOpWithTypeSelTesterCase6
    : public MoveVfpRegisterOpWithTypeSelTester {
 public:
  MoveVfpRegisterOpWithTypeSelTesterCase6(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpWithTypeSelTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveVfpRegisterOpWithTypeSelTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00100000) != 0x00100000 /* L(20)=~1 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000100 /* C(8)=~1 */) return false;
  if ((inst.Bits() & 0x0000000F) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpWithTypeSelTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(20)=0 & inst(8)=0 & inst(23:21)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {baseline: 'MoveVfpRegisterOp',
//       constraints: ,
//       rule: 'Vmov_Rule_330_A1_P648'}
//
// Representative case:
// L(20)=0 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {baseline: MoveVfpRegisterOp,
//       constraints: ,
//       rule: Vmov_Rule_330_A1_P648}
class MoveVfpRegisterOpTester_Case0
    : public MoveVfpRegisterOpTesterCase0 {
 public:
  MoveVfpRegisterOpTester_Case0()
    : MoveVfpRegisterOpTesterCase0(
      state_.MoveVfpRegisterOp_Vmov_Rule_330_A1_P648_instance_)
  {}
};

// Neutral case:
// inst(20)=0 & inst(8)=0 & inst(23:21)=111 & inst(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {baseline: 'VfpUsesRegOp',
//       constraints: ,
//       rule: 'Vmsr_Rule_336_A1_P660'}
//
// Representative case:
// L(20)=0 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {baseline: VfpUsesRegOp,
//       constraints: ,
//       rule: Vmsr_Rule_336_A1_P660}
class VfpUsesRegOpTester_Case1
    : public VfpUsesRegOpTesterCase1 {
 public:
  VfpUsesRegOpTester_Case1()
    : VfpUsesRegOpTesterCase1(
      state_.VfpUsesRegOp_Vmsr_Rule_336_A1_P660_instance_)
  {}
};

// Neutral case:
// inst(20)=0 & inst(8)=1 & inst(23:21)=0xx & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {baseline: 'MoveVfpRegisterOpWithTypeSel',
//       constraints: ,
//       rule: 'Vmov_Rule_328_A1_P644'}
//
// Representative case:
// L(20)=0 & C(8)=1 & A(23:21)=0xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {baseline: MoveVfpRegisterOpWithTypeSel,
//       constraints: ,
//       rule: Vmov_Rule_328_A1_P644}
class MoveVfpRegisterOpWithTypeSelTester_Case2
    : public MoveVfpRegisterOpWithTypeSelTesterCase2 {
 public:
  MoveVfpRegisterOpWithTypeSelTester_Case2()
    : MoveVfpRegisterOpWithTypeSelTesterCase2(
      state_.MoveVfpRegisterOpWithTypeSel_Vmov_Rule_328_A1_P644_instance_)
  {}
};

// Neutral case:
// inst(20)=0 & inst(8)=1 & inst(23:21)=1xx & inst(6:5)=0x & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {baseline: 'DuplicateToAdvSIMDRegisters',
//       constraints: ,
//       rule: 'Vdup_Rule_303_A1_P594'}
//
// Representative case:
// L(20)=0 & C(8)=1 & A(23:21)=1xx & B(6:5)=0x & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {baseline: DuplicateToAdvSIMDRegisters,
//       constraints: ,
//       rule: Vdup_Rule_303_A1_P594}
class DuplicateToAdvSIMDRegistersTester_Case3
    : public DuplicateToAdvSIMDRegistersTesterCase3 {
 public:
  DuplicateToAdvSIMDRegistersTester_Case3()
    : DuplicateToAdvSIMDRegistersTesterCase3(
      state_.DuplicateToAdvSIMDRegisters_Vdup_Rule_303_A1_P594_instance_)
  {}
};

// Neutral case:
// inst(20)=1 & inst(8)=0 & inst(23:21)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {baseline: 'MoveVfpRegisterOp',
//       constraints: ,
//       rule: 'Vmov_Rule_330_A1_P648'}
//
// Representative case:
// L(20)=1 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {baseline: MoveVfpRegisterOp,
//       constraints: ,
//       rule: Vmov_Rule_330_A1_P648}
class MoveVfpRegisterOpTester_Case4
    : public MoveVfpRegisterOpTesterCase4 {
 public:
  MoveVfpRegisterOpTester_Case4()
    : MoveVfpRegisterOpTesterCase4(
      state_.MoveVfpRegisterOp_Vmov_Rule_330_A1_P648_instance_)
  {}
};

// Neutral case:
// inst(20)=1 & inst(8)=0 & inst(23:21)=111 & inst(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {baseline: 'VfpMrsOp',
//       constraints: ,
//       rule: 'Vmrs_Rule_335_A1_P658'}
//
// Representative case:
// L(20)=1 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {baseline: VfpMrsOp,
//       constraints: ,
//       rule: Vmrs_Rule_335_A1_P658}
class VfpMrsOpTester_Case5
    : public VfpMrsOpTesterCase5 {
 public:
  VfpMrsOpTester_Case5()
    : VfpMrsOpTesterCase5(
      state_.VfpMrsOp_Vmrs_Rule_335_A1_P658_instance_)
  {}
};

// Neutral case:
// inst(20)=1 & inst(8)=1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {baseline: 'MoveVfpRegisterOpWithTypeSel',
//       constraints: ,
//       rule: 'Vmov_Rule_329_A1_P646'}
//
// Representative case:
// L(20)=1 & C(8)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {baseline: MoveVfpRegisterOpWithTypeSel,
//       constraints: ,
//       rule: Vmov_Rule_329_A1_P646}
class MoveVfpRegisterOpWithTypeSelTester_Case6
    : public MoveVfpRegisterOpWithTypeSelTesterCase6 {
 public:
  MoveVfpRegisterOpWithTypeSelTester_Case6()
    : MoveVfpRegisterOpWithTypeSelTesterCase6(
      state_.MoveVfpRegisterOpWithTypeSel_Vmov_Rule_329_A1_P646_instance_)
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
// inst(20)=0 & inst(8)=0 & inst(23:21)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {actual: 'MoveVfpRegisterOp',
//       baseline: 'MoveVfpRegisterOp',
//       constraints: ,
//       pattern: 'cccc11100000nnnntttt1010n0010000',
//       rule: 'Vmov_Rule_330_A1_P648'}
//
// Representaive case:
// L(20)=0 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {actual: MoveVfpRegisterOp,
//       baseline: MoveVfpRegisterOp,
//       constraints: ,
//       pattern: cccc11100000nnnntttt1010n0010000,
//       rule: Vmov_Rule_330_A1_P648}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpTester_Case0_TestCase0) {
  MoveVfpRegisterOpTester_Case0 tester;
  tester.Test("cccc11100000nnnntttt1010n0010000");
}

// Neutral case:
// inst(20)=0 & inst(8)=0 & inst(23:21)=111 & inst(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {actual: 'DontCareInstRdNotPc',
//       baseline: 'VfpUsesRegOp',
//       constraints: ,
//       pattern: 'cccc111011100001tttt101000010000',
//       rule: 'Vmsr_Rule_336_A1_P660'}
//
// Representative case:
// L(20)=0 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {actual: DontCareInstRdNotPc,
//       baseline: VfpUsesRegOp,
//       constraints: ,
//       pattern: cccc111011100001tttt101000010000,
//       rule: Vmsr_Rule_336_A1_P660}
TEST_F(Arm32DecoderStateTests,
       VfpUsesRegOpTester_Case1_TestCase1) {
  VfpUsesRegOpTester_Case1 baseline_tester;
  NamedDontCareInstRdNotPc_Vmsr_Rule_336_A1_P660 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc111011100001tttt101000010000");
}

// Neutral case:
// inst(20)=0 & inst(8)=1 & inst(23:21)=0xx & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {actual: 'MoveVfpRegisterOpWithTypeSel',
//       baseline: 'MoveVfpRegisterOpWithTypeSel',
//       constraints: ,
//       pattern: 'cccc11100ii0ddddtttt1011dii10000',
//       rule: 'Vmov_Rule_328_A1_P644'}
//
// Representaive case:
// L(20)=0 & C(8)=1 & A(23:21)=0xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {actual: MoveVfpRegisterOpWithTypeSel,
//       baseline: MoveVfpRegisterOpWithTypeSel,
//       constraints: ,
//       pattern: cccc11100ii0ddddtttt1011dii10000,
//       rule: Vmov_Rule_328_A1_P644}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpWithTypeSelTester_Case2_TestCase2) {
  MoveVfpRegisterOpWithTypeSelTester_Case2 tester;
  tester.Test("cccc11100ii0ddddtttt1011dii10000");
}

// Neutral case:
// inst(20)=0 & inst(8)=1 & inst(23:21)=1xx & inst(6:5)=0x & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {actual: 'DuplicateToAdvSIMDRegisters',
//       baseline: 'DuplicateToAdvSIMDRegisters',
//       constraints: ,
//       pattern: 'cccc11101bq0ddddtttt1011d0e10000',
//       rule: 'Vdup_Rule_303_A1_P594'}
//
// Representaive case:
// L(20)=0 & C(8)=1 & A(23:21)=1xx & B(6:5)=0x & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {actual: DuplicateToAdvSIMDRegisters,
//       baseline: DuplicateToAdvSIMDRegisters,
//       constraints: ,
//       pattern: cccc11101bq0ddddtttt1011d0e10000,
//       rule: Vdup_Rule_303_A1_P594}
TEST_F(Arm32DecoderStateTests,
       DuplicateToAdvSIMDRegistersTester_Case3_TestCase3) {
  DuplicateToAdvSIMDRegistersTester_Case3 tester;
  tester.Test("cccc11101bq0ddddtttt1011d0e10000");
}

// Neutral case:
// inst(20)=1 & inst(8)=0 & inst(23:21)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {actual: 'MoveVfpRegisterOp',
//       baseline: 'MoveVfpRegisterOp',
//       constraints: ,
//       pattern: 'cccc11100001nnnntttt1010n0010000',
//       rule: 'Vmov_Rule_330_A1_P648'}
//
// Representaive case:
// L(20)=1 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = {actual: MoveVfpRegisterOp,
//       baseline: MoveVfpRegisterOp,
//       constraints: ,
//       pattern: cccc11100001nnnntttt1010n0010000,
//       rule: Vmov_Rule_330_A1_P648}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpTester_Case4_TestCase4) {
  MoveVfpRegisterOpTester_Case4 tester;
  tester.Test("cccc11100001nnnntttt1010n0010000");
}

// Neutral case:
// inst(20)=1 & inst(8)=0 & inst(23:21)=111 & inst(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {actual: 'VfpMrsOp',
//       baseline: 'VfpMrsOp',
//       constraints: ,
//       pattern: 'cccc111011110001tttt101000010000',
//       rule: 'Vmrs_Rule_335_A1_P658'}
//
// Representaive case:
// L(20)=1 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = {actual: VfpMrsOp,
//       baseline: VfpMrsOp,
//       constraints: ,
//       pattern: cccc111011110001tttt101000010000,
//       rule: Vmrs_Rule_335_A1_P658}
TEST_F(Arm32DecoderStateTests,
       VfpMrsOpTester_Case5_TestCase5) {
  VfpMrsOpTester_Case5 tester;
  tester.Test("cccc111011110001tttt101000010000");
}

// Neutral case:
// inst(20)=1 & inst(8)=1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {actual: 'MoveVfpRegisterOpWithTypeSel',
//       baseline: 'MoveVfpRegisterOpWithTypeSel',
//       constraints: ,
//       pattern: 'cccc1110iii1nnnntttt1011nii10000',
//       rule: 'Vmov_Rule_329_A1_P646'}
//
// Representaive case:
// L(20)=1 & C(8)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = {actual: MoveVfpRegisterOpWithTypeSel,
//       baseline: MoveVfpRegisterOpWithTypeSel,
//       constraints: ,
//       pattern: cccc1110iii1nnnntttt1011nii10000,
//       rule: Vmov_Rule_329_A1_P646}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpWithTypeSelTester_Case6_TestCase6) {
  MoveVfpRegisterOpWithTypeSelTester_Case6 tester;
  tester.Test("cccc1110iii1nnnntttt1011nii10000");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
