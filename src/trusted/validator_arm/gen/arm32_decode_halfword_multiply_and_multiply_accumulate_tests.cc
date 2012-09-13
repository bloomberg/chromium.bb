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
// inst(23:20)=0100
//    = Binary4RegisterDualResult {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op(23:20)=0100
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTesterCase0
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterCase0(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00400000 /* op(23:20)=~0100 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=0101
//    = UndefinedCondDecoder {'constraints': }
//
// Representaive case:
// op(23:20)=0101
//    = UndefinedCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase1
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase1(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00500000 /* op(23:20)=~0101 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=0110
//    = Binary4RegisterDualOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op(23:20)=0110
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterCase2
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase2(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00600000 /* op(23:20)=~0110 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=0111
//    = UndefinedCondDecoder {'constraints': }
//
// Representaive case:
// op(23:20)=0111
//    = UndefinedCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase3
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase3(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00700000 /* op(23:20)=~0111 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=000x & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op(23:20)=000x & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATesterCase4
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase4(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00E00000) != 0x00000000 /* op(23:20)=~000x */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=001x
//    = Binary4RegisterDualOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op(23:20)=001x
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterCase5
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase5(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00E00000) != 0x00200000 /* op(23:20)=~001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=100x
//    = Binary4RegisterDualResult {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op(23:20)=100x
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTesterCase6
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterCase6(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00E00000) != 0x00800000 /* op(23:20)=~100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=101x
//    = Binary4RegisterDualResult {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op(23:20)=101x
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTesterCase7
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterCase7(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00E00000) != 0x00A00000 /* op(23:20)=~101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=110x
//    = Binary4RegisterDualResult {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op(23:20)=110x
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTesterCase8
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterCase8(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00E00000) != 0x00C00000 /* op(23:20)=~110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=111x
//    = Binary4RegisterDualResult {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op(23:20)=111x
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTesterCase9
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterCase9(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00E00000) != 0x00E00000 /* op(23:20)=~111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:21)=00
//    = Binary4RegisterDualOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:21)=00
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterCase10
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase10(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00000000 /* op1(22:21)=~00 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:21)=01 & inst(5)=0
//    = Binary4RegisterDualOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:21)=01 & op(5)=0
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterCase11
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase11(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op1(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op(5)=~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:21)=01 & inst(5)=1 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:21)=01 & op(5)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATesterCase12
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase12(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op1(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000020 /* op(5)=~1 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:21)=10
//    = Binary4RegisterDualResult {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:21)=10
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTesterCase13
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterCase13(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00400000 /* op1(22:21)=~10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATesterCase14
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase14(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op1(22:21)=~11 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(23:20)=0100
//    = Binary4RegisterDualResult {'constraints': ,
//     'rule': 'Umaal_Rule_244_A1_P482',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=0100
//    = Binary4RegisterDualResult {constraints: ,
//     rule: Umaal_Rule_244_A1_P482,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_Case0
    : public Binary4RegisterDualResultTesterCase0 {
 public:
  Binary4RegisterDualResultTester_Case0()
    : Binary4RegisterDualResultTesterCase0(
      state_.Binary4RegisterDualResult_Umaal_Rule_244_A1_P482_instance_)
  {}
};

// Neutral case:
// inst(23:20)=0101
//    = UndefinedCondDecoder {'constraints': ,
//     'rule': 'Undefined_A5_2_5_0101'}
//
// Representative case:
// op(23:20)=0101
//    = UndefinedCondDecoder {constraints: ,
//     rule: Undefined_A5_2_5_0101}
class UndefinedCondDecoderTester_Case1
    : public UnsafeCondDecoderTesterCase1 {
 public:
  UndefinedCondDecoderTester_Case1()
    : UnsafeCondDecoderTesterCase1(
      state_.UndefinedCondDecoder_Undefined_A5_2_5_0101_instance_)
  {}
};

// Neutral case:
// inst(23:20)=0110
//    = Binary4RegisterDualOp {'constraints': ,
//     'rule': 'Mls_Rule_95_A1_P192',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=0110
//    = Binary4RegisterDualOp {constraints: ,
//     rule: Mls_Rule_95_A1_P192,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case2
    : public Binary4RegisterDualOpTesterCase2 {
 public:
  Binary4RegisterDualOpTester_Case2()
    : Binary4RegisterDualOpTesterCase2(
      state_.Binary4RegisterDualOp_Mls_Rule_95_A1_P192_instance_)
  {}
};

// Neutral case:
// inst(23:20)=0111
//    = UndefinedCondDecoder {'constraints': ,
//     'rule': 'Undefined_A5_2_5_0111'}
//
// Representative case:
// op(23:20)=0111
//    = UndefinedCondDecoder {constraints: ,
//     rule: Undefined_A5_2_5_0111}
class UndefinedCondDecoderTester_Case3
    : public UnsafeCondDecoderTesterCase3 {
 public:
  UndefinedCondDecoderTester_Case3()
    : UnsafeCondDecoderTesterCase3(
      state_.UndefinedCondDecoder_Undefined_A5_2_5_0111_instance_)
  {}
};

// Neutral case:
// inst(23:20)=000x & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {'constraints': ,
//     'rule': 'Mul_Rule_105_A1_P212',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=000x & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: Mul_Rule_105_A1_P212,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_Case4
    : public Binary3RegisterOpAltATesterCase4 {
 public:
  Binary3RegisterOpAltATester_Case4()
    : Binary3RegisterOpAltATesterCase4(
      state_.Binary3RegisterOpAltA_Mul_Rule_105_A1_P212_instance_)
  {}
};

// Neutral case:
// inst(23:20)=001x
//    = Binary4RegisterDualOp {'constraints': ,
//     'rule': 'Mla_Rule_94_A1_P190',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=001x
//    = Binary4RegisterDualOp {constraints: ,
//     rule: Mla_Rule_94_A1_P190,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case5
    : public Binary4RegisterDualOpTesterCase5 {
 public:
  Binary4RegisterDualOpTester_Case5()
    : Binary4RegisterDualOpTesterCase5(
      state_.Binary4RegisterDualOp_Mla_Rule_94_A1_P190_instance_)
  {}
};

// Neutral case:
// inst(23:20)=100x
//    = Binary4RegisterDualResult {'constraints': ,
//     'rule': 'Umull_Rule_246_A1_P486',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=100x
//    = Binary4RegisterDualResult {constraints: ,
//     rule: Umull_Rule_246_A1_P486,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_Case6
    : public Binary4RegisterDualResultTesterCase6 {
 public:
  Binary4RegisterDualResultTester_Case6()
    : Binary4RegisterDualResultTesterCase6(
      state_.Binary4RegisterDualResult_Umull_Rule_246_A1_P486_instance_)
  {}
};

// Neutral case:
// inst(23:20)=101x
//    = Binary4RegisterDualResult {'constraints': ,
//     'rule': 'Umlal_Rule_245_A1_P484',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=101x
//    = Binary4RegisterDualResult {constraints: ,
//     rule: Umlal_Rule_245_A1_P484,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_Case7
    : public Binary4RegisterDualResultTesterCase7 {
 public:
  Binary4RegisterDualResultTester_Case7()
    : Binary4RegisterDualResultTesterCase7(
      state_.Binary4RegisterDualResult_Umlal_Rule_245_A1_P484_instance_)
  {}
};

// Neutral case:
// inst(23:20)=110x
//    = Binary4RegisterDualResult {'constraints': ,
//     'rule': 'Smull_Rule_179_A1_P356',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=110x
//    = Binary4RegisterDualResult {constraints: ,
//     rule: Smull_Rule_179_A1_P356,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_Case8
    : public Binary4RegisterDualResultTesterCase8 {
 public:
  Binary4RegisterDualResultTester_Case8()
    : Binary4RegisterDualResultTesterCase8(
      state_.Binary4RegisterDualResult_Smull_Rule_179_A1_P356_instance_)
  {}
};

// Neutral case:
// inst(23:20)=111x
//    = Binary4RegisterDualResult {'constraints': ,
//     'rule': 'Smlal_Rule_168_A1_P334',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=111x
//    = Binary4RegisterDualResult {constraints: ,
//     rule: Smlal_Rule_168_A1_P334,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_Case9
    : public Binary4RegisterDualResultTesterCase9 {
 public:
  Binary4RegisterDualResultTester_Case9()
    : Binary4RegisterDualResultTesterCase9(
      state_.Binary4RegisterDualResult_Smlal_Rule_168_A1_P334_instance_)
  {}
};

// Neutral case:
// inst(22:21)=00
//    = Binary4RegisterDualOp {'constraints': ,
//     'rule': 'Smlaxx_Rule_166_A1_P330',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=00
//    = Binary4RegisterDualOp {constraints: ,
//     rule: Smlaxx_Rule_166_A1_P330,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case10
    : public Binary4RegisterDualOpTesterCase10 {
 public:
  Binary4RegisterDualOpTester_Case10()
    : Binary4RegisterDualOpTesterCase10(
      state_.Binary4RegisterDualOp_Smlaxx_Rule_166_A1_P330_instance_)
  {}
};

// Neutral case:
// inst(22:21)=01 & inst(5)=0
//    = Binary4RegisterDualOp {'constraints': ,
//     'rule': 'Smlawx_Rule_171_A1_340',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=01 & op(5)=0
//    = Binary4RegisterDualOp {constraints: ,
//     rule: Smlawx_Rule_171_A1_340,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case11
    : public Binary4RegisterDualOpTesterCase11 {
 public:
  Binary4RegisterDualOpTester_Case11()
    : Binary4RegisterDualOpTesterCase11(
      state_.Binary4RegisterDualOp_Smlawx_Rule_171_A1_340_instance_)
  {}
};

// Neutral case:
// inst(22:21)=01 & inst(5)=1 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {'constraints': ,
//     'rule': 'Smulwx_Rule_180_A1_P358',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=01 & op(5)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: Smulwx_Rule_180_A1_P358,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_Case12
    : public Binary3RegisterOpAltATesterCase12 {
 public:
  Binary3RegisterOpAltATester_Case12()
    : Binary3RegisterOpAltATesterCase12(
      state_.Binary3RegisterOpAltA_Smulwx_Rule_180_A1_P358_instance_)
  {}
};

// Neutral case:
// inst(22:21)=10
//    = Binary4RegisterDualResult {'constraints': ,
//     'rule': 'Smlalxx_Rule_169_A1_P336',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=10
//    = Binary4RegisterDualResult {constraints: ,
//     rule: Smlalxx_Rule_169_A1_P336,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_Case13
    : public Binary4RegisterDualResultTesterCase13 {
 public:
  Binary4RegisterDualResultTester_Case13()
    : Binary4RegisterDualResultTesterCase13(
      state_.Binary4RegisterDualResult_Smlalxx_Rule_169_A1_P336_instance_)
  {}
};

// Neutral case:
// inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {'constraints': ,
//     'rule': 'Smulxx_Rule_178_P354',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: Smulxx_Rule_178_P354,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_Case14
    : public Binary3RegisterOpAltATesterCase14 {
 public:
  Binary3RegisterOpAltATester_Case14()
    : Binary3RegisterOpAltATesterCase14(
      state_.Binary3RegisterOpAltA_Smulxx_Rule_178_P354_instance_)
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
// inst(23:20)=0100
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc00000100hhhhllllmmmm1001nnnn',
//     'rule': 'Umaal_Rule_244_A1_P482',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=0100
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc00000100hhhhllllmmmm1001nnnn,
//     rule: Umaal_Rule_244_A1_P482,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Case0_TestCase0) {
  Binary4RegisterDualResultTester_Case0 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Umaal_Rule_244_A1_P482 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00000100hhhhllllmmmm1001nnnn");
}

// Neutral case:
// inst(23:20)=0101
//    = UndefinedCondDecoder => Undefined {'constraints': ,
//     'pattern': 'cccc00000101xxxxxxxxxxxx1001xxxx',
//     'rule': 'Undefined_A5_2_5_0101'}
//
// Representative case:
// op(23:20)=0101
//    = UndefinedCondDecoder => Undefined {constraints: ,
//     pattern: cccc00000101xxxxxxxxxxxx1001xxxx,
//     rule: Undefined_A5_2_5_0101}
TEST_F(Arm32DecoderStateTests,
       UndefinedCondDecoderTester_Case1_TestCase1) {
  UndefinedCondDecoderTester_Case1 baseline_tester;
  NamedUndefined_Undefined_A5_2_5_0101 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00000101xxxxxxxxxxxx1001xxxx");
}

// Neutral case:
// inst(23:20)=0110
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {'constraints': ,
//     'pattern': 'cccc00000110ddddaaaammmm1001nnnn',
//     'rule': 'Mls_Rule_95_A1_P192',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=0110
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: cccc00000110ddddaaaammmm1001nnnn,
//     rule: Mls_Rule_95_A1_P192,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case2_TestCase2) {
  Binary4RegisterDualOpTester_Case2 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Mls_Rule_95_A1_P192 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00000110ddddaaaammmm1001nnnn");
}

// Neutral case:
// inst(23:20)=0111
//    = UndefinedCondDecoder => Undefined {'constraints': ,
//     'pattern': 'cccc00000111xxxxxxxxxxxx1001xxxx',
//     'rule': 'Undefined_A5_2_5_0111'}
//
// Representative case:
// op(23:20)=0111
//    = UndefinedCondDecoder => Undefined {constraints: ,
//     pattern: cccc00000111xxxxxxxxxxxx1001xxxx,
//     rule: Undefined_A5_2_5_0111}
TEST_F(Arm32DecoderStateTests,
       UndefinedCondDecoderTester_Case3_TestCase3) {
  UndefinedCondDecoderTester_Case3 baseline_tester;
  NamedUndefined_Undefined_A5_2_5_0111 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00000111xxxxxxxxxxxx1001xxxx");
}

// Neutral case:
// inst(23:20)=000x & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0000000sdddd0000mmmm1001nnnn',
//     'rule': 'Mul_Rule_105_A1_P212',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=000x & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc0000000sdddd0000mmmm1001nnnn,
//     rule: Mul_Rule_105_A1_P212,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case4_TestCase4) {
  Binary3RegisterOpAltATester_Case4 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Mul_Rule_105_A1_P212 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000sdddd0000mmmm1001nnnn");
}

// Neutral case:
// inst(23:20)=001x
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0000001sddddaaaammmm1001nnnn',
//     'rule': 'Mla_Rule_94_A1_P190',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=001x
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: cccc0000001sddddaaaammmm1001nnnn,
//     rule: Mla_Rule_94_A1_P190,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case5_TestCase5) {
  Binary4RegisterDualOpTester_Case5 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Mla_Rule_94_A1_P190 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001sddddaaaammmm1001nnnn");
}

// Neutral case:
// inst(23:20)=100x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0000100shhhhllllmmmm1001nnnn',
//     'rule': 'Umull_Rule_246_A1_P486',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=100x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc0000100shhhhllllmmmm1001nnnn,
//     rule: Umull_Rule_246_A1_P486,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Case6_TestCase6) {
  Binary4RegisterDualResultTester_Case6 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Umull_Rule_246_A1_P486 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100shhhhllllmmmm1001nnnn");
}

// Neutral case:
// inst(23:20)=101x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0000101shhhhllllmmmm1001nnnn',
//     'rule': 'Umlal_Rule_245_A1_P484',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=101x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc0000101shhhhllllmmmm1001nnnn,
//     rule: Umlal_Rule_245_A1_P484,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Case7_TestCase7) {
  Binary4RegisterDualResultTester_Case7 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Umlal_Rule_245_A1_P484 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101shhhhllllmmmm1001nnnn");
}

// Neutral case:
// inst(23:20)=110x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0000110shhhhllllmmmm1001nnnn',
//     'rule': 'Smull_Rule_179_A1_P356',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=110x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc0000110shhhhllllmmmm1001nnnn,
//     rule: Smull_Rule_179_A1_P356,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Case8_TestCase8) {
  Binary4RegisterDualResultTester_Case8 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smull_Rule_179_A1_P356 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110shhhhllllmmmm1001nnnn");
}

// Neutral case:
// inst(23:20)=111x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0000111shhhhllllmmmm1001nnnn',
//     'rule': 'Smlal_Rule_168_A1_P334',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(23:20)=111x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc0000111shhhhllllmmmm1001nnnn,
//     rule: Smlal_Rule_168_A1_P334,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Case9_TestCase9) {
  Binary4RegisterDualResultTester_Case9 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlal_Rule_168_A1_P334 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111shhhhllllmmmm1001nnnn");
}

// Neutral case:
// inst(22:21)=00
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {'constraints': ,
//     'pattern': 'cccc00010000ddddaaaammmm1xx0nnnn',
//     'rule': 'Smlaxx_Rule_166_A1_P330',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=00
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: cccc00010000ddddaaaammmm1xx0nnnn,
//     rule: Smlaxx_Rule_166_A1_P330,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case10_TestCase10) {
  Binary4RegisterDualOpTester_Case10 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010000ddddaaaammmm1xx0nnnn");
}

// Neutral case:
// inst(22:21)=01 & inst(5)=0
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {'constraints': ,
//     'pattern': 'cccc00010010ddddaaaammmm1x00nnnn',
//     'rule': 'Smlawx_Rule_171_A1_340',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=01 & op(5)=0
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: cccc00010010ddddaaaammmm1x00nnnn,
//     rule: Smlawx_Rule_171_A1_340,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case11_TestCase11) {
  Binary4RegisterDualOpTester_Case11 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010ddddaaaammmm1x00nnnn");
}

// Neutral case:
// inst(22:21)=01 & inst(5)=1 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc00010010dddd0000mmmm1x10nnnn',
//     'rule': 'Smulwx_Rule_180_A1_P358',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=01 & op(5)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc00010010dddd0000mmmm1x10nnnn,
//     rule: Smulwx_Rule_180_A1_P358,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case12_TestCase12) {
  Binary3RegisterOpAltATester_Case12 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010dddd0000mmmm1x10nnnn");
}

// Neutral case:
// inst(22:21)=10
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc00010100hhhhllllmmmm1xx0nnnn',
//     'rule': 'Smlalxx_Rule_169_A1_P336',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=10
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc00010100hhhhllllmmmm1xx0nnnn,
//     rule: Smlalxx_Rule_169_A1_P336,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Case13_TestCase13) {
  Binary4RegisterDualResultTester_Case13 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010100hhhhllllmmmm1xx0nnnn");
}

// Neutral case:
// inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc00010110dddd0000mmmm1xx0nnnn',
//     'rule': 'Smulxx_Rule_178_P354',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc00010110dddd0000mmmm1xx0nnnn,
//     rule: Smulxx_Rule_178_P354,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case14_TestCase14) {
  Binary3RegisterOpAltATester_Case14 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010110dddd0000mmmm1xx0nnnn");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
