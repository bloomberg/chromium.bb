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
// inst(25:20)=001001
//    = LoadRegisterList {'constraints': }
//
// Representaive case:
// op(25:20)=001001
//    = LoadRegisterList {constraints: }
class LoadStoreRegisterListTesterCase0
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase0(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03F00000) != 0x00900000 /* op(25:20)=~001001 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=001011 & inst(19:16)=~1101
//    = LoadRegisterList {'constraints': }
//
// Representaive case:
// op(25:20)=001011 & Rn(19:16)=~1101
//    = LoadRegisterList {constraints: }
class LoadStoreRegisterListTesterCase1
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase1(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03F00000) != 0x00B00000 /* op(25:20)=~001011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=001011 & inst(19:16)=1101
//    = LoadRegisterList {'constraints': }
//
// Representaive case:
// op(25:20)=001011 & Rn(19:16)=1101
//    = LoadRegisterList {constraints: }
class LoadStoreRegisterListTesterCase2
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase2(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03F00000) != 0x00B00000 /* op(25:20)=~001011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=010000
//    = StoreRegisterList {'constraints': }
//
// Representaive case:
// op(25:20)=010000
//    = StoreRegisterList {constraints: }
class LoadStoreRegisterListTesterCase3
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase3(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03F00000) != 0x01000000 /* op(25:20)=~010000 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=010010 & inst(19:16)=~1101
//    = StoreRegisterList {'constraints': }
//
// Representaive case:
// op(25:20)=010010 & Rn(19:16)=~1101
//    = StoreRegisterList {constraints: }
class LoadStoreRegisterListTesterCase4
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase4(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03F00000) != 0x01200000 /* op(25:20)=~010010 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=010010 & inst(19:16)=1101
//    = StoreRegisterList {'constraints': }
//
// Representaive case:
// op(25:20)=010010 & Rn(19:16)=1101
//    = StoreRegisterList {constraints: }
class LoadStoreRegisterListTesterCase5
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase5(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03F00000) != 0x01200000 /* op(25:20)=~010010 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=0000x0
//    = StoreRegisterList {'constraints': }
//
// Representaive case:
// op(25:20)=0000x0
//    = StoreRegisterList {constraints: }
class LoadStoreRegisterListTesterCase6
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase6(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03D00000) != 0x00000000 /* op(25:20)=~0000x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=0000x1
//    = LoadRegisterList {'constraints': }
//
// Representaive case:
// op(25:20)=0000x1
//    = LoadRegisterList {constraints: }
class LoadStoreRegisterListTesterCase7
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase7(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03D00000) != 0x00100000 /* op(25:20)=~0000x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=0010x0
//    = StoreRegisterList {'constraints': }
//
// Representaive case:
// op(25:20)=0010x0
//    = StoreRegisterList {constraints: }
class LoadStoreRegisterListTesterCase8
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase8(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03D00000) != 0x00800000 /* op(25:20)=~0010x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=0100x1
//    = LoadRegisterList {'constraints': }
//
// Representaive case:
// op(25:20)=0100x1
//    = LoadRegisterList {constraints: }
class LoadStoreRegisterListTesterCase9
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase9(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03D00000) != 0x01100000 /* op(25:20)=~0100x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=0110x0
//    = StoreRegisterList {'constraints': }
//
// Representaive case:
// op(25:20)=0110x0
//    = StoreRegisterList {constraints: }
class LoadStoreRegisterListTesterCase10
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase10(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03D00000) != 0x01800000 /* op(25:20)=~0110x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=0110x1
//    = LoadRegisterList {'constraints': }
//
// Representaive case:
// op(25:20)=0110x1
//    = LoadRegisterList {constraints: }
class LoadStoreRegisterListTesterCase11
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase11(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03D00000) != 0x01900000 /* op(25:20)=~0110x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=0xx1x0 & inst(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(25:20)=0xx1x0 & $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
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
  if ((inst.Bits() & 0x02500000) != 0x00400000 /* op(25:20)=~0xx1x0 */) return false;
  if ((inst.Bits() & 0x00200000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=0xx1x1 & inst(15)=0 & inst(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(25:20)=0xx1x1 & R(15)=0 & $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
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
  if ((inst.Bits() & 0x02500000) != 0x00500000 /* op(25:20)=~0xx1x1 */) return false;
  if ((inst.Bits() & 0x00008000) != 0x00000000 /* R(15)=~0 */) return false;
  if ((inst.Bits() & 0x00200000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=0xx1x1 & inst(15)=1
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(25:20)=0xx1x1 & R(15)=1
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase14
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase14(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02500000) != 0x00500000 /* op(25:20)=~0xx1x1 */) return false;
  if ((inst.Bits() & 0x00008000) != 0x00008000 /* R(15)=~1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=10xxxx
//    = BranchImmediate24 {'constraints': }
//
// Representaive case:
// op(25:20)=10xxxx
//    = BranchImmediate24 {constraints: }
class BranchImmediate24TesterCase15
    : public BranchImmediate24Tester {
 public:
  BranchImmediate24TesterCase15(const NamedClassDecoder& decoder)
    : BranchImmediate24Tester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BranchImmediate24TesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03000000) != 0x02000000 /* op(25:20)=~10xxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BranchImmediate24Tester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=11xxxx
//    = BranchImmediate24 {'constraints': }
//
// Representaive case:
// op(25:20)=11xxxx
//    = BranchImmediate24 {constraints: }
class BranchImmediate24TesterCase16
    : public BranchImmediate24Tester {
 public:
  BranchImmediate24TesterCase16(const NamedClassDecoder& decoder)
    : BranchImmediate24Tester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BranchImmediate24TesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03000000) != 0x03000000 /* op(25:20)=~11xxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BranchImmediate24Tester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(25:20)=001001
//    = LoadRegisterList {'constraints': ,
//     'rule': 'Ldm_Ldmia_Ldmfd_Rule_53_A1_P110'}
//
// Representative case:
// op(25:20)=001001
//    = LoadRegisterList {constraints: ,
//     rule: Ldm_Ldmia_Ldmfd_Rule_53_A1_P110}
class LoadRegisterListTester_Case0
    : public LoadStoreRegisterListTesterCase0 {
 public:
  LoadRegisterListTester_Case0()
    : LoadStoreRegisterListTesterCase0(
      state_.LoadRegisterList_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110_instance_)
  {}
};

// Neutral case:
// inst(25:20)=001011 & inst(19:16)=~1101
//    = LoadRegisterList {'constraints': ,
//     'rule': 'Ldm_Ldmia_Ldmfd_Rule_53_A1_P110'}
//
// Representative case:
// op(25:20)=001011 & Rn(19:16)=~1101
//    = LoadRegisterList {constraints: ,
//     rule: Ldm_Ldmia_Ldmfd_Rule_53_A1_P110}
class LoadRegisterListTester_Case1
    : public LoadStoreRegisterListTesterCase1 {
 public:
  LoadRegisterListTester_Case1()
    : LoadStoreRegisterListTesterCase1(
      state_.LoadRegisterList_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110_instance_)
  {}
};

// Neutral case:
// inst(25:20)=001011 & inst(19:16)=1101
//    = LoadRegisterList {'constraints': ,
//     'rule': 'Pop_Rule_A1'}
//
// Representative case:
// op(25:20)=001011 & Rn(19:16)=1101
//    = LoadRegisterList {constraints: ,
//     rule: Pop_Rule_A1}
class LoadRegisterListTester_Case2
    : public LoadStoreRegisterListTesterCase2 {
 public:
  LoadRegisterListTester_Case2()
    : LoadStoreRegisterListTesterCase2(
      state_.LoadRegisterList_Pop_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(25:20)=010000
//    = StoreRegisterList {'constraints': ,
//     'rule': 'Stmdb_Stmfd_Rule_191_A1_P378'}
//
// Representative case:
// op(25:20)=010000
//    = StoreRegisterList {constraints: ,
//     rule: Stmdb_Stmfd_Rule_191_A1_P378}
class StoreRegisterListTester_Case3
    : public LoadStoreRegisterListTesterCase3 {
 public:
  StoreRegisterListTester_Case3()
    : LoadStoreRegisterListTesterCase3(
      state_.StoreRegisterList_Stmdb_Stmfd_Rule_191_A1_P378_instance_)
  {}
};

// Neutral case:
// inst(25:20)=010010 & inst(19:16)=~1101
//    = StoreRegisterList {'constraints': ,
//     'rule': 'Stmdb_Stmfd_Rule_191_A1_P378'}
//
// Representative case:
// op(25:20)=010010 & Rn(19:16)=~1101
//    = StoreRegisterList {constraints: ,
//     rule: Stmdb_Stmfd_Rule_191_A1_P378}
class StoreRegisterListTester_Case4
    : public LoadStoreRegisterListTesterCase4 {
 public:
  StoreRegisterListTester_Case4()
    : LoadStoreRegisterListTesterCase4(
      state_.StoreRegisterList_Stmdb_Stmfd_Rule_191_A1_P378_instance_)
  {}
};

// Neutral case:
// inst(25:20)=010010 & inst(19:16)=1101
//    = StoreRegisterList {'constraints': ,
//     'rule': 'Push_Rule_A1'}
//
// Representative case:
// op(25:20)=010010 & Rn(19:16)=1101
//    = StoreRegisterList {constraints: ,
//     rule: Push_Rule_A1}
class StoreRegisterListTester_Case5
    : public LoadStoreRegisterListTesterCase5 {
 public:
  StoreRegisterListTester_Case5()
    : LoadStoreRegisterListTesterCase5(
      state_.StoreRegisterList_Push_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(25:20)=0000x0
//    = StoreRegisterList {'constraints': ,
//     'rule': 'Stmda_Stmed_Rule_190_A1_P376'}
//
// Representative case:
// op(25:20)=0000x0
//    = StoreRegisterList {constraints: ,
//     rule: Stmda_Stmed_Rule_190_A1_P376}
class StoreRegisterListTester_Case6
    : public LoadStoreRegisterListTesterCase6 {
 public:
  StoreRegisterListTester_Case6()
    : LoadStoreRegisterListTesterCase6(
      state_.StoreRegisterList_Stmda_Stmed_Rule_190_A1_P376_instance_)
  {}
};

// Neutral case:
// inst(25:20)=0000x1
//    = LoadRegisterList {'constraints': ,
//     'rule': 'Ldmda_Ldmfa_Rule_54_A1_P112'}
//
// Representative case:
// op(25:20)=0000x1
//    = LoadRegisterList {constraints: ,
//     rule: Ldmda_Ldmfa_Rule_54_A1_P112}
class LoadRegisterListTester_Case7
    : public LoadStoreRegisterListTesterCase7 {
 public:
  LoadRegisterListTester_Case7()
    : LoadStoreRegisterListTesterCase7(
      state_.LoadRegisterList_Ldmda_Ldmfa_Rule_54_A1_P112_instance_)
  {}
};

// Neutral case:
// inst(25:20)=0010x0
//    = StoreRegisterList {'constraints': ,
//     'rule': 'Stm_Stmia_Stmea_Rule_189_A1_P374'}
//
// Representative case:
// op(25:20)=0010x0
//    = StoreRegisterList {constraints: ,
//     rule: Stm_Stmia_Stmea_Rule_189_A1_P374}
class StoreRegisterListTester_Case8
    : public LoadStoreRegisterListTesterCase8 {
 public:
  StoreRegisterListTester_Case8()
    : LoadStoreRegisterListTesterCase8(
      state_.StoreRegisterList_Stm_Stmia_Stmea_Rule_189_A1_P374_instance_)
  {}
};

// Neutral case:
// inst(25:20)=0100x1
//    = LoadRegisterList {'constraints': ,
//     'rule': 'Ldmdb_Ldmea_Rule_55_A1_P114'}
//
// Representative case:
// op(25:20)=0100x1
//    = LoadRegisterList {constraints: ,
//     rule: Ldmdb_Ldmea_Rule_55_A1_P114}
class LoadRegisterListTester_Case9
    : public LoadStoreRegisterListTesterCase9 {
 public:
  LoadRegisterListTester_Case9()
    : LoadStoreRegisterListTesterCase9(
      state_.LoadRegisterList_Ldmdb_Ldmea_Rule_55_A1_P114_instance_)
  {}
};

// Neutral case:
// inst(25:20)=0110x0
//    = StoreRegisterList {'constraints': ,
//     'rule': 'Stmib_Stmfa_Rule_192_A1_P380'}
//
// Representative case:
// op(25:20)=0110x0
//    = StoreRegisterList {constraints: ,
//     rule: Stmib_Stmfa_Rule_192_A1_P380}
class StoreRegisterListTester_Case10
    : public LoadStoreRegisterListTesterCase10 {
 public:
  StoreRegisterListTester_Case10()
    : LoadStoreRegisterListTesterCase10(
      state_.StoreRegisterList_Stmib_Stmfa_Rule_192_A1_P380_instance_)
  {}
};

// Neutral case:
// inst(25:20)=0110x1
//    = LoadRegisterList {'constraints': ,
//     'rule': 'Ldmib_Ldmed_Rule_56_A1_P116'}
//
// Representative case:
// op(25:20)=0110x1
//    = LoadRegisterList {constraints: ,
//     rule: Ldmib_Ldmed_Rule_56_A1_P116}
class LoadRegisterListTester_Case11
    : public LoadStoreRegisterListTesterCase11 {
 public:
  LoadRegisterListTester_Case11()
    : LoadStoreRegisterListTesterCase11(
      state_.LoadRegisterList_Ldmib_Ldmed_Rule_56_A1_P116_instance_)
  {}
};

// Neutral case:
// inst(25:20)=0xx1x0 & inst(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Stm_Rule_11_B6_A1_P22'}
//
// Representative case:
// op(25:20)=0xx1x0 & $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Stm_Rule_11_B6_A1_P22}
class ForbiddenCondDecoderTester_Case12
    : public UnsafeCondDecoderTesterCase12 {
 public:
  ForbiddenCondDecoderTester_Case12()
    : UnsafeCondDecoderTesterCase12(
      state_.ForbiddenCondDecoder_Stm_Rule_11_B6_A1_P22_instance_)
  {}
};

// Neutral case:
// inst(25:20)=0xx1x1 & inst(15)=0 & inst(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Ldm_Rule_3_B6_A1_P7'}
//
// Representative case:
// op(25:20)=0xx1x1 & R(15)=0 & $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Ldm_Rule_3_B6_A1_P7}
class ForbiddenCondDecoderTester_Case13
    : public UnsafeCondDecoderTesterCase13 {
 public:
  ForbiddenCondDecoderTester_Case13()
    : UnsafeCondDecoderTesterCase13(
      state_.ForbiddenCondDecoder_Ldm_Rule_3_B6_A1_P7_instance_)
  {}
};

// Neutral case:
// inst(25:20)=0xx1x1 & inst(15)=1
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Ldm_Rule_2_B6_A1_P5'}
//
// Representative case:
// op(25:20)=0xx1x1 & R(15)=1
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Ldm_Rule_2_B6_A1_P5}
class ForbiddenCondDecoderTester_Case14
    : public UnsafeCondDecoderTesterCase14 {
 public:
  ForbiddenCondDecoderTester_Case14()
    : UnsafeCondDecoderTesterCase14(
      state_.ForbiddenCondDecoder_Ldm_Rule_2_B6_A1_P5_instance_)
  {}
};

// Neutral case:
// inst(25:20)=10xxxx
//    = BranchImmediate24 {'constraints': ,
//     'rule': 'B_Rule_16_A1_P44'}
//
// Representative case:
// op(25:20)=10xxxx
//    = BranchImmediate24 {constraints: ,
//     rule: B_Rule_16_A1_P44}
class BranchImmediate24Tester_Case15
    : public BranchImmediate24TesterCase15 {
 public:
  BranchImmediate24Tester_Case15()
    : BranchImmediate24TesterCase15(
      state_.BranchImmediate24_B_Rule_16_A1_P44_instance_)
  {}
};

// Neutral case:
// inst(25:20)=11xxxx
//    = BranchImmediate24 {'constraints': ,
//     'rule': 'Bl_Blx_Rule_23_A1_P58'}
//
// Representative case:
// op(25:20)=11xxxx
//    = BranchImmediate24 {constraints: ,
//     rule: Bl_Blx_Rule_23_A1_P58}
class BranchImmediate24Tester_Case16
    : public BranchImmediate24TesterCase16 {
 public:
  BranchImmediate24Tester_Case16()
    : BranchImmediate24TesterCase16(
      state_.BranchImmediate24_Bl_Blx_Rule_23_A1_P58_instance_)
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
// inst(25:20)=001001
//    = LoadRegisterList => LoadMultiple {'constraints': ,
//     'pattern': 'cccc10001001nnnnrrrrrrrrrrrrrrrr',
//     'rule': 'Ldm_Ldmia_Ldmfd_Rule_53_A1_P110'}
//
// Representative case:
// op(25:20)=001001
//    = LoadRegisterList => LoadMultiple {constraints: ,
//     pattern: cccc10001001nnnnrrrrrrrrrrrrrrrr,
//     rule: Ldm_Ldmia_Ldmfd_Rule_53_A1_P110}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_Case0_TestCase0) {
  LoadRegisterListTester_Case0 baseline_tester;
  NamedLoadMultiple_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc10001001nnnnrrrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(25:20)=001011 & inst(19:16)=~1101
//    = LoadRegisterList => LoadMultiple {'constraints': ,
//     'pattern': 'cccc10001011nnnnrrrrrrrrrrrrrrrr',
//     'rule': 'Ldm_Ldmia_Ldmfd_Rule_53_A1_P110'}
//
// Representative case:
// op(25:20)=001011 & Rn(19:16)=~1101
//    = LoadRegisterList => LoadMultiple {constraints: ,
//     pattern: cccc10001011nnnnrrrrrrrrrrrrrrrr,
//     rule: Ldm_Ldmia_Ldmfd_Rule_53_A1_P110}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_Case1_TestCase1) {
  LoadRegisterListTester_Case1 baseline_tester;
  NamedLoadMultiple_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc10001011nnnnrrrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(25:20)=001011 & inst(19:16)=1101
//    = LoadRegisterList => LoadMultiple {'constraints': ,
//     'pattern': 'cccc10001011nnnnrrrrrrrrrrrrrrrr',
//     'rule': 'Pop_Rule_A1'}
//
// Representative case:
// op(25:20)=001011 & Rn(19:16)=1101
//    = LoadRegisterList => LoadMultiple {constraints: ,
//     pattern: cccc10001011nnnnrrrrrrrrrrrrrrrr,
//     rule: Pop_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_Case2_TestCase2) {
  LoadRegisterListTester_Case2 baseline_tester;
  NamedLoadMultiple_Pop_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc10001011nnnnrrrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(25:20)=010000
//    = StoreRegisterList => StoreRegisterList {'constraints': ,
//     'pattern': 'cccc10010000nnnnrrrrrrrrrrrrrrrr',
//     'rule': 'Stmdb_Stmfd_Rule_191_A1_P378'}
//
// Representaive case:
// op(25:20)=010000
//    = StoreRegisterList => StoreRegisterList {constraints: ,
//     pattern: cccc10010000nnnnrrrrrrrrrrrrrrrr,
//     rule: Stmdb_Stmfd_Rule_191_A1_P378}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_Case3_TestCase3) {
  StoreRegisterListTester_Case3 tester;
  tester.Test("cccc10010000nnnnrrrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(25:20)=010010 & inst(19:16)=~1101
//    = StoreRegisterList => StoreRegisterList {'constraints': ,
//     'pattern': 'cccc10010010nnnnrrrrrrrrrrrrrrrr',
//     'rule': 'Stmdb_Stmfd_Rule_191_A1_P378'}
//
// Representaive case:
// op(25:20)=010010 & Rn(19:16)=~1101
//    = StoreRegisterList => StoreRegisterList {constraints: ,
//     pattern: cccc10010010nnnnrrrrrrrrrrrrrrrr,
//     rule: Stmdb_Stmfd_Rule_191_A1_P378}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_Case4_TestCase4) {
  StoreRegisterListTester_Case4 tester;
  tester.Test("cccc10010010nnnnrrrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(25:20)=010010 & inst(19:16)=1101
//    = StoreRegisterList => StoreRegisterList {'constraints': ,
//     'pattern': 'cccc10010010nnnnrrrrrrrrrrrrrrrr',
//     'rule': 'Push_Rule_A1'}
//
// Representaive case:
// op(25:20)=010010 & Rn(19:16)=1101
//    = StoreRegisterList => StoreRegisterList {constraints: ,
//     pattern: cccc10010010nnnnrrrrrrrrrrrrrrrr,
//     rule: Push_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_Case5_TestCase5) {
  StoreRegisterListTester_Case5 tester;
  tester.Test("cccc10010010nnnnrrrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(25:20)=0000x0
//    = StoreRegisterList => StoreRegisterList {'constraints': ,
//     'pattern': 'cccc100000w0nnnnrrrrrrrrrrrrrrrr',
//     'rule': 'Stmda_Stmed_Rule_190_A1_P376'}
//
// Representaive case:
// op(25:20)=0000x0
//    = StoreRegisterList => StoreRegisterList {constraints: ,
//     pattern: cccc100000w0nnnnrrrrrrrrrrrrrrrr,
//     rule: Stmda_Stmed_Rule_190_A1_P376}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_Case6_TestCase6) {
  StoreRegisterListTester_Case6 tester;
  tester.Test("cccc100000w0nnnnrrrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(25:20)=0000x1
//    = LoadRegisterList => LoadMultiple {'constraints': ,
//     'pattern': 'cccc100000w1nnnnrrrrrrrrrrrrrrrr',
//     'rule': 'Ldmda_Ldmfa_Rule_54_A1_P112'}
//
// Representative case:
// op(25:20)=0000x1
//    = LoadRegisterList => LoadMultiple {constraints: ,
//     pattern: cccc100000w1nnnnrrrrrrrrrrrrrrrr,
//     rule: Ldmda_Ldmfa_Rule_54_A1_P112}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_Case7_TestCase7) {
  LoadRegisterListTester_Case7 baseline_tester;
  NamedLoadMultiple_Ldmda_Ldmfa_Rule_54_A1_P112 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100000w1nnnnrrrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(25:20)=0010x0
//    = StoreRegisterList => StoreRegisterList {'constraints': ,
//     'pattern': 'cccc100010w0nnnnrrrrrrrrrrrrrrrr',
//     'rule': 'Stm_Stmia_Stmea_Rule_189_A1_P374'}
//
// Representaive case:
// op(25:20)=0010x0
//    = StoreRegisterList => StoreRegisterList {constraints: ,
//     pattern: cccc100010w0nnnnrrrrrrrrrrrrrrrr,
//     rule: Stm_Stmia_Stmea_Rule_189_A1_P374}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_Case8_TestCase8) {
  StoreRegisterListTester_Case8 tester;
  tester.Test("cccc100010w0nnnnrrrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(25:20)=0100x1
//    = LoadRegisterList => LoadMultiple {'constraints': ,
//     'pattern': 'cccc100100w1nnnnrrrrrrrrrrrrrrrr',
//     'rule': 'Ldmdb_Ldmea_Rule_55_A1_P114'}
//
// Representative case:
// op(25:20)=0100x1
//    = LoadRegisterList => LoadMultiple {constraints: ,
//     pattern: cccc100100w1nnnnrrrrrrrrrrrrrrrr,
//     rule: Ldmdb_Ldmea_Rule_55_A1_P114}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_Case9_TestCase9) {
  LoadRegisterListTester_Case9 baseline_tester;
  NamedLoadMultiple_Ldmdb_Ldmea_Rule_55_A1_P114 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100100w1nnnnrrrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(25:20)=0110x0
//    = StoreRegisterList => StoreRegisterList {'constraints': ,
//     'pattern': 'cccc100110w0nnnnrrrrrrrrrrrrrrrr',
//     'rule': 'Stmib_Stmfa_Rule_192_A1_P380'}
//
// Representaive case:
// op(25:20)=0110x0
//    = StoreRegisterList => StoreRegisterList {constraints: ,
//     pattern: cccc100110w0nnnnrrrrrrrrrrrrrrrr,
//     rule: Stmib_Stmfa_Rule_192_A1_P380}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_Case10_TestCase10) {
  StoreRegisterListTester_Case10 tester;
  tester.Test("cccc100110w0nnnnrrrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(25:20)=0110x1
//    = LoadRegisterList => LoadMultiple {'constraints': ,
//     'pattern': 'cccc100110w1nnnnrrrrrrrrrrrrrrrr',
//     'rule': 'Ldmib_Ldmed_Rule_56_A1_P116'}
//
// Representative case:
// op(25:20)=0110x1
//    = LoadRegisterList => LoadMultiple {constraints: ,
//     pattern: cccc100110w1nnnnrrrrrrrrrrrrrrrr,
//     rule: Ldmib_Ldmed_Rule_56_A1_P116}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_Case11_TestCase11) {
  LoadRegisterListTester_Case11 baseline_tester;
  NamedLoadMultiple_Ldmib_Ldmed_Rule_56_A1_P116 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100110w1nnnnrrrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(25:20)=0xx1x0 & inst(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc100pu100nnnnrrrrrrrrrrrrrrrr',
//     'rule': 'Stm_Rule_11_B6_A1_P22'}
//
// Representative case:
// op(25:20)=0xx1x0 & $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc100pu100nnnnrrrrrrrrrrrrrrrr,
//     rule: Stm_Rule_11_B6_A1_P22}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case12_TestCase12) {
  ForbiddenCondDecoderTester_Case12 baseline_tester;
  NamedForbidden_Stm_Rule_11_B6_A1_P22 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100pu100nnnnrrrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(25:20)=0xx1x1 & inst(15)=0 & inst(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc100pu101nnnn0rrrrrrrrrrrrrrr',
//     'rule': 'Ldm_Rule_3_B6_A1_P7'}
//
// Representative case:
// op(25:20)=0xx1x1 & R(15)=0 & $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc100pu101nnnn0rrrrrrrrrrrrrrr,
//     rule: Ldm_Rule_3_B6_A1_P7}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case13_TestCase13) {
  ForbiddenCondDecoderTester_Case13 baseline_tester;
  NamedForbidden_Ldm_Rule_3_B6_A1_P7 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100pu101nnnn0rrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(25:20)=0xx1x1 & inst(15)=1
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc100pu1w1nnnn1rrrrrrrrrrrrrrr',
//     'rule': 'Ldm_Rule_2_B6_A1_P5'}
//
// Representative case:
// op(25:20)=0xx1x1 & R(15)=1
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc100pu1w1nnnn1rrrrrrrrrrrrrrr,
//     rule: Ldm_Rule_2_B6_A1_P5}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case14_TestCase14) {
  ForbiddenCondDecoderTester_Case14 baseline_tester;
  NamedForbidden_Ldm_Rule_2_B6_A1_P5 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100pu1w1nnnn1rrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(25:20)=10xxxx
//    = BranchImmediate24 => Branch {'constraints': ,
//     'pattern': 'cccc1010iiiiiiiiiiiiiiiiiiiiiiii',
//     'rule': 'B_Rule_16_A1_P44'}
//
// Representative case:
// op(25:20)=10xxxx
//    = BranchImmediate24 => Branch {constraints: ,
//     pattern: cccc1010iiiiiiiiiiiiiiiiiiiiiiii,
//     rule: B_Rule_16_A1_P44}
TEST_F(Arm32DecoderStateTests,
       BranchImmediate24Tester_Case15_TestCase15) {
  BranchImmediate24Tester_Case15 baseline_tester;
  NamedBranch_B_Rule_16_A1_P44 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1010iiiiiiiiiiiiiiiiiiiiiiii");
}

// Neutral case:
// inst(25:20)=11xxxx
//    = BranchImmediate24 => Branch {'constraints': ,
//     'pattern': 'cccc1011iiiiiiiiiiiiiiiiiiiiiiii',
//     'rule': 'Bl_Blx_Rule_23_A1_P58'}
//
// Representative case:
// op(25:20)=11xxxx
//    = BranchImmediate24 => Branch {constraints: ,
//     pattern: cccc1011iiiiiiiiiiiiiiiiiiiiiiii,
//     rule: Bl_Blx_Rule_23_A1_P58}
TEST_F(Arm32DecoderStateTests,
       BranchImmediate24Tester_Case16_TestCase16) {
  BranchImmediate24Tester_Case16 baseline_tester;
  NamedBranch_Bl_Blx_Rule_23_A1_P58 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1011iiiiiiiiiiiiiiiiiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
