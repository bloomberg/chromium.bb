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
// inst(22:20)=000 & inst(7:5)=00x & inst(15:12)=~1111
//    = {baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=~1111
//    = {baseline: Binary4RegisterDualOp,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterCase0
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase0(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20)=~000 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000000 /* op2(7:5)=~00x */) return false;
  if ((inst.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=00x & inst(15:12)=1111
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=1111
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltATesterCase1
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase1(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20)=~000 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000000 /* op2(7:5)=~00x */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=01x & inst(15:12)=~1111
//    = {baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=~1111
//    = {baseline: Binary4RegisterDualOp,
//       constraints: ,
//       safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20)=~000 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000040 /* op2(7:5)=~01x */) return false;
  if ((inst.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=01x & inst(15:12)=1111
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=1111
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltATesterCase3
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase3(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20)=~000 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000040 /* op2(7:5)=~01x */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=001 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: }
//
// Representaive case:
// op1(22:20)=001 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: }
class Binary3RegisterOpAltATesterCase4
    : public Binary3RegisterOpAltATester {
 public:
  Binary3RegisterOpAltATesterCase4(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00100000 /* op1(22:20)=~001 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: }
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: }
class Binary3RegisterOpAltATesterCase5
    : public Binary3RegisterOpAltATester {
 public:
  Binary3RegisterOpAltATesterCase5(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00300000 /* op1(22:20)=~011 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=00x
//    = {baseline: 'Binary4RegisterDualResult',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=100 & op2(7:5)=00x
//    = {baseline: Binary4RegisterDualResult,
//       constraints: ,
//       safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00700000) != 0x00400000 /* op1(22:20)=~100 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000000 /* op2(7:5)=~00x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=01x
//    = {baseline: 'Binary4RegisterDualResult',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=100 & op2(7:5)=01x
//    = {baseline: Binary4RegisterDualResult,
//       constraints: ,
//       safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00700000) != 0x00400000 /* op1(22:20)=~100 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000040 /* op2(7:5)=~01x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=101 & inst(7:5)=00x & inst(15:12)=~1111
//    = {baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=~1111
//    = {baseline: Binary4RegisterDualOp,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterCase8
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase8(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00500000 /* op1(22:20)=~101 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000000 /* op2(7:5)=~00x */) return false;
  if ((inst.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=101 & inst(7:5)=00x & inst(15:12)=1111
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=1111
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltATesterCase9
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase9(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00500000 /* op1(22:20)=~101 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000000 /* op2(7:5)=~00x */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=101 & inst(7:5)=11x
//    = {baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=101 & op2(7:5)=11x
//    = {baseline: Binary4RegisterDualOp,
//       constraints: ,
//       safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00700000) != 0x00500000 /* op1(22:20)=~101 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x000000C0 /* op2(7:5)=~11x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(22:20)=000 & inst(7:5)=00x & inst(15:12)=~1111
//    = {baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       rule: 'Smlad_Rule_167_P332',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=~1111
//    = {baseline: Binary4RegisterDualOp,
//       constraints: ,
//       rule: Smlad_Rule_167_P332,
//       safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case0
    : public Binary4RegisterDualOpTesterCase0 {
 public:
  Binary4RegisterDualOpTester_Case0()
    : Binary4RegisterDualOpTesterCase0(
      state_.Binary4RegisterDualOp_Smlad_Rule_167_P332_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=00x & inst(15:12)=1111
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       rule: 'Smuad_Rule_177_P352',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=1111
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       rule: Smuad_Rule_177_P352,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_Case1
    : public Binary3RegisterOpAltATesterCase1 {
 public:
  Binary3RegisterOpAltATester_Case1()
    : Binary3RegisterOpAltATesterCase1(
      state_.Binary3RegisterOpAltA_Smuad_Rule_177_P352_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=01x & inst(15:12)=~1111
//    = {baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       rule: 'Smlsd_Rule_172_P342',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=~1111
//    = {baseline: Binary4RegisterDualOp,
//       constraints: ,
//       rule: Smlsd_Rule_172_P342,
//       safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case2
    : public Binary4RegisterDualOpTesterCase2 {
 public:
  Binary4RegisterDualOpTester_Case2()
    : Binary4RegisterDualOpTesterCase2(
      state_.Binary4RegisterDualOp_Smlsd_Rule_172_P342_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=01x & inst(15:12)=1111
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       rule: 'Smusd_Rule_181_P360',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=1111
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       rule: Smusd_Rule_181_P360,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_Case3
    : public Binary3RegisterOpAltATesterCase3 {
 public:
  Binary3RegisterOpAltATester_Case3()
    : Binary3RegisterOpAltATesterCase3(
      state_.Binary3RegisterOpAltA_Smusd_Rule_181_P360_instance_)
  {}
};

// Neutral case:
// inst(22:20)=001 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       rule: 'Sdiv_Rule_A1'}
//
// Representative case:
// op1(22:20)=001 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       rule: Sdiv_Rule_A1}
class Binary3RegisterOpAltATester_Case4
    : public Binary3RegisterOpAltATesterCase4 {
 public:
  Binary3RegisterOpAltATester_Case4()
    : Binary3RegisterOpAltATesterCase4(
      state_.Binary3RegisterOpAltA_Sdiv_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(22:20)=011 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       rule: 'Udiv_Rule_A1'}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       rule: Udiv_Rule_A1}
class Binary3RegisterOpAltATester_Case5
    : public Binary3RegisterOpAltATesterCase5 {
 public:
  Binary3RegisterOpAltATester_Case5()
    : Binary3RegisterOpAltATesterCase5(
      state_.Binary3RegisterOpAltA_Udiv_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(22:20)=100 & inst(7:5)=00x
//    = {baseline: 'Binary4RegisterDualResult',
//       constraints: ,
//       rule: 'Smlald_Rule_170_P336',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=00x
//    = {baseline: Binary4RegisterDualResult,
//       constraints: ,
//       rule: Smlald_Rule_170_P336,
//       safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_Case6
    : public Binary4RegisterDualResultTesterCase6 {
 public:
  Binary4RegisterDualResultTester_Case6()
    : Binary4RegisterDualResultTesterCase6(
      state_.Binary4RegisterDualResult_Smlald_Rule_170_P336_instance_)
  {}
};

// Neutral case:
// inst(22:20)=100 & inst(7:5)=01x
//    = {baseline: 'Binary4RegisterDualResult',
//       constraints: ,
//       rule: 'Smlsld_Rule_173_P344',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=01x
//    = {baseline: Binary4RegisterDualResult,
//       constraints: ,
//       rule: Smlsld_Rule_173_P344,
//       safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_Case7
    : public Binary4RegisterDualResultTesterCase7 {
 public:
  Binary4RegisterDualResultTester_Case7()
    : Binary4RegisterDualResultTesterCase7(
      state_.Binary4RegisterDualResult_Smlsld_Rule_173_P344_instance_)
  {}
};

// Neutral case:
// inst(22:20)=101 & inst(7:5)=00x & inst(15:12)=~1111
//    = {baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       rule: 'Smmla_Rule_174_P346',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=~1111
//    = {baseline: Binary4RegisterDualOp,
//       constraints: ,
//       rule: Smmla_Rule_174_P346,
//       safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case8
    : public Binary4RegisterDualOpTesterCase8 {
 public:
  Binary4RegisterDualOpTester_Case8()
    : Binary4RegisterDualOpTesterCase8(
      state_.Binary4RegisterDualOp_Smmla_Rule_174_P346_instance_)
  {}
};

// Neutral case:
// inst(22:20)=101 & inst(7:5)=00x & inst(15:12)=1111
//    = {baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       rule: 'Smmul_Rule_176_P350',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=1111
//    = {baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       rule: Smmul_Rule_176_P350,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_Case9
    : public Binary3RegisterOpAltATesterCase9 {
 public:
  Binary3RegisterOpAltATester_Case9()
    : Binary3RegisterOpAltATesterCase9(
      state_.Binary3RegisterOpAltA_Smmul_Rule_176_P350_instance_)
  {}
};

// Neutral case:
// inst(22:20)=101 & inst(7:5)=11x
//    = {baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       rule: 'Smmls_Rule_175_P348',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=101 & op2(7:5)=11x
//    = {baseline: Binary4RegisterDualOp,
//       constraints: ,
//       rule: Smmls_Rule_175_P348,
//       safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case10
    : public Binary4RegisterDualOpTesterCase10 {
 public:
  Binary4RegisterDualOpTester_Case10()
    : Binary4RegisterDualOpTesterCase10(
      state_.Binary4RegisterDualOp_Smmls_Rule_175_P348_instance_)
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
// inst(22:20)=000 & inst(7:5)=00x & inst(15:12)=~1111
//    = {actual: 'Defs16To19CondsDontCareRdRaRmRnNotPc',
//       baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       pattern: 'cccc01110000ddddaaaammmm00m1nnnn',
//       rule: 'Smlad_Rule_167_P332',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=~1111
//    = {actual: Defs16To19CondsDontCareRdRaRmRnNotPc,
//       baseline: Binary4RegisterDualOp,
//       constraints: ,
//       pattern: cccc01110000ddddaaaammmm00m1nnnn,
//       rule: Smlad_Rule_167_P332,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case0_TestCase0) {
  Binary4RegisterDualOpTester_Case0 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000ddddaaaammmm00m1nnnn");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=00x & inst(15:12)=1111
//    = {actual: 'Defs16To19CondsDontCareRdRmRnNotPc',
//       baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       pattern: 'cccc01110000dddd1111mmmm00m1nnnn',
//       rule: 'Smuad_Rule_177_P352',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=1111
//    = {actual: Defs16To19CondsDontCareRdRmRnNotPc,
//       baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       pattern: cccc01110000dddd1111mmmm00m1nnnn,
//       rule: Smuad_Rule_177_P352,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case1_TestCase1) {
  Binary3RegisterOpAltATester_Case1 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000dddd1111mmmm00m1nnnn");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=01x & inst(15:12)=~1111
//    = {actual: 'Defs16To19CondsDontCareRdRaRmRnNotPc',
//       baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       pattern: 'cccc01110000ddddaaaammmm01m1nnnn',
//       rule: 'Smlsd_Rule_172_P342',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=~1111
//    = {actual: Defs16To19CondsDontCareRdRaRmRnNotPc,
//       baseline: Binary4RegisterDualOp,
//       constraints: ,
//       pattern: cccc01110000ddddaaaammmm01m1nnnn,
//       rule: Smlsd_Rule_172_P342,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case2_TestCase2) {
  Binary4RegisterDualOpTester_Case2 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000ddddaaaammmm01m1nnnn");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=01x & inst(15:12)=1111
//    = {actual: 'Defs16To19CondsDontCareRdRmRnNotPc',
//       baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       pattern: 'cccc01110000dddd1111mmmm01m1nnnn',
//       rule: 'Smusd_Rule_181_P360',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=1111
//    = {actual: Defs16To19CondsDontCareRdRmRnNotPc,
//       baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       pattern: cccc01110000dddd1111mmmm01m1nnnn,
//       rule: Smusd_Rule_181_P360,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case3_TestCase3) {
  Binary3RegisterOpAltATester_Case3 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000dddd1111mmmm01m1nnnn");
}

// Neutral case:
// inst(22:20)=001 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'Defs16To19CondsDontCareRdRmRnNotPc',
//       baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       pattern: 'cccc01110001dddd1111mmmm0001nnnn',
//       rule: 'Sdiv_Rule_A1'}
//
// Representative case:
// op1(22:20)=001 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: Defs16To19CondsDontCareRdRmRnNotPc,
//       baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       pattern: cccc01110001dddd1111mmmm0001nnnn,
//       rule: Sdiv_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case4_TestCase4) {
  Binary3RegisterOpAltATester_Case4 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Sdiv_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110001dddd1111mmmm0001nnnn");
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'Defs16To19CondsDontCareRdRmRnNotPc',
//       baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       pattern: 'cccc01110011dddd1111mmmm0001nnnn',
//       rule: 'Udiv_Rule_A1'}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: Defs16To19CondsDontCareRdRmRnNotPc,
//       baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       pattern: cccc01110011dddd1111mmmm0001nnnn,
//       rule: Udiv_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case5_TestCase5) {
  Binary3RegisterOpAltATester_Case5 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Udiv_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110011dddd1111mmmm0001nnnn");
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=00x
//    = {actual: 'Defs12To19CondsDontCareRdRmRnNotPc',
//       baseline: 'Binary4RegisterDualResult',
//       constraints: ,
//       pattern: 'cccc01110100hhhhllllmmmm00m1nnnn',
//       rule: 'Smlald_Rule_170_P336',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=00x
//    = {actual: Defs12To19CondsDontCareRdRmRnNotPc,
//       baseline: Binary4RegisterDualResult,
//       constraints: ,
//       pattern: cccc01110100hhhhllllmmmm00m1nnnn,
//       rule: Smlald_Rule_170_P336,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Case6_TestCase6) {
  Binary4RegisterDualResultTester_Case6 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110100hhhhllllmmmm00m1nnnn");
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=01x
//    = {actual: 'Defs12To19CondsDontCareRdRmRnNotPc',
//       baseline: 'Binary4RegisterDualResult',
//       constraints: ,
//       pattern: 'cccc01110100hhhhllllmmmm01m1nnnn',
//       rule: 'Smlsld_Rule_173_P344',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=01x
//    = {actual: Defs12To19CondsDontCareRdRmRnNotPc,
//       baseline: Binary4RegisterDualResult,
//       constraints: ,
//       pattern: cccc01110100hhhhllllmmmm01m1nnnn,
//       rule: Smlsld_Rule_173_P344,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Case7_TestCase7) {
  Binary4RegisterDualResultTester_Case7 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110100hhhhllllmmmm01m1nnnn");
}

// Neutral case:
// inst(22:20)=101 & inst(7:5)=00x & inst(15:12)=~1111
//    = {actual: 'Defs16To19CondsDontCareRdRaRmRnNotPc',
//       baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       pattern: 'cccc01110101ddddaaaammmm00r1nnnn',
//       rule: 'Smmla_Rule_174_P346',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=~1111
//    = {actual: Defs16To19CondsDontCareRdRaRmRnNotPc,
//       baseline: Binary4RegisterDualOp,
//       constraints: ,
//       pattern: cccc01110101ddddaaaammmm00r1nnnn,
//       rule: Smmla_Rule_174_P346,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case8_TestCase8) {
  Binary4RegisterDualOpTester_Case8 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110101ddddaaaammmm00r1nnnn");
}

// Neutral case:
// inst(22:20)=101 & inst(7:5)=00x & inst(15:12)=1111
//    = {actual: 'Defs16To19CondsDontCareRdRmRnNotPc',
//       baseline: 'Binary3RegisterOpAltA',
//       constraints: ,
//       pattern: 'cccc01110101dddd1111mmmm00r1nnnn',
//       rule: 'Smmul_Rule_176_P350',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=1111
//    = {actual: Defs16To19CondsDontCareRdRmRnNotPc,
//       baseline: Binary3RegisterOpAltA,
//       constraints: ,
//       pattern: cccc01110101dddd1111mmmm00r1nnnn,
//       rule: Smmul_Rule_176_P350,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case9_TestCase9) {
  Binary3RegisterOpAltATester_Case9 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110101dddd1111mmmm00r1nnnn");
}

// Neutral case:
// inst(22:20)=101 & inst(7:5)=11x
//    = {actual: 'Defs16To19CondsDontCareRdRaRmRnNotPc',
//       baseline: 'Binary4RegisterDualOp',
//       constraints: ,
//       pattern: 'cccc01110101ddddaaaammmm11r1nnnn',
//       rule: 'Smmls_Rule_175_P348',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=101 & op2(7:5)=11x
//    = {actual: Defs16To19CondsDontCareRdRaRmRnNotPc,
//       baseline: Binary4RegisterDualOp,
//       constraints: ,
//       pattern: cccc01110101ddddaaaammmm11r1nnnn,
//       rule: Smmls_Rule_175_P348,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case10_TestCase10) {
  Binary4RegisterDualOpTester_Case10 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110101ddddaaaammmm11r1nnnn");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
