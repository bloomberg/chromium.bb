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
// inst(21:20)=01 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase0
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase0(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase1
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase1(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase2
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase2(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000040 /* op2(7:5)=~010 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase3
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase3(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase4
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase4(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000080 /* op2(7:5)=~100 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase5
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase5(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000E0 /* op2(7:5)=~111 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase6
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase6(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase7
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase7(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase8
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase8(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000040 /* op2(7:5)=~010 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase9
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase9(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase10
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase10(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000080 /* op2(7:5)=~100 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase11
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase11(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000E0 /* op2(7:5)=~111 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase12
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase12(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase13
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase13(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase14
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase14(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000040 /* op2(7:5)=~010 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase15
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase15(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase16
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase16(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000080 /* op2(7:5)=~100 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase17
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase17(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000E0 /* op2(7:5)=~111 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(21:20)=01 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Sadd16_Rule_148_A1_P296',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Sadd16_Rule_148_A1_P296,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case0
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase0 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case0()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase0(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sadd16_Rule_148_A1_P296_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Sasx_Rule_150_A1_P300',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Sasx_Rule_150_A1_P300,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case1
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase1 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case1()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase1(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sasx_Rule_150_A1_P300_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Ssax_Rule_185_A1_P366',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Ssax_Rule_185_A1_P366,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case2
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase2 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case2()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase2(
      state_.Binary3RegisterOpAltBNoCondUpdates_Ssax_Rule_185_A1_P366_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Ssub16_Rule_186_A1_P368',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Ssub16_Rule_186_A1_P368,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case3
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase3 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case3()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase3(
      state_.Binary3RegisterOpAltBNoCondUpdates_Ssub16_Rule_186_A1_P368_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Sadd8_Rule_149_A1_P298',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Sadd8_Rule_149_A1_P298,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case4
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase4 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case4()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase4(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sadd8_Rule_149_A1_P298_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Ssub8_Rule_187_A1_P370',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Ssub8_Rule_187_A1_P370,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case5
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase5 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case5()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase5(
      state_.Binary3RegisterOpAltBNoCondUpdates_Ssub8_Rule_187_A1_P370_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Qadd16_Rule_125_A1_P252',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Qadd16_Rule_125_A1_P252,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case6
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase6 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case6()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase6(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qadd16_Rule_125_A1_P252_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Qasx_Rule_127_A1_P256',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Qasx_Rule_127_A1_P256,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case7
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase7 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case7()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase7(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qasx_Rule_127_A1_P256_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Qsax_Rule_130_A1_P262',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Qsax_Rule_130_A1_P262,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case8
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase8 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case8()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase8(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qsax_Rule_130_A1_P262_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Qsub16_Rule_132_A1_P266',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Qsub16_Rule_132_A1_P266,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case9
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase9 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case9()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase9(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qsub16_Rule_132_A1_P266_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Qadd8_Rule_126_A1_P254',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Qadd8_Rule_126_A1_P254,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case10
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase10 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case10()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase10(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qadd8_Rule_126_A1_P254_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Qsub8_Rule_133_A1_P268',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Qsub8_Rule_133_A1_P268,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case11
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase11 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case11()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase11(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qsub8_Rule_133_A1_P268_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Shadd16_Rule_159_A1_P318',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Shadd16_Rule_159_A1_P318,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case12
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase12 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case12()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase12(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Shasx_Rule_161_A1_P322',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Shasx_Rule_161_A1_P322,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case13
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase13 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case13()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase13(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Shsax_Rule_162_A1_P324',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Shsax_Rule_162_A1_P324,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case14
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase14 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case14()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase14(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Shsub16_Rule_163_A1_P326',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Shsub16_Rule_163_A1_P326,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case15
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase15 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case15()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase15(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Shadd8_Rule_160_A1_P320',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Shadd8_Rule_160_A1_P320,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case16
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase16 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case16()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase16(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Shsub8_Rule_164_A1_P328',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Shsub8_Rule_164_A1_P328,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case17
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase17 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case17()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase17(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328_instance_)
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
// inst(21:20)=01 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100001nnnndddd11110001mmmm',
//       rule: 'Sadd16_Rule_148_A1_P296',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100001nnnndddd11110001mmmm,
//       rule: Sadd16_Rule_148_A1_P296,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case0_TestCase0) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case0 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110001mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100001nnnndddd11110011mmmm',
//       rule: 'Sasx_Rule_150_A1_P300',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100001nnnndddd11110011mmmm,
//       rule: Sasx_Rule_150_A1_P300,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case1_TestCase1) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case1 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110011mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100001nnnndddd11110101mmmm',
//       rule: 'Ssax_Rule_185_A1_P366',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100001nnnndddd11110101mmmm,
//       rule: Ssax_Rule_185_A1_P366,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case2_TestCase2) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case2 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110101mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100001nnnndddd11110111mmmm',
//       rule: 'Ssub16_Rule_186_A1_P368',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100001nnnndddd11110111mmmm,
//       rule: Ssub16_Rule_186_A1_P368,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case3_TestCase3) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case3 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110111mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100001nnnndddd11111001mmmm',
//       rule: 'Sadd8_Rule_149_A1_P298',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100001nnnndddd11111001mmmm,
//       rule: Sadd8_Rule_149_A1_P298,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case4_TestCase4) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case4 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd8_Rule_149_A1_P298 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11111001mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100001nnnndddd11111111mmmm',
//       rule: 'Ssub8_Rule_187_A1_P370',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100001nnnndddd11111111mmmm,
//       rule: Ssub8_Rule_187_A1_P370,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case5_TestCase5) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case5 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11111111mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100010nnnndddd11110001mmmm',
//       rule: 'Qadd16_Rule_125_A1_P252',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100010nnnndddd11110001mmmm,
//       rule: Qadd16_Rule_125_A1_P252,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case6_TestCase6) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case6 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110001mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100010nnnndddd11110011mmmm',
//       rule: 'Qasx_Rule_127_A1_P256',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100010nnnndddd11110011mmmm,
//       rule: Qasx_Rule_127_A1_P256,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case7_TestCase7) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case7 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110011mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100010nnnndddd11110101mmmm',
//       rule: 'Qsax_Rule_130_A1_P262',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100010nnnndddd11110101mmmm,
//       rule: Qsax_Rule_130_A1_P262,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case8_TestCase8) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case8 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110101mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100010nnnndddd11110111mmmm',
//       rule: 'Qsub16_Rule_132_A1_P266',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100010nnnndddd11110111mmmm,
//       rule: Qsub16_Rule_132_A1_P266,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case9_TestCase9) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case9 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110111mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100010nnnndddd11111001mmmm',
//       rule: 'Qadd8_Rule_126_A1_P254',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100010nnnndddd11111001mmmm,
//       rule: Qadd8_Rule_126_A1_P254,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case10_TestCase10) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case10 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11111001mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100010nnnndddd11111111mmmm',
//       rule: 'Qsub8_Rule_133_A1_P268',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100010nnnndddd11111111mmmm,
//       rule: Qsub8_Rule_133_A1_P268,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case11_TestCase11) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case11 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11111111mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100011nnnndddd11110001mmmm',
//       rule: 'Shadd16_Rule_159_A1_P318',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100011nnnndddd11110001mmmm,
//       rule: Shadd16_Rule_159_A1_P318,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case12_TestCase12) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case12 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110001mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100011nnnndddd11110011mmmm',
//       rule: 'Shasx_Rule_161_A1_P322',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100011nnnndddd11110011mmmm,
//       rule: Shasx_Rule_161_A1_P322,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case13_TestCase13) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case13 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110011mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100011nnnndddd11110101mmmm',
//       rule: 'Shsax_Rule_162_A1_P324',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100011nnnndddd11110101mmmm,
//       rule: Shsax_Rule_162_A1_P324,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case14_TestCase14) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case14 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110101mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100011nnnndddd11110111mmmm',
//       rule: 'Shsub16_Rule_163_A1_P326',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100011nnnndddd11110111mmmm,
//       rule: Shsub16_Rule_163_A1_P326,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case15_TestCase15) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case15 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110111mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100011nnnndddd11111001mmmm',
//       rule: 'Shadd8_Rule_160_A1_P320',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100011nnnndddd11111001mmmm,
//       rule: Shadd8_Rule_160_A1_P320,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case16_TestCase16) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case16 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11111001mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01100011nnnndddd11111111mmmm',
//       rule: 'Shsub8_Rule_164_A1_P328',
//       safety: ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01100011nnnndddd11111111mmmm,
//       rule: Shsub8_Rule_164_A1_P328,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case17_TestCase17) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case17 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11111111mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
