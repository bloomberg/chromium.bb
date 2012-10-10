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
// inst(23:20)=1000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'StoreExclusive3RegisterOp',
//       constraints: }
//
// Representaive case:
// op(23:20)=1000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: StoreExclusive3RegisterOp,
//       constraints: }
class StoreExclusive3RegisterOpTesterCase0
    : public StoreExclusive3RegisterOpTester {
 public:
  StoreExclusive3RegisterOpTesterCase0(const NamedClassDecoder& decoder)
    : StoreExclusive3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreExclusive3RegisterOpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00800000 /* op(23:20)=~1000 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreExclusive3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=1001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: 'LoadExclusive2RegisterOp',
//       constraints: }
//
// Representaive case:
// op(23:20)=1001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: LoadExclusive2RegisterOp,
//       constraints: }
class LoadExclusive2RegisterOpTesterCase1
    : public LoadExclusive2RegisterOpTester {
 public:
  LoadExclusive2RegisterOpTesterCase1(const NamedClassDecoder& decoder)
    : LoadExclusive2RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadExclusive2RegisterOpTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00900000 /* op(23:20)=~1001 */) return false;
  if ((inst.Bits() & 0x00000F0F) != 0x00000F0F /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadExclusive2RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=1010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'StoreExclusive3RegisterDoubleOp',
//       constraints: }
//
// Representaive case:
// op(23:20)=1010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: StoreExclusive3RegisterDoubleOp,
//       constraints: }
class StoreExclusive3RegisterDoubleOpTesterCase2
    : public StoreExclusive3RegisterDoubleOpTester {
 public:
  StoreExclusive3RegisterDoubleOpTesterCase2(const NamedClassDecoder& decoder)
    : StoreExclusive3RegisterDoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreExclusive3RegisterDoubleOpTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00A00000 /* op(23:20)=~1010 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreExclusive3RegisterDoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=1011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: 'LoadExclusive2RegisterDoubleOp',
//       constraints: }
//
// Representaive case:
// op(23:20)=1011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: LoadExclusive2RegisterDoubleOp,
//       constraints: }
class LoadExclusive2RegisterDoubleOpTesterCase3
    : public LoadExclusive2RegisterDoubleOpTester {
 public:
  LoadExclusive2RegisterDoubleOpTesterCase3(const NamedClassDecoder& decoder)
    : LoadExclusive2RegisterDoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadExclusive2RegisterDoubleOpTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00B00000 /* op(23:20)=~1011 */) return false;
  if ((inst.Bits() & 0x00000F0F) != 0x00000F0F /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadExclusive2RegisterDoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=1100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'StoreExclusive3RegisterOp',
//       constraints: }
//
// Representaive case:
// op(23:20)=1100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: StoreExclusive3RegisterOp,
//       constraints: }
class StoreExclusive3RegisterOpTesterCase4
    : public StoreExclusive3RegisterOpTester {
 public:
  StoreExclusive3RegisterOpTesterCase4(const NamedClassDecoder& decoder)
    : StoreExclusive3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreExclusive3RegisterOpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00C00000 /* op(23:20)=~1100 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreExclusive3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=1101 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: 'LoadExclusive2RegisterOp',
//       constraints: }
//
// Representaive case:
// op(23:20)=1101 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: LoadExclusive2RegisterOp,
//       constraints: }
class LoadExclusive2RegisterOpTesterCase5
    : public LoadExclusive2RegisterOpTester {
 public:
  LoadExclusive2RegisterOpTesterCase5(const NamedClassDecoder& decoder)
    : LoadExclusive2RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadExclusive2RegisterOpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00D00000 /* op(23:20)=~1101 */) return false;
  if ((inst.Bits() & 0x00000F0F) != 0x00000F0F /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadExclusive2RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=1110 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'StoreExclusive3RegisterOp',
//       constraints: }
//
// Representaive case:
// op(23:20)=1110 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: StoreExclusive3RegisterOp,
//       constraints: }
class StoreExclusive3RegisterOpTesterCase6
    : public StoreExclusive3RegisterOpTester {
 public:
  StoreExclusive3RegisterOpTesterCase6(const NamedClassDecoder& decoder)
    : StoreExclusive3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreExclusive3RegisterOpTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00E00000 /* op(23:20)=~1110 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreExclusive3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: 'LoadExclusive2RegisterOp',
//       constraints: }
//
// Representaive case:
// op(23:20)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: LoadExclusive2RegisterOp,
//       constraints: }
class LoadExclusive2RegisterOpTesterCase7
    : public LoadExclusive2RegisterOpTester {
 public:
  LoadExclusive2RegisterOpTesterCase7(const NamedClassDecoder& decoder)
    : LoadExclusive2RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadExclusive2RegisterOpTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00F00000 /* op(23:20)=~1111 */) return false;
  if ((inst.Bits() & 0x00000F0F) != 0x00000F0F /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxx1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadExclusive2RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=0x00 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Deprecated',
//       constraints: }
//
// Representaive case:
// op(23:20)=0x00 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Deprecated,
//       constraints: }
class UnsafeCondDecoderTesterCase8
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase8(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00000000 /* op(23:20)=~0x00 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(23:20)=1000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'StoreExclusive3RegisterOp',
//       constraints: ,
//       rule: 'Strex_Rule_202_A1_P400'}
//
// Representative case:
// op(23:20)=1000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: StoreExclusive3RegisterOp,
//       constraints: ,
//       rule: Strex_Rule_202_A1_P400}
class StoreExclusive3RegisterOpTester_Case0
    : public StoreExclusive3RegisterOpTesterCase0 {
 public:
  StoreExclusive3RegisterOpTester_Case0()
    : StoreExclusive3RegisterOpTesterCase0(
      state_.StoreExclusive3RegisterOp_Strex_Rule_202_A1_P400_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: 'LoadExclusive2RegisterOp',
//       constraints: ,
//       rule: 'Ldrex_Rule_69_A1_P142'}
//
// Representative case:
// op(23:20)=1001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: LoadExclusive2RegisterOp,
//       constraints: ,
//       rule: Ldrex_Rule_69_A1_P142}
class LoadExclusive2RegisterOpTester_Case1
    : public LoadExclusive2RegisterOpTesterCase1 {
 public:
  LoadExclusive2RegisterOpTester_Case1()
    : LoadExclusive2RegisterOpTesterCase1(
      state_.LoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'StoreExclusive3RegisterDoubleOp',
//       constraints: ,
//       rule: 'Strexd_Rule_204_A1_P404'}
//
// Representative case:
// op(23:20)=1010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: StoreExclusive3RegisterDoubleOp,
//       constraints: ,
//       rule: Strexd_Rule_204_A1_P404}
class StoreExclusive3RegisterDoubleOpTester_Case2
    : public StoreExclusive3RegisterDoubleOpTesterCase2 {
 public:
  StoreExclusive3RegisterDoubleOpTester_Case2()
    : StoreExclusive3RegisterDoubleOpTesterCase2(
      state_.StoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: 'LoadExclusive2RegisterDoubleOp',
//       constraints: ,
//       rule: 'Ldrexd_Rule_71_A1_P146'}
//
// Representative case:
// op(23:20)=1011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: LoadExclusive2RegisterDoubleOp,
//       constraints: ,
//       rule: Ldrexd_Rule_71_A1_P146}
class LoadExclusive2RegisterDoubleOpTester_Case3
    : public LoadExclusive2RegisterDoubleOpTesterCase3 {
 public:
  LoadExclusive2RegisterDoubleOpTester_Case3()
    : LoadExclusive2RegisterDoubleOpTesterCase3(
      state_.LoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'StoreExclusive3RegisterOp',
//       constraints: ,
//       rule: 'Strexb_Rule_203_A1_P402'}
//
// Representative case:
// op(23:20)=1100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: StoreExclusive3RegisterOp,
//       constraints: ,
//       rule: Strexb_Rule_203_A1_P402}
class StoreExclusive3RegisterOpTester_Case4
    : public StoreExclusive3RegisterOpTesterCase4 {
 public:
  StoreExclusive3RegisterOpTester_Case4()
    : StoreExclusive3RegisterOpTesterCase4(
      state_.StoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1101 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: 'LoadExclusive2RegisterOp',
//       constraints: ,
//       rule: 'Ldrexb_Rule_70_A1_P144'}
//
// Representative case:
// op(23:20)=1101 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: LoadExclusive2RegisterOp,
//       constraints: ,
//       rule: Ldrexb_Rule_70_A1_P144}
class LoadExclusive2RegisterOpTester_Case5
    : public LoadExclusive2RegisterOpTesterCase5 {
 public:
  LoadExclusive2RegisterOpTester_Case5()
    : LoadExclusive2RegisterOpTesterCase5(
      state_.LoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1110 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'StoreExclusive3RegisterOp',
//       constraints: ,
//       rule: 'Strexh_Rule_205_A1_P406'}
//
// Representative case:
// op(23:20)=1110 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: StoreExclusive3RegisterOp,
//       constraints: ,
//       rule: Strexh_Rule_205_A1_P406}
class StoreExclusive3RegisterOpTester_Case6
    : public StoreExclusive3RegisterOpTesterCase6 {
 public:
  StoreExclusive3RegisterOpTester_Case6()
    : StoreExclusive3RegisterOpTesterCase6(
      state_.StoreExclusive3RegisterOp_Strexh_Rule_205_A1_P406_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: 'LoadExclusive2RegisterOp',
//       constraints: ,
//       rule: 'Ldrexh_Rule_72_A1_P148'}
//
// Representative case:
// op(23:20)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {baseline: LoadExclusive2RegisterOp,
//       constraints: ,
//       rule: Ldrexh_Rule_72_A1_P148}
class LoadExclusive2RegisterOpTester_Case7
    : public LoadExclusive2RegisterOpTesterCase7 {
 public:
  LoadExclusive2RegisterOpTester_Case7()
    : LoadExclusive2RegisterOpTesterCase7(
      state_.LoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148_instance_)
  {}
};

// Neutral case:
// inst(23:20)=0x00 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Deprecated',
//       constraints: ,
//       rule: 'Swp_Swpb_Rule_A1'}
//
// Representative case:
// op(23:20)=0x00 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Deprecated,
//       constraints: ,
//       rule: Swp_Swpb_Rule_A1}
class DeprecatedTester_Case8
    : public UnsafeCondDecoderTesterCase8 {
 public:
  DeprecatedTester_Case8()
    : UnsafeCondDecoderTesterCase8(
      state_.Deprecated_Swp_Swpb_Rule_A1_instance_)
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
// inst(23:20)=1000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'StoreBasedMemoryRtBits0To3',
//       baseline: 'StoreExclusive3RegisterOp',
//       constraints: ,
//       pattern: 'cccc00011000nnnndddd11111001tttt',
//       rule: 'Strex_Rule_202_A1_P400'}
//
// Representative case:
// op(23:20)=1000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: StoreBasedMemoryRtBits0To3,
//       baseline: StoreExclusive3RegisterOp,
//       constraints: ,
//       pattern: cccc00011000nnnndddd11111001tttt,
//       rule: Strex_Rule_202_A1_P400}
TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterOpTester_Case0_TestCase0) {
  StoreExclusive3RegisterOpTester_Case0 baseline_tester;
  NamedStoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011000nnnndddd11111001tttt");
}

// Neutral case:
// inst(23:20)=1001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {actual: 'LoadBasedMemory',
//       baseline: 'LoadExclusive2RegisterOp',
//       constraints: ,
//       pattern: 'cccc00011001nnnntttt111110011111',
//       rule: 'Ldrex_Rule_69_A1_P142'}
//
// Representative case:
// op(23:20)=1001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {actual: LoadBasedMemory,
//       baseline: LoadExclusive2RegisterOp,
//       constraints: ,
//       pattern: cccc00011001nnnntttt111110011111,
//       rule: Ldrex_Rule_69_A1_P142}
TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterOpTester_Case1_TestCase1) {
  LoadExclusive2RegisterOpTester_Case1 baseline_tester;
  NamedLoadBasedMemory_Ldrex_Rule_69_A1_P142 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011001nnnntttt111110011111");
}

// Neutral case:
// inst(23:20)=1010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'StoreBasedMemoryDoubleRtBits0To3',
//       baseline: 'StoreExclusive3RegisterDoubleOp',
//       constraints: ,
//       pattern: 'cccc00011010nnnndddd11111001tttt',
//       rule: 'Strexd_Rule_204_A1_P404'}
//
// Representative case:
// op(23:20)=1010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: StoreBasedMemoryDoubleRtBits0To3,
//       baseline: StoreExclusive3RegisterDoubleOp,
//       constraints: ,
//       pattern: cccc00011010nnnndddd11111001tttt,
//       rule: Strexd_Rule_204_A1_P404}
TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterDoubleOpTester_Case2_TestCase2) {
  StoreExclusive3RegisterDoubleOpTester_Case2 baseline_tester;
  NamedStoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011010nnnndddd11111001tttt");
}

// Neutral case:
// inst(23:20)=1011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {actual: 'LoadBasedMemoryDouble',
//       baseline: 'LoadExclusive2RegisterDoubleOp',
//       constraints: ,
//       pattern: 'cccc00011011nnnntttt111110011111',
//       rule: 'Ldrexd_Rule_71_A1_P146'}
//
// Representative case:
// op(23:20)=1011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {actual: LoadBasedMemoryDouble,
//       baseline: LoadExclusive2RegisterDoubleOp,
//       constraints: ,
//       pattern: cccc00011011nnnntttt111110011111,
//       rule: Ldrexd_Rule_71_A1_P146}
TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterDoubleOpTester_Case3_TestCase3) {
  LoadExclusive2RegisterDoubleOpTester_Case3 baseline_tester;
  NamedLoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011011nnnntttt111110011111");
}

// Neutral case:
// inst(23:20)=1100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'StoreBasedMemoryRtBits0To3',
//       baseline: 'StoreExclusive3RegisterOp',
//       constraints: ,
//       pattern: 'cccc00011100nnnndddd11111001tttt',
//       rule: 'Strexb_Rule_203_A1_P402'}
//
// Representative case:
// op(23:20)=1100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: StoreBasedMemoryRtBits0To3,
//       baseline: StoreExclusive3RegisterOp,
//       constraints: ,
//       pattern: cccc00011100nnnndddd11111001tttt,
//       rule: Strexb_Rule_203_A1_P402}
TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterOpTester_Case4_TestCase4) {
  StoreExclusive3RegisterOpTester_Case4 baseline_tester;
  NamedStoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011100nnnndddd11111001tttt");
}

// Neutral case:
// inst(23:20)=1101 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {actual: 'LoadBasedMemory',
//       baseline: 'LoadExclusive2RegisterOp',
//       constraints: ,
//       pattern: 'cccc00011101nnnntttt111110011111',
//       rule: 'Ldrexb_Rule_70_A1_P144'}
//
// Representative case:
// op(23:20)=1101 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {actual: LoadBasedMemory,
//       baseline: LoadExclusive2RegisterOp,
//       constraints: ,
//       pattern: cccc00011101nnnntttt111110011111,
//       rule: Ldrexb_Rule_70_A1_P144}
TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterOpTester_Case5_TestCase5) {
  LoadExclusive2RegisterOpTester_Case5 baseline_tester;
  NamedLoadBasedMemory_Ldrexb_Rule_70_A1_P144 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011101nnnntttt111110011111");
}

// Neutral case:
// inst(23:20)=1110 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'StoreBasedMemoryRtBits0To3',
//       baseline: 'StoreExclusive3RegisterOp',
//       constraints: ,
//       pattern: 'cccc00011110nnnndddd11111001tttt',
//       rule: 'Strexh_Rule_205_A1_P406'}
//
// Representative case:
// op(23:20)=1110 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: StoreBasedMemoryRtBits0To3,
//       baseline: StoreExclusive3RegisterOp,
//       constraints: ,
//       pattern: cccc00011110nnnndddd11111001tttt,
//       rule: Strexh_Rule_205_A1_P406}
TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterOpTester_Case6_TestCase6) {
  StoreExclusive3RegisterOpTester_Case6 baseline_tester;
  NamedStoreBasedMemoryRtBits0To3_Strexh_Rule_205_A1_P406 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011110nnnndddd11111001tttt");
}

// Neutral case:
// inst(23:20)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {actual: 'LoadBasedMemory',
//       baseline: 'LoadExclusive2RegisterOp',
//       constraints: ,
//       pattern: 'cccc00011111nnnntttt111110011111',
//       rule: 'Ldrexh_Rule_72_A1_P148'}
//
// Representative case:
// op(23:20)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = {actual: LoadBasedMemory,
//       baseline: LoadExclusive2RegisterOp,
//       constraints: ,
//       pattern: cccc00011111nnnntttt111110011111,
//       rule: Ldrexh_Rule_72_A1_P148}
TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterOpTester_Case7_TestCase7) {
  LoadExclusive2RegisterOpTester_Case7 baseline_tester;
  NamedLoadBasedMemory_Ldrexh_Rule_72_A1_P148 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011111nnnntttt111110011111");
}

// Neutral case:
// inst(23:20)=0x00 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'Deprecated',
//       baseline: 'Deprecated',
//       constraints: ,
//       pattern: 'cccc00010b00nnnntttt00001001tttt',
//       rule: 'Swp_Swpb_Rule_A1'}
//
// Representaive case:
// op(23:20)=0x00 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: Deprecated,
//       baseline: Deprecated,
//       constraints: ,
//       pattern: cccc00010b00nnnntttt00001001tttt,
//       rule: Swp_Swpb_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       DeprecatedTester_Case8_TestCase8) {
  DeprecatedTester_Case8 tester;
  tester.Test("cccc00010b00nnnntttt00001001tttt");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
