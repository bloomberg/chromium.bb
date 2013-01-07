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
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterImmedShiftedOpRegsNotPc',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Binary3RegisterImmedShiftedOpRegsNotPc,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterImmedShiftedOpTesterCase0
    : public Binary3RegisterImmedShiftedOpTesterRegsNotPc {
 public:
  Binary3RegisterImmedShiftedOpTesterCase0(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~000
  if ((inst.Bits() & 0x00700000)  !=
          0x00000000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOpRegsNotPc',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Unary2RegisterImmedShiftedOpRegsNotPc,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Unary2RegisterImmedShiftedOpRegsNotPcTesterCase1
    : public Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTesterCase1(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpRegsNotPcTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~000
  if ((inst.Bits() & 0x00700000)  !=
          0x00000000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
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
  // op1(22:20)=~000
  if ((inst.Bits() & 0x00700000)  !=
          0x00000000) return false;
  // op2(7:5)=~101
  if ((inst.Bits() & 0x000000E0)  !=
          0x000000A0) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=xx0
//    = {baseline: 'Binary3RegisterImmedShiftedOpRegsNotPc',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=xx0
//    = {baseline: Binary3RegisterImmedShiftedOpRegsNotPc,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Binary3RegisterImmedShiftedOpTesterCase3
    : public Binary3RegisterImmedShiftedOpTesterRegsNotPc {
 public:
  Binary3RegisterImmedShiftedOpTesterCase3(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~000
  if ((inst.Bits() & 0x00700000)  !=
          0x00000000) return false;
  // op2(7:5)=~xx0
  if ((inst.Bits() & 0x00000020)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=010 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTesterCase4
    : public Unary2RegisterSatImmedShiftedOpTesterRegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTesterCase4(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~010
  if ((inst.Bits() & 0x00700000)  !=
          0x00200000) return false;
  // op2(7:5)=~001
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000020) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
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
  // op1(22:20)=~010
  if ((inst.Bits() & 0x00700000)  !=
          0x00200000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOpRegsNotPc',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Unary2RegisterImmedShiftedOpRegsNotPc,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Unary2RegisterImmedShiftedOpRegsNotPcTesterCase6
    : public Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTesterCase6(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpRegsNotPcTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~010
  if ((inst.Bits() & 0x00700000)  !=
          0x00200000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase7
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase7(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~011
  if ((inst.Bits() & 0x00700000)  !=
          0x00300000) return false;
  // op2(7:5)=~001
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000020) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx
  if ((inst.Bits() & 0x000F0F00)  !=
          0x000F0F00) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
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
  // op1(22:20)=~011
  if ((inst.Bits() & 0x00700000)  !=
          0x00300000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase9
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase9(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~011
  if ((inst.Bits() & 0x00700000)  !=
          0x00300000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase10
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase10(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~011
  if ((inst.Bits() & 0x00700000)  !=
          0x00300000) return false;
  // op2(7:5)=~101
  if ((inst.Bits() & 0x000000E0)  !=
          0x000000A0) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx
  if ((inst.Bits() & 0x000F0F00)  !=
          0x000F0F00) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
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
  // op1(22:20)=~100
  if ((inst.Bits() & 0x00700000)  !=
          0x00400000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase12
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase12(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~100
  if ((inst.Bits() & 0x00700000)  !=
          0x00400000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=110 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTesterCase13
    : public Unary2RegisterSatImmedShiftedOpTesterRegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTesterCase13(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~110
  if ((inst.Bits() & 0x00700000)  !=
          0x00600000) return false;
  // op2(7:5)=~001
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000020) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
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
  // op1(22:20)=~110
  if ((inst.Bits() & 0x00700000)  !=
          0x00600000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase15
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase15(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~110
  if ((inst.Bits() & 0x00700000)  !=
          0x00600000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase16
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase16(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~111
  if ((inst.Bits() & 0x00700000)  !=
          0x00700000) return false;
  // op2(7:5)=~001
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000020) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx
  if ((inst.Bits() & 0x000F0F00)  !=
          0x000F0F00) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
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
  // op1(22:20)=~111
  if ((inst.Bits() & 0x00700000)  !=
          0x00700000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase18
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase18(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~111
  if ((inst.Bits() & 0x00700000)  !=
          0x00700000) return false;
  // op2(7:5)=~011
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000060) return false;
  // A(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
  if ((inst.Bits() & 0x00000300)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase19
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase19(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~111
  if ((inst.Bits() & 0x00700000)  !=
          0x00700000) return false;
  // op2(7:5)=~101
  if ((inst.Bits() & 0x000000E0)  !=
          0x000000A0) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx
  if ((inst.Bits() & 0x000F0F00)  !=
          0x000F0F00) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=01x & inst(7:5)=xx0
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=01x & op2(7:5)=xx0
//    = {baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTesterCase20
    : public Unary2RegisterSatImmedShiftedOpTesterRegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTesterCase20(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterCase20
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~01x
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // op2(7:5)=~xx0
  if ((inst.Bits() & 0x00000020)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=11x & inst(7:5)=xx0
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       safety: ['RegsNotPc']}
//
// Representaive case:
// op1(22:20)=11x & op2(7:5)=xx0
//    = {baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTesterCase21
    : public Unary2RegisterSatImmedShiftedOpTesterRegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTesterCase21(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterCase21
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~11x
  if ((inst.Bits() & 0x00600000)  !=
          0x00600000) return false;
  // op2(7:5)=~xx0
  if ((inst.Bits() & 0x00000020)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterImmedShiftedOpRegsNotPc',
//       constraints: ,
//       rule: 'Sxtab16_Rule_221_A1_P436',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Binary3RegisterImmedShiftedOpRegsNotPc,
//       constraints: ,
//       rule: Sxtab16_Rule_221_A1_P436,
//       safety: ['RegsNotPc']}
class Binary3RegisterImmedShiftedOpRegsNotPcTester_Case0
    : public Binary3RegisterImmedShiftedOpTesterCase0 {
 public:
  Binary3RegisterImmedShiftedOpRegsNotPcTester_Case0()
    : Binary3RegisterImmedShiftedOpTesterCase0(
      state_.Binary3RegisterImmedShiftedOpRegsNotPc_Sxtab16_Rule_221_A1_P436_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOpRegsNotPc',
//       constraints: ,
//       rule: 'Sxtb16_Rule_224_A1_P442',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Unary2RegisterImmedShiftedOpRegsNotPc,
//       constraints: ,
//       rule: Sxtb16_Rule_224_A1_P442,
//       safety: ['RegsNotPc']}
class Unary2RegisterImmedShiftedOpRegsNotPcTester_Case1
    : public Unary2RegisterImmedShiftedOpRegsNotPcTesterCase1 {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTester_Case1()
    : Unary2RegisterImmedShiftedOpRegsNotPcTesterCase1(
      state_.Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb16_Rule_224_A1_P442_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Sel_Rule_156_A1_P312',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Sel_Rule_156_A1_P312,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case2
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase2 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case2()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase2(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sel_Rule_156_A1_P312_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=xx0
//    = {baseline: 'Binary3RegisterImmedShiftedOpRegsNotPc',
//       constraints: ,
//       rule: 'Pkh_Rule_116_A1_P234',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=xx0
//    = {baseline: Binary3RegisterImmedShiftedOpRegsNotPc,
//       constraints: ,
//       rule: Pkh_Rule_116_A1_P234,
//       safety: ['RegsNotPc']}
class Binary3RegisterImmedShiftedOpRegsNotPcTester_Case3
    : public Binary3RegisterImmedShiftedOpTesterCase3 {
 public:
  Binary3RegisterImmedShiftedOpRegsNotPcTester_Case3()
    : Binary3RegisterImmedShiftedOpTesterCase3(
      state_.Binary3RegisterImmedShiftedOpRegsNotPc_Pkh_Rule_116_A1_P234_instance_)
  {}
};

// Neutral case:
// inst(22:20)=010 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       rule: 'Ssat16_Rule_184_A1_P364',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=010 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       rule: Ssat16_Rule_184_A1_P364,
//       safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTester_Case4
    : public Unary2RegisterSatImmedShiftedOpTesterCase4 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_Case4()
    : Unary2RegisterSatImmedShiftedOpTesterCase4(
      state_.Unary2RegisterSatImmedShiftedOp_Ssat16_Rule_184_A1_P364_instance_)
  {}
};

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Sxtab_Rule_220_A1_P434',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Sxtab_Rule_220_A1_P434,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case5
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase5 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case5()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase5(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sxtab_Rule_220_A1_P434_instance_)
  {}
};

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterImmedShiftedOpRegsNotPc',
//       constraints: ,
//       rule: 'Sxtb_Rule_223_A1_P440',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Unary2RegisterImmedShiftedOpRegsNotPc,
//       constraints: ,
//       rule: Sxtb_Rule_223_A1_P440,
//       safety: ['RegsNotPc']}
class Unary2RegisterImmedShiftedOpRegsNotPcTester_Case6
    : public Unary2RegisterImmedShiftedOpRegsNotPcTesterCase6 {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTester_Case6()
    : Unary2RegisterImmedShiftedOpRegsNotPcTesterCase6(
      state_.Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb_Rule_223_A1_P440_instance_)
  {}
};

// Neutral case:
// inst(22:20)=011 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       rule: 'Rev_Rule_135_A1_P272',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       rule: Rev_Rule_135_A1_P272,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case7
    : public Unary2RegisterOpNotRmIsPcTesterCase7 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case7()
    : Unary2RegisterOpNotRmIsPcTesterCase7(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev_Rule_135_A1_P272_instance_)
  {}
};

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Sxtah_Rule_222_A1_P438',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Sxtah_Rule_222_A1_P438,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case8
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase8 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case8()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase8(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sxtah_Rule_222_A1_P438_instance_)
  {}
};

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       rule: 'Sxth_Rule_225_A1_P444',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       rule: Sxth_Rule_225_A1_P444,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case9
    : public Unary2RegisterOpNotRmIsPcTesterCase9 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case9()
    : Unary2RegisterOpNotRmIsPcTesterCase9(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Sxth_Rule_225_A1_P444_instance_)
  {}
};

// Neutral case:
// inst(22:20)=011 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       rule: 'Rev16_Rule_136_A1_P274',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       rule: Rev16_Rule_136_A1_P274,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case10
    : public Unary2RegisterOpNotRmIsPcTesterCase10 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case10()
    : Unary2RegisterOpNotRmIsPcTesterCase10(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev16_Rule_136_A1_P274_instance_)
  {}
};

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Uxtab16_Rule_262_A1_P516',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Uxtab16_Rule_262_A1_P516,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case11
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase11 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case11()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase11(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uxtab16_Rule_262_A1_P516_instance_)
  {}
};

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       rule: 'Uxtb16_Rule_264_A1_P522',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       rule: Uxtb16_Rule_264_A1_P522,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case12
    : public Unary2RegisterOpNotRmIsPcTesterCase12 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case12()
    : Unary2RegisterOpNotRmIsPcTesterCase12(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb16_Rule_264_A1_P522_instance_)
  {}
};

// Neutral case:
// inst(22:20)=110 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       rule: 'Usat16_Rule_256_A1_P506',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=110 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       rule: Usat16_Rule_256_A1_P506,
//       safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTester_Case13
    : public Unary2RegisterSatImmedShiftedOpTesterCase13 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_Case13()
    : Unary2RegisterSatImmedShiftedOpTesterCase13(
      state_.Unary2RegisterSatImmedShiftedOp_Usat16_Rule_256_A1_P506_instance_)
  {}
};

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Uxtab_Rule_260_A1_P514',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Uxtab_Rule_260_A1_P514,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case14
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase14 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case14()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase14(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uxtab_Rule_260_A1_P514_instance_)
  {}
};

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       rule: 'Uxtb_Rule_263_A1_P520',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       rule: Uxtb_Rule_263_A1_P520,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case15
    : public Unary2RegisterOpNotRmIsPcTesterCase15 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case15()
    : Unary2RegisterOpNotRmIsPcTesterCase15(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb_Rule_263_A1_P520_instance_)
  {}
};

// Neutral case:
// inst(22:20)=111 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       rule: 'Rbit_Rule_134_A1_P270',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       rule: Rbit_Rule_134_A1_P270,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case16
    : public Unary2RegisterOpNotRmIsPcTesterCase16 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case16()
    : Unary2RegisterOpNotRmIsPcTesterCase16(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Rbit_Rule_134_A1_P270_instance_)
  {}
};

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       rule: 'Uxtah_Rule_262_A1_P518',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       rule: Uxtah_Rule_262_A1_P518,
//       safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case17
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase17 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case17()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase17(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uxtah_Rule_262_A1_P518_instance_)
  {}
};

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       rule: 'Uxth_Rule_265_A1_P524',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       rule: Uxth_Rule_265_A1_P524,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case18
    : public Unary2RegisterOpNotRmIsPcTesterCase18 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case18()
    : Unary2RegisterOpNotRmIsPcTesterCase18(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxth_Rule_265_A1_P524_instance_)
  {}
};

// Neutral case:
// inst(22:20)=111 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       rule: 'Revsh_Rule_137_A1_P276',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       rule: Revsh_Rule_137_A1_P276,
//       safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case19
    : public Unary2RegisterOpNotRmIsPcTesterCase19 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case19()
    : Unary2RegisterOpNotRmIsPcTesterCase19(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Revsh_Rule_137_A1_P276_instance_)
  {}
};

// Neutral case:
// inst(22:20)=01x & inst(7:5)=xx0
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       rule: 'Ssat_Rule_183_A1_P362',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=01x & op2(7:5)=xx0
//    = {baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       rule: Ssat_Rule_183_A1_P362,
//       safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTester_Case20
    : public Unary2RegisterSatImmedShiftedOpTesterCase20 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_Case20()
    : Unary2RegisterSatImmedShiftedOpTesterCase20(
      state_.Unary2RegisterSatImmedShiftedOp_Ssat_Rule_183_A1_P362_instance_)
  {}
};

// Neutral case:
// inst(22:20)=11x & inst(7:5)=xx0
//    = {baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       rule: 'Usat_Rule_255_A1_P504',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=11x & op2(7:5)=xx0
//    = {baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       rule: Usat_Rule_255_A1_P504,
//       safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTester_Case21
    : public Unary2RegisterSatImmedShiftedOpTesterCase21 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_Case21()
    : Unary2RegisterSatImmedShiftedOpTesterCase21(
      state_.Unary2RegisterSatImmedShiftedOp_Usat_Rule_255_A1_P504_instance_)
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
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterImmedShiftedOpRegsNotPc',
//       constraints: ,
//       pattern: 'cccc01101000nnnnddddrr000111mmmm',
//       rule: 'Sxtab16_Rule_221_A1_P436',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterImmedShiftedOpRegsNotPc,
//       constraints: ,
//       pattern: cccc01101000nnnnddddrr000111mmmm,
//       rule: Sxtab16_Rule_221_A1_P436,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpRegsNotPcTester_Case0_TestCase0) {
  Binary3RegisterImmedShiftedOpRegsNotPcTester_Case0 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtab16_Rule_221_A1_P436 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101000nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPc',
//       baseline: 'Unary2RegisterImmedShiftedOpRegsNotPc',
//       constraints: ,
//       pattern: 'cccc011010001111ddddrr000111mmmm',
//       rule: 'Sxtb16_Rule_224_A1_P442',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRdRnNotPc,
//       baseline: Unary2RegisterImmedShiftedOpRegsNotPc,
//       constraints: ,
//       pattern: cccc011010001111ddddrr000111mmmm,
//       rule: Sxtb16_Rule_224_A1_P442,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpRegsNotPcTester_Case1_TestCase1) {
  Unary2RegisterImmedShiftedOpRegsNotPcTester_Case1 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxtb16_Rule_224_A1_P442 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010001111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01101000nnnndddd11111011mmmm',
//       rule: 'Sel_Rule_156_A1_P312',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01101000nnnndddd11111011mmmm,
//       rule: Sel_Rule_156_A1_P312,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case2_TestCase2) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case2 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sel_Rule_156_A1_P312 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101000nnnndddd11111011mmmm");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=xx0
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterImmedShiftedOpRegsNotPc',
//       constraints: ,
//       pattern: 'cccc01101000nnnnddddiiiiit01mmmm',
//       rule: 'Pkh_Rule_116_A1_P234',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=xx0
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterImmedShiftedOpRegsNotPc,
//       constraints: ,
//       pattern: cccc01101000nnnnddddiiiiit01mmmm,
//       rule: Pkh_Rule_116_A1_P234,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpRegsNotPcTester_Case3_TestCase3) {
  Binary3RegisterImmedShiftedOpRegsNotPcTester_Case3 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Pkh_Rule_116_A1_P234 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101000nnnnddddiiiiit01mmmm");
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPc',
//       baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       pattern: 'cccc01101010iiiidddd11110011nnnn',
//       rule: 'Ssat16_Rule_184_A1_P364',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=010 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRdRnNotPc,
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       pattern: cccc01101010iiiidddd11110011nnnn,
//       rule: Ssat16_Rule_184_A1_P364,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_Case4_TestCase4) {
  Unary2RegisterSatImmedShiftedOpTester_Case4 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Ssat16_Rule_184_A1_P364 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101010iiiidddd11110011nnnn");
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01101010nnnnddddrr000111mmmm',
//       rule: 'Sxtab_Rule_220_A1_P434',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01101010nnnnddddrr000111mmmm,
//       rule: Sxtab_Rule_220_A1_P434,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case5_TestCase5) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case5 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtab_Rule_220_A1_P434 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101010nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPc',
//       baseline: 'Unary2RegisterImmedShiftedOpRegsNotPc',
//       constraints: ,
//       pattern: 'cccc011010101111ddddrr000111mmmm',
//       rule: 'Sxtb_Rule_223_A1_P440',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRdRnNotPc,
//       baseline: Unary2RegisterImmedShiftedOpRegsNotPc,
//       constraints: ,
//       pattern: cccc011010101111ddddrr000111mmmm,
//       rule: Sxtb_Rule_223_A1_P440,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpRegsNotPcTester_Case6_TestCase6) {
  Unary2RegisterImmedShiftedOpRegsNotPcTester_Case6 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxtb_Rule_223_A1_P440 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010101111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPc',
//       baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc011010111111dddd11110011mmmm',
//       rule: 'Rev_Rule_135_A1_P272',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRdRnNotPc,
//       baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       pattern: cccc011010111111dddd11110011mmmm,
//       rule: Rev_Rule_135_A1_P272,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case7_TestCase7) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case7 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Rev_Rule_135_A1_P272 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010111111dddd11110011mmmm");
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01101011nnnnddddrr000111mmmm',
//       rule: 'Sxtah_Rule_222_A1_P438',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01101011nnnnddddrr000111mmmm,
//       rule: Sxtah_Rule_222_A1_P438,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case8_TestCase8) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case8 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtah_Rule_222_A1_P438 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101011nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPc',
//       baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc011010111111ddddrr000111mmmm',
//       rule: 'Sxth_Rule_225_A1_P444',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRdRnNotPc,
//       baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       pattern: cccc011010111111ddddrr000111mmmm,
//       rule: Sxth_Rule_225_A1_P444,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case9_TestCase9) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case9 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxth_Rule_225_A1_P444 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010111111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPc',
//       baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc011010111111dddd11111011mmmm',
//       rule: 'Rev16_Rule_136_A1_P274',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRdRnNotPc,
//       baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       pattern: cccc011010111111dddd11111011mmmm,
//       rule: Rev16_Rule_136_A1_P274,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case10_TestCase10) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case10 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Rev16_Rule_136_A1_P274 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010111111dddd11111011mmmm");
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01101100nnnnddddrr000111mmmm',
//       rule: 'Uxtab16_Rule_262_A1_P516',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01101100nnnnddddrr000111mmmm,
//       rule: Uxtab16_Rule_262_A1_P516,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case11_TestCase11) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case11 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtab16_Rule_262_A1_P516 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101100nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPc',
//       baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc011011001111ddddrr000111mmmm',
//       rule: 'Uxtb16_Rule_264_A1_P522',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRdRnNotPc,
//       baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       pattern: cccc011011001111ddddrr000111mmmm,
//       rule: Uxtb16_Rule_264_A1_P522,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case12_TestCase12) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case12 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxtb16_Rule_264_A1_P522 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011001111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPc',
//       baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       pattern: 'cccc01101110iiiidddd11110011nnnn',
//       rule: 'Usat16_Rule_256_A1_P506',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=110 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRdRnNotPc,
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       pattern: cccc01101110iiiidddd11110011nnnn,
//       rule: Usat16_Rule_256_A1_P506,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_Case13_TestCase13) {
  Unary2RegisterSatImmedShiftedOpTester_Case13 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Usat16_Rule_256_A1_P506 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101110iiiidddd11110011nnnn");
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01101110nnnnddddrr000111mmmm',
//       rule: 'Uxtab_Rule_260_A1_P514',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01101110nnnnddddrr000111mmmm,
//       rule: Uxtab_Rule_260_A1_P514,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case14_TestCase14) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case14 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtab_Rule_260_A1_P514 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101110nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPc',
//       baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc011011101111ddddrr000111mmmm',
//       rule: 'Uxtb_Rule_263_A1_P520',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRdRnNotPc,
//       baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       pattern: cccc011011101111ddddrr000111mmmm,
//       rule: Uxtb_Rule_263_A1_P520,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case15_TestCase15) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case15 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxtb_Rule_263_A1_P520 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011101111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPc',
//       baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc011011111111dddd11110011mmmm',
//       rule: 'Rbit_Rule_134_A1_P270',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRdRnNotPc,
//       baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       pattern: cccc011011111111dddd11110011mmmm,
//       rule: Rbit_Rule_134_A1_P270,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case16_TestCase16) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case16 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Rbit_Rule_134_A1_P270 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011111111dddd11110011mmmm");
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRnRdRmNotPc',
//       baseline: 'Binary3RegisterOpAltBNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc01101111nnnnddddrr000111mmmm',
//       rule: 'Uxtah_Rule_262_A1_P518',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRnRdRmNotPc,
//       baseline: Binary3RegisterOpAltBNoCondUpdates,
//       constraints: ,
//       pattern: cccc01101111nnnnddddrr000111mmmm,
//       rule: Uxtah_Rule_262_A1_P518,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case17_TestCase17) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case17 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtah_Rule_262_A1_P518 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101111nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPc',
//       baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc011011111111ddddrr000111mmmm',
//       rule: 'Uxth_Rule_265_A1_P524',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRdRnNotPc,
//       baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       pattern: cccc011011111111ddddrr000111mmmm,
//       rule: Uxth_Rule_265_A1_P524,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case18_TestCase18) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case18 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxth_Rule_265_A1_P524 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011111111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPc',
//       baseline: 'Unary2RegisterOpNotRmIsPcNoCondUpdates',
//       constraints: ,
//       pattern: 'cccc011011111111dddd11111011mmmm',
//       rule: 'Revsh_Rule_137_A1_P276',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {actual: Defs12To15CondsDontCareRdRnNotPc,
//       baseline: Unary2RegisterOpNotRmIsPcNoCondUpdates,
//       constraints: ,
//       pattern: cccc011011111111dddd11111011mmmm,
//       rule: Revsh_Rule_137_A1_P276,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case19_TestCase19) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case19 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Revsh_Rule_137_A1_P276 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011111111dddd11111011mmmm");
}

// Neutral case:
// inst(22:20)=01x & inst(7:5)=xx0
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPc',
//       baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       pattern: 'cccc0110101iiiiiddddiiiiis01nnnn',
//       rule: 'Ssat_Rule_183_A1_P362',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=01x & op2(7:5)=xx0
//    = {actual: Defs12To15CondsDontCareRdRnNotPc,
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       pattern: cccc0110101iiiiiddddiiiiis01nnnn,
//       rule: Ssat_Rule_183_A1_P362,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_Case20_TestCase20) {
  Unary2RegisterSatImmedShiftedOpTester_Case20 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Ssat_Rule_183_A1_P362 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110101iiiiiddddiiiiis01nnnn");
}

// Neutral case:
// inst(22:20)=11x & inst(7:5)=xx0
//    = {actual: 'Defs12To15CondsDontCareRdRnNotPc',
//       baseline: 'Unary2RegisterSatImmedShiftedOp',
//       constraints: ,
//       pattern: 'cccc0110111iiiiiddddiiiiis01nnnn',
//       rule: 'Usat_Rule_255_A1_P504',
//       safety: ['RegsNotPc']}
//
// Representative case:
// op1(22:20)=11x & op2(7:5)=xx0
//    = {actual: Defs12To15CondsDontCareRdRnNotPc,
//       baseline: Unary2RegisterSatImmedShiftedOp,
//       constraints: ,
//       pattern: cccc0110111iiiiiddddiiiiis01nnnn,
//       rule: Usat_Rule_255_A1_P504,
//       safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_Case21_TestCase21) {
  Unary2RegisterSatImmedShiftedOpTester_Case21 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Usat_Rule_255_A1_P504 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110111iiiiiddddiiiiis01nnnn");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
