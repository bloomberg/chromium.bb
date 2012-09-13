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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=10 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=11 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uadd16_Rule_233_A1_P460',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uadd16_Rule_233_A1_P460,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case0
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase0 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case0()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase0(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uadd16_Rule_233_A1_P460_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uasx_Rule_235_A1_P464',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uasx_Rule_235_A1_P464,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case1
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase1 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case1()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase1(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uasx_Rule_235_A1_P464_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Usax_Rule_257_A1_P508',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Usax_Rule_257_A1_P508,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case2
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase2 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case2()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase2(
      state_.Binary3RegisterOpAltBNoCondUpdates_Usax_Rule_257_A1_P508_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Usub16_Rule_258_A1_P510',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Usub16_Rule_258_A1_P510,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case3
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase3 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case3()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase3(
      state_.Binary3RegisterOpAltBNoCondUpdates_Usub16_Rule_258_A1_P510_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uadd8_Rule_234_A1_P462',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uadd8_Rule_234_A1_P462,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case4
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase4 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case4()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase4(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uadd8_Rule_234_A1_P462_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Usub8_Rule_259_A1_P512',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Usub8_Rule_259_A1_P512,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case5
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase5 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case5()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase5(
      state_.Binary3RegisterOpAltBNoCondUpdates_Usub8_Rule_259_A1_P512_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uqadd16_Rule_247_A1_P488',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uqadd16_Rule_247_A1_P488,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case6
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase6 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case6()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase6(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uqadd16_Rule_247_A1_P488_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uqasx_Rule_249_A1_P492',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uqasx_Rule_249_A1_P492,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case7
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase7 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case7()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase7(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uqasx_Rule_249_A1_P492_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uqsax_Rule_250_A1_P494',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uqsax_Rule_250_A1_P494,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case8
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase8 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case8()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase8(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uqsax_Rule_250_A1_P494_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uqsub16_Rule_251_A1_P496',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uqsub16_Rule_251_A1_P496,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case9
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase9 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case9()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase9(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uqsub16_Rule_251_A1_P496_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uqadd8_Rule_248_A1_P490',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uqadd8_Rule_248_A1_P490,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case10
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase10 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case10()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase10(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uqadd8_Rule_248_A1_P490_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uqsub8_Rule_252_A1_P498',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uqsub8_Rule_252_A1_P498,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case11
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase11 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case11()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase11(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uqsub8_Rule_252_A1_P498_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uhadd16_Rule_238_A1_P470',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uhadd16_Rule_238_A1_P470,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case12
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase12 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case12()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase12(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uhasx_Rule_240_A1_P474',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uhasx_Rule_240_A1_P474,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case13
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase13 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case13()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase13(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uhsax_Rule_241_A1_P476',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uhsax_Rule_241_A1_P476,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case14
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase14 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case14()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase14(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uhsub16_Rule_242_A1_P478',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uhsub16_Rule_242_A1_P478,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case15
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase15 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case15()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase15(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uhadd8_Rule_239_A1_P472',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uhadd8_Rule_239_A1_P472,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case16
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase16 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case16()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase16(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uhsub8_Rule_243_A1_P480',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uhsub8_Rule_243_A1_P480,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case17
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase17 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case17()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase17(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480_instance_)
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
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100101nnnndddd11110001mmmm',
//     'rule': 'Uadd16_Rule_233_A1_P460',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100101nnnndddd11110001mmmm,
//     rule: Uadd16_Rule_233_A1_P460,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case0_TestCase0) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case0 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd16_Rule_233_A1_P460 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100101nnnndddd11110001mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100101nnnndddd11110011mmmm',
//     'rule': 'Uasx_Rule_235_A1_P464',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100101nnnndddd11110011mmmm,
//     rule: Uasx_Rule_235_A1_P464,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case1_TestCase1) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case1 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uasx_Rule_235_A1_P464 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100101nnnndddd11110011mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100101nnnndddd11110101mmmm',
//     'rule': 'Usax_Rule_257_A1_P508',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100101nnnndddd11110101mmmm,
//     rule: Usax_Rule_257_A1_P508,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case2_TestCase2) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case2 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Usax_Rule_257_A1_P508 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100101nnnndddd11110101mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100101nnnndddd11110111mmmm',
//     'rule': 'Usub16_Rule_258_A1_P510',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100101nnnndddd11110111mmmm,
//     rule: Usub16_Rule_258_A1_P510,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case3_TestCase3) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case3 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub16_Rule_258_A1_P510 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100101nnnndddd11110111mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100101nnnndddd11111001mmmm',
//     'rule': 'Uadd8_Rule_234_A1_P462',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100101nnnndddd11111001mmmm,
//     rule: Uadd8_Rule_234_A1_P462,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case4_TestCase4) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case4 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd8_Rule_234_A1_P462 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100101nnnndddd11111001mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100101nnnndddd11111111mmmm',
//     'rule': 'Usub8_Rule_259_A1_P512',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100101nnnndddd11111111mmmm,
//     rule: Usub8_Rule_259_A1_P512,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case5_TestCase5) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case5 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub8_Rule_259_A1_P512 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100101nnnndddd11111111mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100110nnnndddd11110001mmmm',
//     'rule': 'Uqadd16_Rule_247_A1_P488',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100110nnnndddd11110001mmmm,
//     rule: Uqadd16_Rule_247_A1_P488,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case6_TestCase6) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case6 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd16_Rule_247_A1_P488 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100110nnnndddd11110001mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100110nnnndddd11110011mmmm',
//     'rule': 'Uqasx_Rule_249_A1_P492',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100110nnnndddd11110011mmmm,
//     rule: Uqasx_Rule_249_A1_P492,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case7_TestCase7) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case7 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqasx_Rule_249_A1_P492 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100110nnnndddd11110011mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100110nnnndddd11110101mmmm',
//     'rule': 'Uqsax_Rule_250_A1_P494',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100110nnnndddd11110101mmmm,
//     rule: Uqsax_Rule_250_A1_P494,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case8_TestCase8) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case8 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsax_Rule_250_A1_P494 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100110nnnndddd11110101mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100110nnnndddd11110111mmmm',
//     'rule': 'Uqsub16_Rule_251_A1_P496',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100110nnnndddd11110111mmmm,
//     rule: Uqsub16_Rule_251_A1_P496,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case9_TestCase9) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case9 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub16_Rule_251_A1_P496 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100110nnnndddd11110111mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100110nnnndddd11111001mmmm',
//     'rule': 'Uqadd8_Rule_248_A1_P490',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100110nnnndddd11111001mmmm,
//     rule: Uqadd8_Rule_248_A1_P490,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case10_TestCase10) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case10 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd8_Rule_248_A1_P490 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100110nnnndddd11111001mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100110nnnndddd11111111mmmm',
//     'rule': 'Uqsub8_Rule_252_A1_P498',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100110nnnndddd11111111mmmm,
//     rule: Uqsub8_Rule_252_A1_P498,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case11_TestCase11) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case11 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub8_Rule_252_A1_P498 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100110nnnndddd11111111mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100111nnnndddd11110001mmmm',
//     'rule': 'Uhadd16_Rule_238_A1_P470',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100111nnnndddd11110001mmmm,
//     rule: Uhadd16_Rule_238_A1_P470,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case12_TestCase12) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case12 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd16_Rule_238_A1_P470 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100111nnnndddd11110001mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100111nnnndddd11110011mmmm',
//     'rule': 'Uhasx_Rule_240_A1_P474',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100111nnnndddd11110011mmmm,
//     rule: Uhasx_Rule_240_A1_P474,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case13_TestCase13) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case13 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhasx_Rule_240_A1_P474 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100111nnnndddd11110011mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100111nnnndddd11110101mmmm',
//     'rule': 'Uhsax_Rule_241_A1_P476',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100111nnnndddd11110101mmmm,
//     rule: Uhsax_Rule_241_A1_P476,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case14_TestCase14) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case14 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsax_Rule_241_A1_P476 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100111nnnndddd11110101mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100111nnnndddd11110111mmmm',
//     'rule': 'Uhsub16_Rule_242_A1_P478',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100111nnnndddd11110111mmmm,
//     rule: Uhsub16_Rule_242_A1_P478,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case15_TestCase15) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case15 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub16_Rule_242_A1_P478 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100111nnnndddd11110111mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100111nnnndddd11111001mmmm',
//     'rule': 'Uhadd8_Rule_239_A1_P472',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100111nnnndddd11111001mmmm,
//     rule: Uhadd8_Rule_239_A1_P472,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case16_TestCase16) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case16 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd8_Rule_239_A1_P472 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100111nnnndddd11111001mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100111nnnndddd11111111mmmm',
//     'rule': 'Uhsub8_Rule_243_A1_P480',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100111nnnndddd11111111mmmm,
//     rule: Uhsub8_Rule_243_A1_P480,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case17_TestCase17) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case17 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub8_Rule_243_A1_P480 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100111nnnndddd11111111mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
