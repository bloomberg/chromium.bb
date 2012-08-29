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

class LoadStoreRegisterListTesterop_25To20Is0000x0
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterop_25To20Is0000x0(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStoreRegisterListTesterop_25To20Is0000x0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03D00000) != 0x00000000 /* op(25:20) == ~0000x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStoreRegisterListTesterop_25To20Is0000x1
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterop_25To20Is0000x1(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStoreRegisterListTesterop_25To20Is0000x1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03D00000) != 0x00100000 /* op(25:20) == ~0000x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStoreRegisterListTesterop_25To20Is0010x0
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterop_25To20Is0010x0(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStoreRegisterListTesterop_25To20Is0010x0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03D00000) != 0x00800000 /* op(25:20) == ~0010x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStoreRegisterListTesterop_25To20Is0010x1
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterop_25To20Is0010x1(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStoreRegisterListTesterop_25To20Is0010x1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03D00000) != 0x00900000 /* op(25:20) == ~0010x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStoreRegisterListTesterop_25To20Is0100x0
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterop_25To20Is0100x0(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStoreRegisterListTesterop_25To20Is0100x0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03D00000) != 0x01000000 /* op(25:20) == ~0100x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStoreRegisterListTesterop_25To20Is0100x1
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterop_25To20Is0100x1(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStoreRegisterListTesterop_25To20Is0100x1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03D00000) != 0x01100000 /* op(25:20) == ~0100x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStoreRegisterListTesterop_25To20Is0110x0
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterop_25To20Is0110x0(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStoreRegisterListTesterop_25To20Is0110x0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03D00000) != 0x01800000 /* op(25:20) == ~0110x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStoreRegisterListTesterop_25To20Is0110x1
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterop_25To20Is0110x1(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStoreRegisterListTesterop_25To20Is0110x1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03D00000) != 0x01900000 /* op(25:20) == ~0110x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop_25To20Is0xx1x0
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop_25To20Is0xx1x0(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop_25To20Is0xx1x0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02500000) != 0x00400000 /* op(25:20) == ~0xx1x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop_25To20Is0xx1x1_R_15Is0
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop_25To20Is0xx1x1_R_15Is0(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop_25To20Is0xx1x1_R_15Is0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02500000) != 0x00500000 /* op(25:20) == ~0xx1x1 */) return false;
  if ((inst.Bits() & 0x00008000) != 0x00000000 /* R(15:15) == ~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop_25To20Is0xx1x1_R_15Is1
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop_25To20Is0xx1x1_R_15Is1(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop_25To20Is0xx1x1_R_15Is1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02500000) != 0x00500000 /* op(25:20) == ~0xx1x1 */) return false;
  if ((inst.Bits() & 0x00008000) != 0x00008000 /* R(15:15) == ~1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class BranchImmediate24Testerop_25To20Is10xxxx
    : public BranchImmediate24Tester {
 public:
  BranchImmediate24Testerop_25To20Is10xxxx(const NamedClassDecoder& decoder)
    : BranchImmediate24Tester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool BranchImmediate24Testerop_25To20Is10xxxx
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03000000) != 0x02000000 /* op(25:20) == ~10xxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BranchImmediate24Tester::
      PassesParsePreconditions(inst, decoder);
}

class BranchImmediate24Testerop_25To20Is11xxxx
    : public BranchImmediate24Tester {
 public:
  BranchImmediate24Testerop_25To20Is11xxxx(const NamedClassDecoder& decoder)
    : BranchImmediate24Tester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool BranchImmediate24Testerop_25To20Is11xxxx
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03000000) != 0x03000000 /* op(25:20) == ~11xxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BranchImmediate24Tester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* op(25:25) == ~0 */) return false;
  if ((inst.Bits() & 0x01200000) != 0x00200000 /* op1(24:20) == ~0xx1x */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x000000B0 /* op2(7:4) == ~1011 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* op(25:25) == ~0 */) return false;
  if ((inst.Bits() & 0x01200000) != 0x00200000 /* op1(24:20) == ~0xx1x */) return false;
  if ((inst.Bits() & 0x000000D0) != 0x000000D0 /* op2(7:4) == ~11x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10000RegsNotPc
    : public Unary1RegisterImmediateOpTesterRegsNotPc {
 public:
  Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10000RegsNotPc(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10000RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* op(25:25) == ~1 */) return false;
  if ((inst.Bits() & 0x01F00000) != 0x01000000 /* op1(24:20) == ~10000 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10100RegsNotPc
    : public Unary1RegisterImmediateOpTesterRegsNotPc {
 public:
  Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10100RegsNotPc(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10100RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* op(25:25) == ~1 */) return false;
  if ((inst.Bits() & 0x01F00000) != 0x01400000 /* op1(24:20) == ~10100 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterImmediateOpTesterop_24To20Is0000xNotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterop_24To20Is0000xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterImmediateOpTesterop_24To20Is0000xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op(24:20) == ~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterImmediateOpTesterop_24To20Is0001xNotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterop_24To20Is0001xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterImmediateOpTesterop_24To20Is0001xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op(24:20) == ~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterImmediateOpTesterop_24To20Is0010x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS
    : public Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS {
 public:
  Binary2RegisterImmediateOpTesterop_24To20Is0010x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterImmediateOpTesterop_24To20Is0010x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op(24:20) == ~0010x */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS::
      PassesParsePreconditions(inst, decoder);
}

class Unary1RegisterImmediateOpTesterop_24To20Is00100_Rn_19To16Is1111
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTesterop_24To20Is00100_Rn_19To16Is1111(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary1RegisterImmediateOpTesterop_24To20Is00100_Rn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x00400000 /* op(24:20) == ~00100 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop_24To20Is00101_Rn_19To16Is1111
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop_24To20Is00101_Rn_19To16Is1111(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop_24To20Is00101_Rn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x00500000 /* op(24:20) == ~00101 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterImmediateOpTesterop_24To20Is0011xNotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterop_24To20Is0011xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterImmediateOpTesterop_24To20Is0011xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op(24:20) == ~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterImmediateOpTesterop_24To20Is0100x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS
    : public Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS {
 public:
  Binary2RegisterImmediateOpTesterop_24To20Is0100x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterImmediateOpTesterop_24To20Is0100x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op(24:20) == ~0100x */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS::
      PassesParsePreconditions(inst, decoder);
}

class Unary1RegisterImmediateOpTesterop_24To20Is01000_Rn_19To16Is1111
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTesterop_24To20Is01000_Rn_19To16Is1111(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary1RegisterImmediateOpTesterop_24To20Is01000_Rn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x00800000 /* op(24:20) == ~01000 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop_24To20Is01001_Rn_19To16Is1111
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop_24To20Is01001_Rn_19To16Is1111(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop_24To20Is01001_Rn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x00900000 /* op(24:20) == ~01001 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterImmediateOpTesterop_24To20Is0101xNotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterop_24To20Is0101xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterImmediateOpTesterop_24To20Is0101xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op(24:20) == ~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterImmediateOpTesterop_24To20Is0110xNotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterop_24To20Is0110xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterImmediateOpTesterop_24To20Is0110xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op(24:20) == ~0110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterImmediateOpTesterop_24To20Is0111xNotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterop_24To20Is0111xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterImmediateOpTesterop_24To20Is0111xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op(24:20) == ~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class BinaryRegisterImmediateTestTesterop_24To20Is10001
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterop_24To20Is10001(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool BinaryRegisterImmediateTestTesterop_24To20Is10001
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01100000 /* op(24:20) == ~10001 */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

class BinaryRegisterImmediateTestTesterop_24To20Is10011
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterop_24To20Is10011(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool BinaryRegisterImmediateTestTesterop_24To20Is10011
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01300000 /* op(24:20) == ~10011 */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

class BinaryRegisterImmediateTestTesterop_24To20Is10101
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterop_24To20Is10101(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool BinaryRegisterImmediateTestTesterop_24To20Is10101
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01500000 /* op(24:20) == ~10101 */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

class BinaryRegisterImmediateTestTesterop_24To20Is10111
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterop_24To20Is10111(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool BinaryRegisterImmediateTestTesterop_24To20Is10111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01700000 /* op(24:20) == ~10111 */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterImmediateOpTesterop_24To20Is1100xNotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterop_24To20Is1100xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterImmediateOpTesterop_24To20Is1100xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op(24:20) == ~1100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Unary1RegisterImmediateOpTesterop_24To20Is1101xNotRdIsPcAndS
    : public Unary1RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Unary1RegisterImmediateOpTesterop_24To20Is1101xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary1RegisterImmediateOpTesterop_24To20Is1101xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op(24:20) == ~1101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterImmediateOpTesterop_24To20Is1110xNotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterop_24To20Is1110xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterImmediateOpTesterop_24To20Is1110xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op(24:20) == ~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Unary1RegisterImmediateOpTesterop_24To20Is1111xNotRdIsPcAndS
    : public Unary1RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Unary1RegisterImmediateOpTesterop_24To20Is1111xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary1RegisterImmediateOpTesterop_24To20Is1111xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op(24:20) == ~1111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterImmedShiftedOpTesterop1_24To20Is0000xNotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterop1_24To20Is0000xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterImmedShiftedOpTesterop1_24To20Is0000xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op1(24:20) == ~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterImmedShiftedOpTesterop1_24To20Is0001xNotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterop1_24To20Is0001xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterImmedShiftedOpTesterop1_24To20Is0001xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op1(24:20) == ~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterImmedShiftedOpTesterop1_24To20Is0010xNotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterop1_24To20Is0010xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterImmedShiftedOpTesterop1_24To20Is0010xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op1(24:20) == ~0010x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterImmedShiftedOpTesterop1_24To20Is0011xNotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterop1_24To20Is0011xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterImmedShiftedOpTesterop1_24To20Is0011xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op1(24:20) == ~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterImmedShiftedOpTesterop1_24To20Is0100xNotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterop1_24To20Is0100xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterImmedShiftedOpTesterop1_24To20Is0100xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op1(24:20) == ~0100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterImmedShiftedOpTesterop1_24To20Is0101xNotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterop1_24To20Is0101xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterImmedShiftedOpTesterop1_24To20Is0101xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op1(24:20) == ~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterImmedShiftedOpTesterop1_24To20Is0110xNotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterop1_24To20Is0110xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterImmedShiftedOpTesterop1_24To20Is0110xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op1(24:20) == ~0110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterImmedShiftedOpTesterop1_24To20Is0111xNotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterop1_24To20Is0111xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterImmedShiftedOpTesterop1_24To20Is0111xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op1(24:20) == ~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterImmedShiftedTestTesterop1_24To20Is10001
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterop1_24To20Is10001(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterImmedShiftedTestTesterop1_24To20Is10001
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01100000 /* op1(24:20) == ~10001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterImmedShiftedTestTesterop1_24To20Is10011
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterop1_24To20Is10011(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterImmedShiftedTestTesterop1_24To20Is10011
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01300000 /* op1(24:20) == ~10011 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterImmedShiftedTestTesterop1_24To20Is10101
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterop1_24To20Is10101(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterImmedShiftedTestTesterop1_24To20Is10101
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01500000 /* op1(24:20) == ~10101 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterImmedShiftedTestTesterop1_24To20Is10111
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterop1_24To20Is10111(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterImmedShiftedTestTesterop1_24To20Is10111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01700000 /* op1(24:20) == ~10111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterImmedShiftedOpTesterop1_24To20Is1100xNotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterop1_24To20Is1100xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterImmedShiftedOpTesterop1_24To20Is1100xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op1(24:20) == ~1100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterOpTesterop1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is00NotRdIsPcAndS
    : public Unary2RegisterOpTesterNotRdIsPcAndS {
 public:
  Unary2RegisterOpTesterop1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is00NotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Unary2RegisterOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterOpTesterop1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is00NotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20) == ~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7) == ~00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op3(6:5) == ~00 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS
    : public Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20) == ~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7) == 00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op3(6:5) == ~00 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op3_6To5Is01NotRdIsPcAndS
    : public Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op3_6To5Is01NotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op3_6To5Is01NotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20) == ~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op3(6:5) == ~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op3_6To5Is10NotRdIsPcAndS
    : public Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op3_6To5Is10NotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op3_6To5Is10NotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20) == ~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op3(6:5) == ~10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterOpTesterop1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is11NotRdIsPcAndS
    : public Unary2RegisterOpTesterNotRdIsPcAndS {
 public:
  Unary2RegisterOpTesterop1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is11NotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Unary2RegisterOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterOpTesterop1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is11NotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20) == ~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7) == ~00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op3(6:5) == ~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS
    : public Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20) == ~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7) == 00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op3(6:5) == ~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterImmedShiftedOpTesterop1_24To20Is1110xNotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterop1_24To20Is1110xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterImmedShiftedOpTesterop1_24To20Is1110xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20) == ~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterImmedShiftedOpTesterop1_24To20Is1111xNotRdIsPcAndS
    : public Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTesterop1_24To20Is1111xNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterImmedShiftedOpTesterop1_24To20Is1111xNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op1(24:20) == ~1111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterShiftedOpTesterop1_24To20Is0000xRegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterop1_24To20Is0000xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterShiftedOpTesterop1_24To20Is0000xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op1(24:20) == ~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterShiftedOpTesterop1_24To20Is0001xRegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterop1_24To20Is0001xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterShiftedOpTesterop1_24To20Is0001xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op1(24:20) == ~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterShiftedOpTesterop1_24To20Is0010xRegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterop1_24To20Is0010xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterShiftedOpTesterop1_24To20Is0010xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op1(24:20) == ~0010x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterShiftedOpTesterop1_24To20Is0011xRegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterop1_24To20Is0011xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterShiftedOpTesterop1_24To20Is0011xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op1(24:20) == ~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterShiftedOpTesterop1_24To20Is0100xRegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterop1_24To20Is0100xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterShiftedOpTesterop1_24To20Is0100xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op1(24:20) == ~0100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterShiftedOpTesterop1_24To20Is0101xRegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterop1_24To20Is0101xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterShiftedOpTesterop1_24To20Is0101xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op1(24:20) == ~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterShiftedOpTesterop1_24To20Is0110xRegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterop1_24To20Is0110xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterShiftedOpTesterop1_24To20Is0110xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op1(24:20) == ~0110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterShiftedOpTesterop1_24To20Is0111xRegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterop1_24To20Is0111xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterShiftedOpTesterop1_24To20Is0111xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op1(24:20) == ~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterShiftedTestTesterop1_24To20Is10001RegsNotPc
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTesterop1_24To20Is10001RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterShiftedTestTesterop1_24To20Is10001RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01100000 /* op1(24:20) == ~10001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterShiftedTestTesterop1_24To20Is10011RegsNotPc
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTesterop1_24To20Is10011RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterShiftedTestTesterop1_24To20Is10011RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01300000 /* op1(24:20) == ~10011 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterShiftedTestTesterop1_24To20Is10101RegsNotPc
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTesterop1_24To20Is10101RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterShiftedTestTesterop1_24To20Is10101RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01500000 /* op1(24:20) == ~10101 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterShiftedTestTesterop1_24To20Is10111RegsNotPc
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTesterop1_24To20Is10111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterShiftedTestTesterop1_24To20Is10111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01700000 /* op1(24:20) == ~10111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterShiftedOpTesterop1_24To20Is1100xRegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterop1_24To20Is1100xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterShiftedOpTesterop1_24To20Is1100xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op1(24:20) == ~1100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is00RegsNotPc
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is00RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is00RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20) == ~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op2(6:5) == ~00 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is01RegsNotPc
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is01RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is01RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20) == ~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5) == ~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is10RegsNotPc
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is10RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is10RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20) == ~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5) == ~10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is11RegsNotPc
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is11RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is11RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20) == ~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5) == ~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterShiftedOpTesterop1_24To20Is1110xRegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterop1_24To20Is1110xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterShiftedOpTesterop1_24To20Is1110xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20) == ~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Unary3RegisterShiftedOpTesterop1_24To20Is1111xRegsNotPc
    : public Unary3RegisterShiftedOpTesterRegsNotPc {
 public:
  Unary3RegisterShiftedOpTesterop1_24To20Is1111xRegsNotPc(const NamedClassDecoder& decoder)
    : Unary3RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary3RegisterShiftedOpTesterop1_24To20Is1111xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op1(24:20) == ~1111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary3RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class StoreVectorRegisterListTesteropcode_24To20Is01x00
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesteropcode_24To20Is01x00(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool StoreVectorRegisterListTesteropcode_24To20Is01x00
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x00800000 /* opcode(24:20) == ~01x00 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

class StoreVectorRegisterListTesteropcode_24To20Is01x10
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesteropcode_24To20Is01x10(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool StoreVectorRegisterListTesteropcode_24To20Is01x10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x00A00000 /* opcode(24:20) == ~01x10 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

class StoreVectorRegisterTesteropcode_24To20Is1xx00
    : public StoreVectorRegisterTester {
 public:
  StoreVectorRegisterTesteropcode_24To20Is1xx00(const NamedClassDecoder& decoder)
    : StoreVectorRegisterTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool StoreVectorRegisterTesteropcode_24To20Is1xx00
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01300000) != 0x01000000 /* opcode(24:20) == ~1xx00 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterTester::
      PassesParsePreconditions(inst, decoder);
}

class StoreVectorRegisterListTesteropcode_24To20Is10x10_NotRn_19To16Is1101NotRnIsSp
    : public StoreVectorRegisterListTesterNotRnIsSp {
 public:
  StoreVectorRegisterListTesteropcode_24To20Is10x10_NotRn_19To16Is1101NotRnIsSp(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTesterNotRnIsSp(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool StoreVectorRegisterListTesteropcode_24To20Is10x10_NotRn_19To16Is1101NotRnIsSp
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x01200000 /* opcode(24:20) == ~10x10 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16) == 1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTesterNotRnIsSp::
      PassesParsePreconditions(inst, decoder);
}

class StoreVectorRegisterListTesteropcode_24To20Is10x10_Rn_19To16Is1101
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesteropcode_24To20Is10x10_Rn_19To16Is1101(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool StoreVectorRegisterListTesteropcode_24To20Is10x10_Rn_19To16Is1101
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x01200000 /* opcode(24:20) == ~10x10 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16) == ~1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStoreVectorRegisterListTesteropcode_24To20Is01x01
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesteropcode_24To20Is01x01(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStoreVectorRegisterListTesteropcode_24To20Is01x01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x00900000 /* opcode(24:20) == ~01x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_NotRn_19To16Is1101NotRnIsSp
    : public LoadStoreVectorRegisterListTesterNotRnIsSp {
 public:
  LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_NotRn_19To16Is1101NotRnIsSp(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTesterNotRnIsSp(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_NotRn_19To16Is1101NotRnIsSp
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x00B00000 /* opcode(24:20) == ~01x11 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16) == 1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTesterNotRnIsSp::
      PassesParsePreconditions(inst, decoder);
}

class LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_Rn_19To16Is1101
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_Rn_19To16Is1101(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_Rn_19To16Is1101
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x00B00000 /* opcode(24:20) == ~01x11 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16) == ~1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStoreVectorOpTesteropcode_24To20Is1xx01
    : public LoadStoreVectorOpTester {
 public:
  LoadStoreVectorOpTesteropcode_24To20Is1xx01(const NamedClassDecoder& decoder)
    : LoadStoreVectorOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStoreVectorOpTesteropcode_24To20Is1xx01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01300000) != 0x01100000 /* opcode(24:20) == ~1xx01 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorOpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStoreVectorRegisterListTesteropcode_24To20Is10x11
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesteropcode_24To20Is10x11(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStoreVectorRegisterListTesteropcode_24To20Is10x11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x01300000 /* opcode(24:20) == ~10x11 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore3RegisterOpTesterop2_6To5Is01_op1_24To20Isxx0x0
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterop2_6To5Is01_op1_24To20Isxx0x0(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore3RegisterOpTesterop2_6To5Is01_op1_24To20Isxx0x0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5) == ~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00000000 /* op1(24:20) == ~xx0x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore3RegisterOpTesterop2_6To5Is01_op1_24To20Isxx0x1
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterop2_6To5Is01_op1_24To20Isxx0x1(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore3RegisterOpTesterop2_6To5Is01_op1_24To20Isxx0x1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5) == ~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20) == ~xx0x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x0
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x0(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5) == ~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20) == ~xx1x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_NotRn_19To16Is1111
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_NotRn_19To16Is1111(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_NotRn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5) == ~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20) == ~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16Is1111
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16Is1111(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5) == ~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20) == ~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore3RegisterDoubleOpTesterop2_6To5Is10_op1_24To20Isxx0x0
    : public LoadStore3RegisterDoubleOpTester {
 public:
  LoadStore3RegisterDoubleOpTesterop2_6To5Is10_op1_24To20Isxx0x0(const NamedClassDecoder& decoder)
    : LoadStore3RegisterDoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore3RegisterDoubleOpTesterop2_6To5Is10_op1_24To20Isxx0x0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5) == ~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00000000 /* op1(24:20) == ~xx0x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterDoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore3RegisterOpTesterop2_6To5Is10_op1_24To20Isxx0x1
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterop2_6To5Is10_op1_24To20Isxx0x1(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore3RegisterOpTesterop2_6To5Is10_op1_24To20Isxx0x1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5) == ~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20) == ~xx0x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_NotRn_19To16Is1111
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_NotRn_19To16Is1111(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8DoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_NotRn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5) == ~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20) == ~xx1x0 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8DoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16Is1111
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16Is1111(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8DoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5) == ~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20) == ~xx1x0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8DoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_NotRn_19To16Is1111
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_NotRn_19To16Is1111(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_NotRn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5) == ~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20) == ~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16Is1111
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16Is1111(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5) == ~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20) == ~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore3RegisterDoubleOpTesterop2_6To5Is11_op1_24To20Isxx0x0
    : public LoadStore3RegisterDoubleOpTester {
 public:
  LoadStore3RegisterDoubleOpTesterop2_6To5Is11_op1_24To20Isxx0x0(const NamedClassDecoder& decoder)
    : LoadStore3RegisterDoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore3RegisterDoubleOpTesterop2_6To5Is11_op1_24To20Isxx0x0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5) == ~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00000000 /* op1(24:20) == ~xx0x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterDoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore3RegisterOpTesterop2_6To5Is11_op1_24To20Isxx0x1
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterop2_6To5Is11_op1_24To20Isxx0x1(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore3RegisterOpTesterop2_6To5Is11_op1_24To20Isxx0x1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5) == ~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20) == ~xx0x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is11_op1_24To20Isxx1x0
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is11_op1_24To20Isxx1x0(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8DoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is11_op1_24To20Isxx1x0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5) == ~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20) == ~xx1x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8DoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_NotRn_19To16Is1111
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_NotRn_19To16Is1111(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_NotRn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5) == ~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20) == ~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16Is1111
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16Is1111(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5) == ~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20) == ~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc1_23To20Is0x00
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc1_23To20Is0x00(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc1_23To20Is0x00
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00000000 /* opc1(23:20) == ~0x00 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc1_23To20Is0x01
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc1_23To20Is0x01(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc1_23To20Is0x01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00100000 /* opc1(23:20) == ~0x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc1_23To20Is0x10_opc3_7To6Isx1
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc1_23To20Is0x10_opc3_7To6Isx1(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc1_23To20Is0x10_opc3_7To6Isx1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00200000 /* opc1(23:20) == ~0x10 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6) == ~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc1_23To20Is0x10_opc3_7To6Isx0
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc1_23To20Is0x10_opc3_7To6Isx0(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc1_23To20Is0x10_opc3_7To6Isx0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00200000 /* opc1(23:20) == ~0x10 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000000 /* opc3(7:6) == ~x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc1_23To20Is0x11_opc3_7To6Isx0
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc1_23To20Is0x11_opc3_7To6Isx0(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc1_23To20Is0x11_opc3_7To6Isx0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00300000 /* opc1(23:20) == ~0x11 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000000 /* opc3(7:6) == ~x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc1_23To20Is0x11_opc3_7To6Isx1
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc1_23To20Is0x11_opc3_7To6Isx1(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc1_23To20Is0x11_opc3_7To6Isx1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00300000 /* opc1(23:20) == ~0x11 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6) == ~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc1_23To20Is1x00_opc3_7To6Isx0
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc1_23To20Is1x00_opc3_7To6Isx0(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc1_23To20Is1x00_opc3_7To6Isx0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00800000 /* opc1(23:20) == ~1x00 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000000 /* opc3(7:6) == ~x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualOpTesterop1_22To21Is00RegsNotPc
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterop1_22To21Is00RegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualOpTesterop1_22To21Is00RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00000000 /* op1(22:21) == ~00 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualOpTesterop1_22To21Is01_op_5Is0RegsNotPc
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterop1_22To21Is01_op_5Is0RegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualOpTesterop1_22To21Is01_op_5Is0RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op1(22:21) == ~01 */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op(5:5) == ~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltATesterop1_22To21Is01_op_5Is1RegsNotPc
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterop1_22To21Is01_op_5Is1RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltATesterop1_22To21Is01_op_5Is1RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op1(22:21) == ~01 */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000020 /* op(5:5) == ~1 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualResultTesterop1_22To21Is10RegsNotPc
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterop1_22To21Is10RegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualResultTesterop1_22To21Is10RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00400000 /* op1(22:21) == ~10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltATesterop1_22To21Is11RegsNotPc
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterop1_22To21Is11RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltATesterop1_22To21Is11RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op1(22:21) == ~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x0_B_4Is0_Notop1_repeated_24To20Is0x010
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x0_B_4Is0_Notop1_repeated_24To20Is0x010(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x0_B_4Is0_Notop1_repeated_24To20Is0x010
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25:25) == ~1 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00000000 /* op1(24:20) == ~xx0x0 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4:4) == ~0 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00200000 /* op1_repeated(24:20) == 0x010 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterImm5OpTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterA_25Is0_op1_24To20Is0x010
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterA_25Is0_op1_24To20Is0x010(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterA_25Is0_op1_24To20Is0x010
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25:25) == ~0 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00200000 /* op1(24:20) == ~0x010 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterA_25Is1_op1_24To20Is0x010_B_4Is0
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterA_25Is1_op1_24To20Is0x010_B_4Is0(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterA_25Is1_op1_24To20Is0x010_B_4Is0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25:25) == ~1 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00200000 /* op1(24:20) == ~0x010 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4:4) == ~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x011NotRnIsPc
    : public LoadStore2RegisterImm12OpTesterNotRnIsPc {
 public:
  LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x011NotRnIsPc(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTesterNotRnIsPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x011NotRnIsPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25:25) == ~0 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20) == ~xx0x1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00300000 /* op1_repeated(24:20) == 0x011 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTesterNotRnIsPc::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x011
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x011(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x011
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25:25) == ~0 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20) == ~xx0x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00300000 /* op1_repeated(24:20) == 0x011 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x1_B_4Is0_Notop1_repeated_24To20Is0x011
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x1_B_4Is0_Notop1_repeated_24To20Is0x011(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x1_B_4Is0_Notop1_repeated_24To20Is0x011
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25:25) == ~1 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20) == ~xx0x1 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4:4) == ~0 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00300000 /* op1_repeated(24:20) == 0x011 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterImm5OpTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterA_25Is0_op1_24To20Is0x011
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterA_25Is0_op1_24To20Is0x011(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterA_25Is0_op1_24To20Is0x011
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25:25) == ~0 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00300000 /* op1(24:20) == ~0x011 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterA_25Is1_op1_24To20Is0x011_B_4Is0
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterA_25Is1_op1_24To20Is0x011_B_4Is0(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterA_25Is1_op1_24To20Is0x011_B_4Is0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25:25) == ~1 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00300000 /* op1(24:20) == ~0x011 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4:4) == ~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x0_Notop1_repeated_24To20Is0x110
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x0_Notop1_repeated_24To20Is0x110(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x0_Notop1_repeated_24To20Is0x110
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25:25) == ~0 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20) == ~xx1x0 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00600000 /* op1_repeated(24:20) == 0x110 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x0_B_4Is0_Notop1_repeated_24To20Is0x110
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x0_B_4Is0_Notop1_repeated_24To20Is0x110(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x0_B_4Is0_Notop1_repeated_24To20Is0x110
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25:25) == ~1 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20) == ~xx1x0 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4:4) == ~0 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00600000 /* op1_repeated(24:20) == 0x110 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterImm5OpTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterA_25Is0_op1_24To20Is0x110
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterA_25Is0_op1_24To20Is0x110(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterA_25Is0_op1_24To20Is0x110
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25:25) == ~0 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00600000 /* op1(24:20) == ~0x110 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterA_25Is1_op1_24To20Is0x110_B_4Is0
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterA_25Is1_op1_24To20Is0x110_B_4Is0(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterA_25Is1_op1_24To20Is0x110_B_4Is0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25:25) == ~1 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00600000 /* op1(24:20) == ~0x110 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4:4) == ~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x111NotRnIsPc
    : public LoadStore2RegisterImm12OpTesterNotRnIsPc {
 public:
  LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x111NotRnIsPc(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTesterNotRnIsPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x111NotRnIsPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25:25) == ~0 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20) == ~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16) == 1111 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00700000 /* op1_repeated(24:20) == 0x111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTesterNotRnIsPc::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x111
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x111(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25:25) == ~0 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20) == ~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16) == ~1111 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00700000 /* op1_repeated(24:20) == 0x111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x1_B_4Is0_Notop1_repeated_24To20Is0x111
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x1_B_4Is0_Notop1_repeated_24To20Is0x111(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x1_B_4Is0_Notop1_repeated_24To20Is0x111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25:25) == ~1 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20) == ~xx1x1 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4:4) == ~0 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00700000 /* op1_repeated(24:20) == 0x111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterImm5OpTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterA_25Is0_op1_24To20Is0x111
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterA_25Is0_op1_24To20Is0x111(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterA_25Is0_op1_24To20Is0x111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25:25) == ~0 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00700000 /* op1(24:20) == ~0x111 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterA_25Is1_op1_24To20Is0x111_B_4Is0
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterA_25Is1_op1_24To20Is0x111_B_4Is0(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterA_25Is1_op1_24To20Is0x111_B_4Is0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25:25) == ~1 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00700000 /* op1(24:20) == ~0x111 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4:4) == ~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm12OpTesterFlags_24To21Is1001_Rn_19To16Is1101_Imm12_11To0Is000000000100
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterFlags_24To21Is1001_Rn_19To16Is1101_Imm12_11To0Is000000000100(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm12OpTesterFlags_24To21Is1001_Rn_19To16Is1101_Imm12_11To0Is000000000100
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01200000 /* Flags(24:21) == ~1001 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16) == ~1101 */) return false;
  if ((inst.Bits() & 0x00000FFF) != 0x00000004 /* Imm12(11:0) == ~000000000100 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadStore2RegisterImm12OpTesterNotcccc010100101101tttt000000000100
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterNotcccc010100101101tttt000000000100(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadStore2RegisterImm12OpTesterNotcccc010100101101tttt000000000100
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check pattern restrictions of row.
  if ((inst.Bits() & 0x0FFF0FFF) == 0x052D0004 /* constraint(31:0) == xxxx010100101101xxxx000000000100 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltATesterop1_24To20Is11000_op2_7To5Is000_Rd_15To12Is1111RegsNotPc
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterop1_24To20Is11000_op2_7To5Is000_Rd_15To12Is1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltATesterop1_24To20Is11000_op2_7To5Is000_Rd_15To12Is1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01800000 /* op1(24:20) == ~11000 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5) == ~000 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualOpTesterop1_24To20Is11000_op2_7To5Is000_NotRd_15To12Is1111RegsNotPc
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterop1_24To20Is11000_op2_7To5Is000_NotRd_15To12Is1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualOpTesterop1_24To20Is11000_op2_7To5Is000_NotRd_15To12Is1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01800000 /* op1(24:20) == ~11000 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5) == ~000 */) return false;
  if ((inst.Bits() & 0x0000F000) == 0x0000F000 /* Rd(15:12) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterBitRangeNotRnIsPcTesterop1_24To20Is1101x_op2_7To5Isx10
    : public Binary2RegisterBitRangeNotRnIsPcTester {
 public:
  Binary2RegisterBitRangeNotRnIsPcTesterop1_24To20Is1101x_op2_7To5Isx10(const NamedClassDecoder& decoder)
    : Binary2RegisterBitRangeNotRnIsPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterBitRangeNotRnIsPcTesterop1_24To20Is1101x_op2_7To5Isx10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20) == ~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(7:5) == ~x10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterBitRangeNotRnIsPcTester::
      PassesParsePreconditions(inst, decoder);
}

class Unary1RegisterBitRangeTesterop1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111
    : public Unary1RegisterBitRangeTester {
 public:
  Unary1RegisterBitRangeTesterop1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111(const NamedClassDecoder& decoder)
    : Unary1RegisterBitRangeTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary1RegisterBitRangeTesterop1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20) == ~1110x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op2(7:5) == ~x00 */) return false;
  if ((inst.Bits() & 0x0000000F) != 0x0000000F /* Rn(3:0) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterBitRangeTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterBitRangeTesterop1_24To20Is1110x_op2_7To5Isx00_NotRn_3To0Is1111
    : public Binary2RegisterBitRangeTester {
 public:
  Binary2RegisterBitRangeTesterop1_24To20Is1110x_op2_7To5Isx00_NotRn_3To0Is1111(const NamedClassDecoder& decoder)
    : Binary2RegisterBitRangeTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterBitRangeTesterop1_24To20Is1110x_op2_7To5Isx00_NotRn_3To0Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20) == ~1110x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op2(7:5) == ~x00 */) return false;
  if ((inst.Bits() & 0x0000000F) == 0x0000000F /* Rn(3:0) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterBitRangeTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary2RegisterBitRangeNotRnIsPcTesterop1_24To20Is1111x_op2_7To5Isx10
    : public Binary2RegisterBitRangeNotRnIsPcTester {
 public:
  Binary2RegisterBitRangeNotRnIsPcTesterop1_24To20Is1111x_op2_7To5Isx10(const NamedClassDecoder& decoder)
    : Binary2RegisterBitRangeNotRnIsPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary2RegisterBitRangeNotRnIsPcTesterop1_24To20Is1111x_op2_7To5Isx10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op1(24:20) == ~1111x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(7:5) == ~x10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterBitRangeNotRnIsPcTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop2_6To4Is000_B_9Is1_op_22To21Isx0
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop2_6To4Is000_B_9Is1_op_22To21Isx0(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop2_6To4Is000_B_9Is1_op_22To21Isx0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4) == ~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000200 /* B(9:9) == ~1 */) return false;
  if ((inst.Bits() & 0x00200000) != 0x00000000 /* op(22:21) == ~x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop2_6To4Is000_B_9Is1_op_22To21Isx1
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop2_6To4Is000_B_9Is1_op_22To21Isx1(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop2_6To4Is000_B_9Is1_op_22To21Isx1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4) == ~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000200 /* B(9:9) == ~1 */) return false;
  if ((inst.Bits() & 0x00200000) != 0x00200000 /* op(22:21) == ~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class Unary1RegisterSetTesterop2_6To4Is000_B_9Is0_op_22To21Isx0
    : public Unary1RegisterSetTester {
 public:
  Unary1RegisterSetTesterop2_6To4Is000_B_9Is0_op_22To21Isx0(const NamedClassDecoder& decoder)
    : Unary1RegisterSetTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary1RegisterSetTesterop2_6To4Is000_B_9Is0_op_22To21Isx0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4) == ~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9:9) == ~0 */) return false;
  if ((inst.Bits() & 0x00200000) != 0x00000000 /* op(22:21) == ~x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterSetTester::
      PassesParsePreconditions(inst, decoder);
}

class Unary1RegisterUseTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx00
    : public Unary1RegisterUseTester {
 public:
  Unary1RegisterUseTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx00(const NamedClassDecoder& decoder)
    : Unary1RegisterUseTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary1RegisterUseTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx00
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4) == ~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9:9) == ~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21) == ~01 */) return false;
  if ((inst.Bits() & 0x00030000) != 0x00000000 /* op1(19:16) == ~xx00 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterUseTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4) == ~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9:9) == ~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21) == ~01 */) return false;
  if ((inst.Bits() & 0x00030000) != 0x00010000 /* op1(19:16) == ~xx01 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4) == ~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9:9) == ~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21) == ~01 */) return false;
  if ((inst.Bits() & 0x00020000) != 0x00020000 /* op1(19:16) == ~xx1x */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop2_6To4Is000_B_9Is0_op_22To21Is11
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop2_6To4Is000_B_9Is0_op_22To21Is11(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop2_6To4Is000_B_9Is0_op_22To21Is11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4) == ~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9:9) == ~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21) == ~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class BranchToRegisterTesterop2_6To4Is001_op_22To21Is01
    : public BranchToRegisterTester {
 public:
  BranchToRegisterTesterop2_6To4Is001_op_22To21Is01(const NamedClassDecoder& decoder)
    : BranchToRegisterTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool BranchToRegisterTesterop2_6To4Is001_op_22To21Is01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000010 /* op2(6:4) == ~001 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21) == ~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return BranchToRegisterTester::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterOpNotRmIsPcTesterop2_6To4Is001_op_22To21Is11
    : public Unary2RegisterOpNotRmIsPcTester {
 public:
  Unary2RegisterOpNotRmIsPcTesterop2_6To4Is001_op_22To21Is11(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterOpNotRmIsPcTesterop2_6To4Is001_op_22To21Is11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000010 /* op2(6:4) == ~001 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21) == ~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop2_6To4Is010_op_22To21Is01
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop2_6To4Is010_op_22To21Is01(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop2_6To4Is010_op_22To21Is01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000020 /* op2(6:4) == ~010 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21) == ~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class BranchToRegisterTesterop2_6To4Is011_op_22To21Is01RegsNotPc
    : public BranchToRegisterTesterRegsNotPc {
 public:
  BranchToRegisterTesterop2_6To4Is011_op_22To21Is01RegsNotPc(const NamedClassDecoder& decoder)
    : BranchToRegisterTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool BranchToRegisterTesterop2_6To4Is011_op_22To21Is01RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000030 /* op2(6:4) == ~011 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21) == ~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return BranchToRegisterTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop2_6To4Is110_op_22To21Is11
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop2_6To4Is110_op_22To21Is11(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop2_6To4Is110_op_22To21Is11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000060 /* op2(6:4) == ~110 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21) == ~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class Immediate16UseTesterop2_6To4Is111_op_22To21Is01
    : public Immediate16UseTester {
 public:
  Immediate16UseTesterop2_6To4Is111_op_22To21Is01(const NamedClassDecoder& decoder)
    : Immediate16UseTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Immediate16UseTesterop2_6To4Is111_op_22To21Is01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000070 /* op2(6:4) == ~111 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21) == ~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return Immediate16UseTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop2_6To4Is111_op_22To21Is10
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop2_6To4Is111_op_22To21Is10(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop2_6To4Is111_op_22To21Is10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000070 /* op2(6:4) == ~111 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00400000 /* op(22:21) == ~10 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop2_6To4Is111_op_22To21Is11
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop2_6To4Is111_op_22To21Is11(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop2_6To4Is111_op_22To21Is11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000070 /* op2(6:4) == ~111 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21) == ~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class CondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000000
    : public CondNopTester {
 public:
  CondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000000(const NamedClassDecoder& decoder)
    : CondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000000
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22:22) == ~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16) == ~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000000 /* op2(7:0) == ~00000000 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class CondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000001
    : public CondNopTester {
 public:
  CondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000001(const NamedClassDecoder& decoder)
    : CondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000001
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22:22) == ~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16) == ~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000001 /* op2(7:0) == ~00000001 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000010
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000010(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000010
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22:22) == ~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16) == ~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000002 /* op2(7:0) == ~00000010 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000011
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000011(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000011
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22:22) == ~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16) == ~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000003 /* op2(7:0) == ~00000011 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000100
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000100(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000100
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22:22) == ~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16) == ~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000004 /* op2(7:0) == ~00000100 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class CondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx
    : public CondNopTester {
 public:
  CondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx(const NamedClassDecoder& decoder)
    : CondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22:22) == ~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16) == ~0000 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x000000F0 /* op2(7:0) == ~1111xxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return CondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class MoveImmediate12ToApsrTesterop_22Is0_op1_19To16Is0100
    : public MoveImmediate12ToApsrTester {
 public:
  MoveImmediate12ToApsrTesterop_22Is0_op1_19To16Is0100(const NamedClassDecoder& decoder)
    : MoveImmediate12ToApsrTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool MoveImmediate12ToApsrTesterop_22Is0_op1_19To16Is0100
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22:22) == ~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00040000 /* op1(19:16) == ~0100 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveImmediate12ToApsrTester::
      PassesParsePreconditions(inst, decoder);
}

class MoveImmediate12ToApsrTesterop_22Is0_op1_19To16Is1x00
    : public MoveImmediate12ToApsrTester {
 public:
  MoveImmediate12ToApsrTesterop_22Is0_op1_19To16Is1x00(const NamedClassDecoder& decoder)
    : MoveImmediate12ToApsrTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool MoveImmediate12ToApsrTesterop_22Is0_op1_19To16Is1x00
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22:22) == ~0 */) return false;
  if ((inst.Bits() & 0x000B0000) != 0x00080000 /* op1(19:16) == ~1x00 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveImmediate12ToApsrTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop_22Is0_op1_19To16Isxx01
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop_22Is0_op1_19To16Isxx01(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop_22Is0_op1_19To16Isxx01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22:22) == ~0 */) return false;
  if ((inst.Bits() & 0x00030000) != 0x00010000 /* op1(19:16) == ~xx01 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop_22Is0_op1_19To16Isxx1x
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop_22Is0_op1_19To16Isxx1x(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop_22Is0_op1_19To16Isxx1x
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22:22) == ~0 */) return false;
  if ((inst.Bits() & 0x00020000) != 0x00020000 /* op1(19:16) == ~xx1x */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop_22Is1
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop_22Is1(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop_22Is1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00400000 /* op(22:22) == ~1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltATesterop_23To20Is000xRegsNotPc
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterop_23To20Is000xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltATesterop_23To20Is000xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00E00000) != 0x00000000 /* op(23:20) == ~000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualOpTesterop_23To20Is001xRegsNotPc
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterop_23To20Is001xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualOpTesterop_23To20Is001xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00E00000) != 0x00200000 /* op(23:20) == ~001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualResultTesterop_23To20Is0100RegsNotPc
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterop_23To20Is0100RegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualResultTesterop_23To20Is0100RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00400000 /* op(23:20) == ~0100 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualOpTesterop_23To20Is0110RegsNotPc
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterop_23To20Is0110RegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualOpTesterop_23To20Is0110RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00600000 /* op(23:20) == ~0110 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualResultTesterop_23To20Is100xRegsNotPc
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterop_23To20Is100xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualResultTesterop_23To20Is100xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00E00000) != 0x00800000 /* op(23:20) == ~100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualResultTesterop_23To20Is101xRegsNotPc
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterop_23To20Is101xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualResultTesterop_23To20Is101xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00E00000) != 0x00A00000 /* op(23:20) == ~101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualResultTesterop_23To20Is110xRegsNotPc
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterop_23To20Is110xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualResultTesterop_23To20Is110xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00E00000) != 0x00C00000 /* op(23:20) == ~110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualResultTesterop_23To20Is111xRegsNotPc
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterop_23To20Is111xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualResultTesterop_23To20Is111xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00E00000) != 0x00E00000 /* op(23:20) == ~111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc3_7To6Isx0
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc3_7To6Isx0(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc3_7To6Isx0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000040) != 0x00000000 /* opc3(7:6) == ~x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc2_19To16Is0000_opc3_7To6Is01
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc2_19To16Is0000_opc3_7To6Is01(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc2_19To16Is0000_opc3_7To6Is01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* opc2(19:16) == ~0000 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000040 /* opc3(7:6) == ~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc2_19To16Is0001_opc3_7To6Is01
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc2_19To16Is0001_opc3_7To6Is01(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc2_19To16Is0001_opc3_7To6Is01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x000F0000) != 0x00010000 /* opc2(19:16) == ~0001 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000040 /* opc3(7:6) == ~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc2_19To16Is0001_opc3_7To6Is11
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc2_19To16Is0001_opc3_7To6Is11(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc2_19To16Is0001_opc3_7To6Is11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x000F0000) != 0x00010000 /* opc2(19:16) == ~0001 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x000000C0 /* opc3(7:6) == ~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc2_19To16Is001x_opc3_7To6Isx1
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc2_19To16Is001x_opc3_7To6Isx1(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc2_19To16Is001x_opc3_7To6Isx1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x000E0000) != 0x00020000 /* opc2(19:16) == ~001x */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6) == ~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc2_19To16Is0100_opc3_7To6Isx1
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc2_19To16Is0100_opc3_7To6Isx1(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc2_19To16Is0100_opc3_7To6Isx1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x000F0000) != 0x00040000 /* opc2(19:16) == ~0100 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6) == ~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc2_19To16Is0101_opc3_7To6Isx1
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc2_19To16Is0101_opc3_7To6Isx1(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc2_19To16Is0101_opc3_7To6Isx1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x000F0000) != 0x00050000 /* opc2(19:16) == ~0101 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6) == ~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc2_19To16Is0111_opc3_7To6Is11
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc2_19To16Is0111_opc3_7To6Is11(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc2_19To16Is0111_opc3_7To6Is11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x000F0000) != 0x00070000 /* opc2(19:16) == ~0111 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x000000C0 /* opc3(7:6) == ~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc2_19To16Is1000_opc3_7To6Isx1
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc2_19To16Is1000_opc3_7To6Isx1(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc2_19To16Is1000_opc3_7To6Isx1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x000F0000) != 0x00080000 /* opc2(19:16) == ~1000 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6) == ~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc2_19To16Is101x_opc3_7To6Isx1
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc2_19To16Is101x_opc3_7To6Isx1(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc2_19To16Is101x_opc3_7To6Isx1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x000E0000) != 0x000A0000 /* opc2(19:16) == ~101x */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6) == ~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc2_19To16Is110x_opc3_7To6Isx1
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc2_19To16Is110x_opc3_7To6Isx1(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc2_19To16Is110x_opc3_7To6Isx1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x000E0000) != 0x000C0000 /* opc2(19:16) == ~110x */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6) == ~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class CondVfpOpTesteropc2_19To16Is111x_opc3_7To6Isx1
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc2_19To16Is111x_opc3_7To6Isx1(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool CondVfpOpTesteropc2_19To16Is111x_opc3_7To6Isx1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x000E0000) != 0x000E0000 /* opc2(19:16) == ~111x */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6) == ~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Isxx0RegsNotPc
    : public Binary3RegisterImmedShiftedOpTesterRegsNotPc {
 public:
  Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Isxx0RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Isxx0RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20) == ~000 */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op2(7:5) == ~xx0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is01x_op2_7To5Isxx0
    : public Unary2RegisterSatImmedShiftedOpTester {
 public:
  Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is01x_op2_7To5Isxx0(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is01x_op2_7To5Isxx0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op1(22:20) == ~01x */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op2(7:5) == ~xx0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is11x_op2_7To5Isxx0
    : public Unary2RegisterSatImmedShiftedOpTester {
 public:
  Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is11x_op2_7To5Isxx0(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is11x_op2_7To5Isxx0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op1(22:20) == ~11x */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op2(7:5) == ~xx0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc
    : public Binary3RegisterImmedShiftedOpTesterNotRnIsPcAndRegsNotPc {
 public:
  Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRnIsPcAndRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20) == ~000 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5) == ~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRnIsPcAndRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is000_op2_7To5Is011_A_19To16Is1111
    : public Unary2RegisterImmedShiftedOpRegsNotPcTester {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is000_op2_7To5Is011_A_19To16Is1111(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpRegsNotPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is000_op2_7To5Is011_A_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20) == ~000 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5) == ~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpRegsNotPcTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop1_22To20Is000_op2_7To5Is101RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop1_22To20Is000_op2_7To5Is101RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop1_22To20Is000_op2_7To5Is101RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20) == ~000 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000A0 /* op2(7:5) == ~101 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is010_op2_7To5Is001
    : public Unary2RegisterSatImmedShiftedOpTester {
 public:
  Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is010_op2_7To5Is001(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is010_op2_7To5Is001
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00200000 /* op1(22:20) == ~010 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5) == ~001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop1_22To20Is010_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc
    : public Binary3RegisterOpAltBTesterNotRnIsPcAndRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop1_22To20Is010_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterNotRnIsPcAndRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop1_22To20Is010_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00200000 /* op1(22:20) == ~010 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5) == ~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterNotRnIsPcAndRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is010_op2_7To5Is011_A_19To16Is1111
    : public Unary2RegisterImmedShiftedOpRegsNotPcTester {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is010_op2_7To5Is011_A_19To16Is1111(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpRegsNotPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is010_op2_7To5Is011_A_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00200000 /* op1(22:20) == ~010 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5) == ~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpRegsNotPcTester::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is001
    : public Unary2RegisterOpNotRmIsPcTester {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is001(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is001
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00300000 /* op1(22:20) == ~011 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5) == ~001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is011_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is011_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is011_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00300000 /* op1(22:20) == ~011 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5) == ~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is011_A_19To16Is1111
    : public Unary2RegisterOpNotRmIsPcTester {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is011_A_19To16Is1111(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is011_A_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00300000 /* op1(22:20) == ~011 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5) == ~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTester::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is101
    : public Unary2RegisterOpNotRmIsPcTester {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is101(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is101
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00300000 /* op1(22:20) == ~011 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000A0 /* op2(7:5) == ~101 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is100_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is100_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is100_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00400000 /* op1(22:20) == ~100 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5) == ~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is100_op2_7To5Is011_A_19To16Is1111
    : public Unary2RegisterOpNotRmIsPcTester {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is100_op2_7To5Is011_A_19To16Is1111(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is100_op2_7To5Is011_A_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00400000 /* op1(22:20) == ~100 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5) == ~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTester::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is110_op2_7To5Is001
    : public Unary2RegisterSatImmedShiftedOpTester {
 public:
  Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is110_op2_7To5Is001(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is110_op2_7To5Is001
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00600000 /* op1(22:20) == ~110 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5) == ~001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is110_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is110_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is110_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00600000 /* op1(22:20) == ~110 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5) == ~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is110_op2_7To5Is011_A_19To16Is1111
    : public Unary2RegisterOpNotRmIsPcTester {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is110_op2_7To5Is011_A_19To16Is1111(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is110_op2_7To5Is011_A_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00600000 /* op1(22:20) == ~110 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5) == ~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTester::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is001
    : public Unary2RegisterOpNotRmIsPcTester {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is001(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is001
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00700000 /* op1(22:20) == ~111 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5) == ~001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is111_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is111_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is111_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00700000 /* op1(22:20) == ~111 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5) == ~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is011_A_19To16Is1111
    : public Unary2RegisterOpNotRmIsPcTester {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is011_A_19To16Is1111(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is011_A_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00700000 /* op1(22:20) == ~111 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5) == ~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTester::
      PassesParsePreconditions(inst, decoder);
}

class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is101
    : public Unary2RegisterOpNotRmIsPcTester {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is101(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is101
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00700000 /* op1(22:20) == ~111 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000A0 /* op2(7:5) == ~101 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTester::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is000RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is000RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is000RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20) == ~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5) == ~000 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is001RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is001RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is001RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20) == ~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5) == ~001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is010RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is010RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is010RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20) == ~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000040 /* op2(7:5) == ~010 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is011RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is011RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is011RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20) == ~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5) == ~011 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is100RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is100RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is100RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20) == ~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000080 /* op2(7:5) == ~100 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is111RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20) == ~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000E0 /* op2(7:5) == ~111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is000RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is000RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is000RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20) == ~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5) == ~000 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is001RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is001RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is001RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20) == ~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5) == ~001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is010RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is010RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is010RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20) == ~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000040 /* op2(7:5) == ~010 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is011RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is011RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is011RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20) == ~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5) == ~011 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is100RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is100RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is100RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20) == ~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000080 /* op2(7:5) == ~100 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is111RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20) == ~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000E0 /* op2(7:5) == ~111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is000RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is000RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is000RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20) == ~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5) == ~000 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is001RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is001RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is001RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20) == ~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5) == ~001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is010RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is010RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is010RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20) == ~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000040 /* op2(7:5) == ~010 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is011RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is011RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is011RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20) == ~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5) == ~011 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is100RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is100RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is100RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20) == ~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000080 /* op2(7:5) == ~100 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is111RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20) == ~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000E0 /* op2(7:5) == ~111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop_22To21Is00RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop_22To21Is00RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop_22To21Is00RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00000000 /* op(22:21) == ~00 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop_22To21Is01RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop_22To21Is01RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop_22To21Is01RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21) == ~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop_22To21Is10RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop_22To21Is10RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop_22To21Is10RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00400000 /* op(22:21) == ~10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltBTesterop_22To21Is11RegsNotPc
    : public Binary3RegisterOpAltBTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBTesterop_22To21Is11RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltBTesterop_22To21Is11RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21) == ~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc
    : public Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc {
 public:
  Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20) == ~000 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000000 /* op2(7:5) == ~00x */) return false;
  if ((inst.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltATesterop1_22To20Is000_op2_7To5Is00x_A_15To12Is1111RegsNotPc
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterop1_22To20Is000_op2_7To5Is00x_A_15To12Is1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltATesterop1_22To20Is000_op2_7To5Is00x_A_15To12Is1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20) == ~000 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000000 /* op2(7:5) == ~00x */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is01x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc
    : public Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc {
 public:
  Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is01x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is01x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20) == ~000 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000040 /* op2(7:5) == ~01x */) return false;
  if ((inst.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltATesterop1_22To20Is000_op2_7To5Is01x_A_15To12Is1111RegsNotPc
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterop1_22To20Is000_op2_7To5Is01x_A_15To12Is1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltATesterop1_22To20Is000_op2_7To5Is01x_A_15To12Is1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20) == ~000 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000040 /* op2(7:5) == ~01x */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualResultTesterop1_22To20Is100_op2_7To5Is00xRegsNotPc
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterop1_22To20Is100_op2_7To5Is00xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualResultTesterop1_22To20Is100_op2_7To5Is00xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00400000 /* op1(22:20) == ~100 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000000 /* op2(7:5) == ~00x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualResultTesterop1_22To20Is100_op2_7To5Is01xRegsNotPc
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterop1_22To20Is100_op2_7To5Is01xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualResultTesterop1_22To20Is100_op2_7To5Is01xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00400000 /* op1(22:20) == ~100 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000040 /* op2(7:5) == ~01x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc
    : public Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc {
 public:
  Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00500000 /* op1(22:20) == ~101 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000000 /* op2(7:5) == ~00x */) return false;
  if ((inst.Bits() & 0x0000F000) == 0x0000F000 /* A(15:12) == 1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary3RegisterOpAltATesterop1_22To20Is101_op2_7To5Is00x_A_15To12Is1111RegsNotPc
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterop1_22To20Is101_op2_7To5Is00x_A_15To12Is1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary3RegisterOpAltATesterop1_22To20Is101_op2_7To5Is00x_A_15To12Is1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00500000 /* op1(22:20) == ~101 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000000 /* op2(7:5) == ~00x */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is11xRegsNotPc
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is11xRegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is11xRegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00500000 /* op1(22:20) == ~101 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x000000C0 /* op2(7:5) == ~11x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeCondNopTesterop_23To20Is0x00
    : public UnsafeCondNopTester {
 public:
  UnsafeCondNopTesterop_23To20Is0x00(const NamedClassDecoder& decoder)
    : UnsafeCondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeCondNopTesterop_23To20Is0x00
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00000000 /* op(23:20) == ~0x00 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondNopTester::
      PassesParsePreconditions(inst, decoder);
}

class StoreExclusive3RegisterOpTesterop_23To20Is1000
    : public StoreExclusive3RegisterOpTester {
 public:
  StoreExclusive3RegisterOpTesterop_23To20Is1000(const NamedClassDecoder& decoder)
    : StoreExclusive3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool StoreExclusive3RegisterOpTesterop_23To20Is1000
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00800000 /* op(23:20) == ~1000 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreExclusive3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadExclusive2RegisterOpTesterop_23To20Is1001
    : public LoadExclusive2RegisterOpTester {
 public:
  LoadExclusive2RegisterOpTesterop_23To20Is1001(const NamedClassDecoder& decoder)
    : LoadExclusive2RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadExclusive2RegisterOpTesterop_23To20Is1001
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00900000 /* op(23:20) == ~1001 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadExclusive2RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

class StoreExclusive3RegisterDoubleOpTesterop_23To20Is1010
    : public StoreExclusive3RegisterDoubleOpTester {
 public:
  StoreExclusive3RegisterDoubleOpTesterop_23To20Is1010(const NamedClassDecoder& decoder)
    : StoreExclusive3RegisterDoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool StoreExclusive3RegisterDoubleOpTesterop_23To20Is1010
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00A00000 /* op(23:20) == ~1010 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreExclusive3RegisterDoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadExclusive2RegisterDoubleOpTesterop_23To20Is1011
    : public LoadExclusive2RegisterDoubleOpTester {
 public:
  LoadExclusive2RegisterDoubleOpTesterop_23To20Is1011(const NamedClassDecoder& decoder)
    : LoadExclusive2RegisterDoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadExclusive2RegisterDoubleOpTesterop_23To20Is1011
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00B00000 /* op(23:20) == ~1011 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadExclusive2RegisterDoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

class StoreExclusive3RegisterOpTesterop_23To20Is1100
    : public StoreExclusive3RegisterOpTester {
 public:
  StoreExclusive3RegisterOpTesterop_23To20Is1100(const NamedClassDecoder& decoder)
    : StoreExclusive3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool StoreExclusive3RegisterOpTesterop_23To20Is1100
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00C00000 /* op(23:20) == ~1100 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreExclusive3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadExclusive2RegisterOpTesterop_23To20Is1101
    : public LoadExclusive2RegisterOpTester {
 public:
  LoadExclusive2RegisterOpTesterop_23To20Is1101(const NamedClassDecoder& decoder)
    : LoadExclusive2RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadExclusive2RegisterOpTesterop_23To20Is1101
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00D00000 /* op(23:20) == ~1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadExclusive2RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

class StoreExclusive3RegisterOpTesterop_23To20Is1110
    : public StoreExclusive3RegisterOpTester {
 public:
  StoreExclusive3RegisterOpTesterop_23To20Is1110(const NamedClassDecoder& decoder)
    : StoreExclusive3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool StoreExclusive3RegisterOpTesterop_23To20Is1110
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00E00000 /* op(23:20) == ~1110 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreExclusive3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

class LoadExclusive2RegisterOpTesterop_23To20Is1111
    : public LoadExclusive2RegisterOpTester {
 public:
  LoadExclusive2RegisterOpTesterop_23To20Is1111(const NamedClassDecoder& decoder)
    : LoadExclusive2RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool LoadExclusive2RegisterOpTesterop_23To20Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00F00000 /* op(23:20) == ~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadExclusive2RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

class MoveVfpRegisterOpTesterL_20Is0_C_8Is0_A_23To21Is000
    : public MoveVfpRegisterOpTester {
 public:
  MoveVfpRegisterOpTesterL_20Is0_C_8Is0_A_23To21Is000(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool MoveVfpRegisterOpTesterL_20Is0_C_8Is0_A_23To21Is000
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00100000) != 0x00000000 /* L(20:20) == ~0 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000000 /* C(8:8) == ~0 */) return false;
  if ((inst.Bits() & 0x00E00000) != 0x00000000 /* A(23:21) == ~000 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

class VfpUsesRegOpTesterL_20Is0_C_8Is0_A_23To21Is111
    : public VfpUsesRegOpTester {
 public:
  VfpUsesRegOpTesterL_20Is0_C_8Is0_A_23To21Is111(const NamedClassDecoder& decoder)
    : VfpUsesRegOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool VfpUsesRegOpTesterL_20Is0_C_8Is0_A_23To21Is111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00100000) != 0x00000000 /* L(20:20) == ~0 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000000 /* C(8:8) == ~0 */) return false;
  if ((inst.Bits() & 0x00E00000) != 0x00E00000 /* A(23:21) == ~111 */) return false;

  // Check other preconditions defined for the base decoder.
  return VfpUsesRegOpTester::
      PassesParsePreconditions(inst, decoder);
}

class MoveVfpRegisterOpWithTypeSelTesterL_20Is0_C_8Is1_A_23To21Is0xx
    : public MoveVfpRegisterOpWithTypeSelTester {
 public:
  MoveVfpRegisterOpWithTypeSelTesterL_20Is0_C_8Is1_A_23To21Is0xx(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpWithTypeSelTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool MoveVfpRegisterOpWithTypeSelTesterL_20Is0_C_8Is1_A_23To21Is0xx
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00100000) != 0x00000000 /* L(20:20) == ~0 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000100 /* C(8:8) == ~1 */) return false;
  if ((inst.Bits() & 0x00800000) != 0x00000000 /* A(23:21) == ~0xx */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpWithTypeSelTester::
      PassesParsePreconditions(inst, decoder);
}

class DuplicateToVfpRegistersTesterL_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x
    : public DuplicateToVfpRegistersTester {
 public:
  DuplicateToVfpRegistersTesterL_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x(const NamedClassDecoder& decoder)
    : DuplicateToVfpRegistersTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool DuplicateToVfpRegistersTesterL_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00100000) != 0x00000000 /* L(20:20) == ~0 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000100 /* C(8:8) == ~1 */) return false;
  if ((inst.Bits() & 0x00800000) != 0x00800000 /* A(23:21) == ~1xx */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000000 /* B(6:5) == ~0x */) return false;

  // Check other preconditions defined for the base decoder.
  return DuplicateToVfpRegistersTester::
      PassesParsePreconditions(inst, decoder);
}

class MoveVfpRegisterOpTesterL_20Is1_C_8Is0_A_23To21Is000
    : public MoveVfpRegisterOpTester {
 public:
  MoveVfpRegisterOpTesterL_20Is1_C_8Is0_A_23To21Is000(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool MoveVfpRegisterOpTesterL_20Is1_C_8Is0_A_23To21Is000
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00100000) != 0x00100000 /* L(20:20) == ~1 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000000 /* C(8:8) == ~0 */) return false;
  if ((inst.Bits() & 0x00E00000) != 0x00000000 /* A(23:21) == ~000 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

class VfpMrsOpTesterL_20Is1_C_8Is0_A_23To21Is111
    : public VfpMrsOpTester {
 public:
  VfpMrsOpTesterL_20Is1_C_8Is0_A_23To21Is111(const NamedClassDecoder& decoder)
    : VfpMrsOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool VfpMrsOpTesterL_20Is1_C_8Is0_A_23To21Is111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00100000) != 0x00100000 /* L(20:20) == ~1 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000000 /* C(8:8) == ~0 */) return false;
  if ((inst.Bits() & 0x00E00000) != 0x00E00000 /* A(23:21) == ~111 */) return false;

  // Check other preconditions defined for the base decoder.
  return VfpMrsOpTester::
      PassesParsePreconditions(inst, decoder);
}

class MoveVfpRegisterOpWithTypeSelTesterL_20Is1_C_8Is1
    : public MoveVfpRegisterOpWithTypeSelTester {
 public:
  MoveVfpRegisterOpWithTypeSelTesterL_20Is1_C_8Is1(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpWithTypeSelTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool MoveVfpRegisterOpWithTypeSelTesterL_20Is1_C_8Is1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00100000) != 0x00100000 /* L(20:20) == ~1 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000100 /* C(8:8) == ~1 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpWithTypeSelTester::
      PassesParsePreconditions(inst, decoder);
}

class UnsafeUncondNopTesterop1_27To20Is101xxxxx
    : public UnsafeUncondNopTester {
 public:
  UnsafeUncondNopTesterop1_27To20Is101xxxxx(const NamedClassDecoder& decoder)
    : UnsafeUncondNopTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

};

bool UnsafeUncondNopTesterop1_27To20Is101xxxxx
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E000000) != 0x0A000000 /* op1(27:20) == ~101xxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondNopTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

class StoreRegisterListTester_op_25To20Is0000x0_Stmda_Stmed_Rule_190_A1_P376
    : public LoadStoreRegisterListTesterop_25To20Is0000x0 {
 public:
  StoreRegisterListTester_op_25To20Is0000x0_Stmda_Stmed_Rule_190_A1_P376()
    : LoadStoreRegisterListTesterop_25To20Is0000x0(
      state_.StoreRegisterList_Stmda_Stmed_Rule_190_A1_P376_instance_)
  {}
};

class LoadRegisterListTester_op_25To20Is0000x1_Ldmda_Ldmfa_Rule_54_A1_P112
    : public LoadStoreRegisterListTesterop_25To20Is0000x1 {
 public:
  LoadRegisterListTester_op_25To20Is0000x1_Ldmda_Ldmfa_Rule_54_A1_P112()
    : LoadStoreRegisterListTesterop_25To20Is0000x1(
      state_.LoadRegisterList_Ldmda_Ldmfa_Rule_54_A1_P112_instance_)
  {}
};

class StoreRegisterListTester_op_25To20Is0010x0_Stm_Stmia_Stmea_Rule_189_A1_P374
    : public LoadStoreRegisterListTesterop_25To20Is0010x0 {
 public:
  StoreRegisterListTester_op_25To20Is0010x0_Stm_Stmia_Stmea_Rule_189_A1_P374()
    : LoadStoreRegisterListTesterop_25To20Is0010x0(
      state_.StoreRegisterList_Stm_Stmia_Stmea_Rule_189_A1_P374_instance_)
  {}
};

class LoadRegisterListTester_op_25To20Is0010x1_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110
    : public LoadStoreRegisterListTesterop_25To20Is0010x1 {
 public:
  LoadRegisterListTester_op_25To20Is0010x1_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110()
    : LoadStoreRegisterListTesterop_25To20Is0010x1(
      state_.LoadRegisterList_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110_instance_)
  {}
};

class StoreRegisterListTester_op_25To20Is0100x0_Stmdb_Stmfd_Rule_191_A1_P378
    : public LoadStoreRegisterListTesterop_25To20Is0100x0 {
 public:
  StoreRegisterListTester_op_25To20Is0100x0_Stmdb_Stmfd_Rule_191_A1_P378()
    : LoadStoreRegisterListTesterop_25To20Is0100x0(
      state_.StoreRegisterList_Stmdb_Stmfd_Rule_191_A1_P378_instance_)
  {}
};

class LoadRegisterListTester_op_25To20Is0100x1_Ldmdb_Ldmea_Rule_55_A1_P114
    : public LoadStoreRegisterListTesterop_25To20Is0100x1 {
 public:
  LoadRegisterListTester_op_25To20Is0100x1_Ldmdb_Ldmea_Rule_55_A1_P114()
    : LoadStoreRegisterListTesterop_25To20Is0100x1(
      state_.LoadRegisterList_Ldmdb_Ldmea_Rule_55_A1_P114_instance_)
  {}
};

class StoreRegisterListTester_op_25To20Is0110x0_Stmid_Stmfa_Rule_192_A1_P380
    : public LoadStoreRegisterListTesterop_25To20Is0110x0 {
 public:
  StoreRegisterListTester_op_25To20Is0110x0_Stmid_Stmfa_Rule_192_A1_P380()
    : LoadStoreRegisterListTesterop_25To20Is0110x0(
      state_.StoreRegisterList_Stmid_Stmfa_Rule_192_A1_P380_instance_)
  {}
};

class LoadRegisterListTester_op_25To20Is0110x1_Ldmib_Ldmed_Rule_56_A1_P116
    : public LoadStoreRegisterListTesterop_25To20Is0110x1 {
 public:
  LoadRegisterListTester_op_25To20Is0110x1_Ldmib_Ldmed_Rule_56_A1_P116()
    : LoadStoreRegisterListTesterop_25To20Is0110x1(
      state_.LoadRegisterList_Ldmib_Ldmed_Rule_56_A1_P116_instance_)
  {}
};

class ForbiddenCondNopTester_op_25To20Is0xx1x0_Stm_Rule_11_B6_A1_P22
    : public UnsafeCondNopTesterop_25To20Is0xx1x0 {
 public:
  ForbiddenCondNopTester_op_25To20Is0xx1x0_Stm_Rule_11_B6_A1_P22()
    : UnsafeCondNopTesterop_25To20Is0xx1x0(
      state_.ForbiddenCondNop_Stm_Rule_11_B6_A1_P22_instance_)
  {}
};

class ForbiddenCondNopTester_op_25To20Is0xx1x1_R_15Is0_Ldm_Rule_3_B6_A1_P7
    : public UnsafeCondNopTesterop_25To20Is0xx1x1_R_15Is0 {
 public:
  ForbiddenCondNopTester_op_25To20Is0xx1x1_R_15Is0_Ldm_Rule_3_B6_A1_P7()
    : UnsafeCondNopTesterop_25To20Is0xx1x1_R_15Is0(
      state_.ForbiddenCondNop_Ldm_Rule_3_B6_A1_P7_instance_)
  {}
};

class ForbiddenCondNopTester_op_25To20Is0xx1x1_R_15Is1_Ldm_Rule_2_B6_A1_P5
    : public UnsafeCondNopTesterop_25To20Is0xx1x1_R_15Is1 {
 public:
  ForbiddenCondNopTester_op_25To20Is0xx1x1_R_15Is1_Ldm_Rule_2_B6_A1_P5()
    : UnsafeCondNopTesterop_25To20Is0xx1x1_R_15Is1(
      state_.ForbiddenCondNop_Ldm_Rule_2_B6_A1_P5_instance_)
  {}
};

class BranchImmediate24Tester_op_25To20Is10xxxx_B_Rule_16_A1_P44
    : public BranchImmediate24Testerop_25To20Is10xxxx {
 public:
  BranchImmediate24Tester_op_25To20Is10xxxx_B_Rule_16_A1_P44()
    : BranchImmediate24Testerop_25To20Is10xxxx(
      state_.BranchImmediate24_B_Rule_16_A1_P44_instance_)
  {}
};

class BranchImmediate24Tester_op_25To20Is11xxxx_Bl_Blx_Rule_23_A1_P58
    : public BranchImmediate24Testerop_25To20Is11xxxx {
 public:
  BranchImmediate24Tester_op_25To20Is11xxxx_Bl_Blx_Rule_23_A1_P58()
    : BranchImmediate24Testerop_25To20Is11xxxx(
      state_.BranchImmediate24_Bl_Blx_Rule_23_A1_P58_instance_)
  {}
};

class ForbiddenCondNopTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011_extra_load_store_instructions_unpriviledged
    : public UnsafeCondNopTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011 {
 public:
  ForbiddenCondNopTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011_extra_load_store_instructions_unpriviledged()
    : UnsafeCondNopTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011(
      state_.ForbiddenCondNop_extra_load_store_instructions_unpriviledged_instance_)
  {}
};

class ForbiddenCondNopTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1_extra_load_store_instructions_unpriviledged
    : public UnsafeCondNopTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1 {
 public:
  ForbiddenCondNopTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1_extra_load_store_instructions_unpriviledged()
    : UnsafeCondNopTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1(
      state_.ForbiddenCondNop_extra_load_store_instructions_unpriviledged_instance_)
  {}
};

class Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10000RegsNotPc_Mov_Rule_96_A2_P194
    : public Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10000RegsNotPc {
 public:
  Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10000RegsNotPc_Mov_Rule_96_A2_P194()
    : Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10000RegsNotPc(
      state_.Unary1RegisterImmediateOp_Mov_Rule_96_A2_P194_instance_)
  {}
};

class Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10100RegsNotPc_Mov_Rule_99_A1_P200
    : public Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10100RegsNotPc {
 public:
  Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10100RegsNotPc_Mov_Rule_99_A1_P200()
    : Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10100RegsNotPc(
      state_.Unary1RegisterImmediateOp_Mov_Rule_99_A1_P200_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_op_24To20Is0000xNotRdIsPcAndS_And_Rule_11_A1_P34
    : public Binary2RegisterImmediateOpTesterop_24To20Is0000xNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0000xNotRdIsPcAndS_And_Rule_11_A1_P34()
    : Binary2RegisterImmediateOpTesterop_24To20Is0000xNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_And_Rule_11_A1_P34_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_op_24To20Is0001xNotRdIsPcAndS_Eor_Rule_44_A1_P94
    : public Binary2RegisterImmediateOpTesterop_24To20Is0001xNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0001xNotRdIsPcAndS_Eor_Rule_44_A1_P94()
    : Binary2RegisterImmediateOpTesterop_24To20Is0001xNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Eor_Rule_44_A1_P94_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_op_24To20Is0010x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Sub_Rule_212_A1_P420
    : public Binary2RegisterImmediateOpTesterop_24To20Is0010x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0010x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Sub_Rule_212_A1_P420()
    : Binary2RegisterImmediateOpTesterop_24To20Is0010x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS(
      state_.Binary2RegisterImmediateOp_Sub_Rule_212_A1_P420_instance_)
  {}
};

class Unary1RegisterImmediateOpTester_op_24To20Is00100_Rn_19To16Is1111_Adr_Rule_10_A2_P32
    : public Unary1RegisterImmediateOpTesterop_24To20Is00100_Rn_19To16Is1111 {
 public:
  Unary1RegisterImmediateOpTester_op_24To20Is00100_Rn_19To16Is1111_Adr_Rule_10_A2_P32()
    : Unary1RegisterImmediateOpTesterop_24To20Is00100_Rn_19To16Is1111(
      state_.Unary1RegisterImmediateOp_Adr_Rule_10_A2_P32_instance_)
  {}
};

class ForbiddenCondNopTester_op_24To20Is00101_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1a
    : public UnsafeCondNopTesterop_24To20Is00101_Rn_19To16Is1111 {
 public:
  ForbiddenCondNopTester_op_24To20Is00101_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1a()
    : UnsafeCondNopTesterop_24To20Is00101_Rn_19To16Is1111(
      state_.ForbiddenCondNop_Subs_Pc_Lr_and_related_instructions_Rule_A1a_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_op_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_142_A1_P284
    : public Binary2RegisterImmediateOpTesterop_24To20Is0011xNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_142_A1_P284()
    : Binary2RegisterImmediateOpTesterop_24To20Is0011xNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Rsb_Rule_142_A1_P284_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_op_24To20Is0100x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Add_Rule_5_A1_P22
    : public Binary2RegisterImmediateOpTesterop_24To20Is0100x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0100x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Add_Rule_5_A1_P22()
    : Binary2RegisterImmediateOpTesterop_24To20Is0100x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS(
      state_.Binary2RegisterImmediateOp_Add_Rule_5_A1_P22_instance_)
  {}
};

class Unary1RegisterImmediateOpTester_op_24To20Is01000_Rn_19To16Is1111_Adr_Rule_10_A1_P32
    : public Unary1RegisterImmediateOpTesterop_24To20Is01000_Rn_19To16Is1111 {
 public:
  Unary1RegisterImmediateOpTester_op_24To20Is01000_Rn_19To16Is1111_Adr_Rule_10_A1_P32()
    : Unary1RegisterImmediateOpTesterop_24To20Is01000_Rn_19To16Is1111(
      state_.Unary1RegisterImmediateOp_Adr_Rule_10_A1_P32_instance_)
  {}
};

class ForbiddenCondNopTester_op_24To20Is01001_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1b
    : public UnsafeCondNopTesterop_24To20Is01001_Rn_19To16Is1111 {
 public:
  ForbiddenCondNopTester_op_24To20Is01001_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1b()
    : UnsafeCondNopTesterop_24To20Is01001_Rn_19To16Is1111(
      state_.ForbiddenCondNop_Subs_Pc_Lr_and_related_instructions_Rule_A1b_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_op_24To20Is0101xNotRdIsPcAndS_Adc_Rule_6_A1_P14
    : public Binary2RegisterImmediateOpTesterop_24To20Is0101xNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0101xNotRdIsPcAndS_Adc_Rule_6_A1_P14()
    : Binary2RegisterImmediateOpTesterop_24To20Is0101xNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Adc_Rule_6_A1_P14_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_op_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_151_A1_P302
    : public Binary2RegisterImmediateOpTesterop_24To20Is0110xNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_151_A1_P302()
    : Binary2RegisterImmediateOpTesterop_24To20Is0110xNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Sbc_Rule_151_A1_P302_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_op_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_145_A1_P290
    : public Binary2RegisterImmediateOpTesterop_24To20Is0111xNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_145_A1_P290()
    : Binary2RegisterImmediateOpTesterop_24To20Is0111xNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Rsc_Rule_145_A1_P290_instance_)
  {}
};

class MaskedBinaryRegisterImmediateTestTester_op_24To20Is10001_Tst_Rule_230_A1_P454
    : public BinaryRegisterImmediateTestTesterop_24To20Is10001 {
 public:
  MaskedBinaryRegisterImmediateTestTester_op_24To20Is10001_Tst_Rule_230_A1_P454()
    : BinaryRegisterImmediateTestTesterop_24To20Is10001(
      state_.MaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454_instance_)
  {}
};

class BinaryRegisterImmediateTestTester_op_24To20Is10011_Teq_Rule_227_A1_P448
    : public BinaryRegisterImmediateTestTesterop_24To20Is10011 {
 public:
  BinaryRegisterImmediateTestTester_op_24To20Is10011_Teq_Rule_227_A1_P448()
    : BinaryRegisterImmediateTestTesterop_24To20Is10011(
      state_.BinaryRegisterImmediateTest_Teq_Rule_227_A1_P448_instance_)
  {}
};

class BinaryRegisterImmediateTestTester_op_24To20Is10101_Cmp_Rule_35_A1_P80
    : public BinaryRegisterImmediateTestTesterop_24To20Is10101 {
 public:
  BinaryRegisterImmediateTestTester_op_24To20Is10101_Cmp_Rule_35_A1_P80()
    : BinaryRegisterImmediateTestTesterop_24To20Is10101(
      state_.BinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80_instance_)
  {}
};

class BinaryRegisterImmediateTestTester_op_24To20Is10111_Cmn_Rule_32_A1_P74
    : public BinaryRegisterImmediateTestTesterop_24To20Is10111 {
 public:
  BinaryRegisterImmediateTestTester_op_24To20Is10111_Cmn_Rule_32_A1_P74()
    : BinaryRegisterImmediateTestTesterop_24To20Is10111(
      state_.BinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74_instance_)
  {}
};

class Binary2RegisterImmediateOpTester_op_24To20Is1100xNotRdIsPcAndS_Orr_Rule_113_A1_P228
    : public Binary2RegisterImmediateOpTesterop_24To20Is1100xNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is1100xNotRdIsPcAndS_Orr_Rule_113_A1_P228()
    : Binary2RegisterImmediateOpTesterop_24To20Is1100xNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Orr_Rule_113_A1_P228_instance_)
  {}
};

class Unary1RegisterImmediateOpTester_op_24To20Is1101xNotRdIsPcAndS_Mov_Rule_96_A1_P194
    : public Unary1RegisterImmediateOpTesterop_24To20Is1101xNotRdIsPcAndS {
 public:
  Unary1RegisterImmediateOpTester_op_24To20Is1101xNotRdIsPcAndS_Mov_Rule_96_A1_P194()
    : Unary1RegisterImmediateOpTesterop_24To20Is1101xNotRdIsPcAndS(
      state_.Unary1RegisterImmediateOp_Mov_Rule_96_A1_P194_instance_)
  {}
};

class MaskedBinary2RegisterImmediateOpTester_op_24To20Is1110xNotRdIsPcAndS_Bic_Rule_19_A1_P50
    : public Binary2RegisterImmediateOpTesterop_24To20Is1110xNotRdIsPcAndS {
 public:
  MaskedBinary2RegisterImmediateOpTester_op_24To20Is1110xNotRdIsPcAndS_Bic_Rule_19_A1_P50()
    : Binary2RegisterImmediateOpTesterop_24To20Is1110xNotRdIsPcAndS(
      state_.MaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50_instance_)
  {}
};

class Unary1RegisterImmediateOpTester_op_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_106_A1_P214
    : public Unary1RegisterImmediateOpTesterop_24To20Is1111xNotRdIsPcAndS {
 public:
  Unary1RegisterImmediateOpTester_op_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_106_A1_P214()
    : Unary1RegisterImmediateOpTesterop_24To20Is1111xNotRdIsPcAndS(
      state_.Unary1RegisterImmediateOp_Mvn_Rule_106_A1_P214_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0000xNotRdIsPcAndS_And_Rule_7_A1_P36
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0000xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0000xNotRdIsPcAndS_And_Rule_7_A1_P36()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0000xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_And_Rule_7_A1_P36_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0001xNotRdIsPcAndS_Eor_Rule_45_A1_P96
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0001xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0001xNotRdIsPcAndS_Eor_Rule_45_A1_P96()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0001xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0010xNotRdIsPcAndS_Sub_Rule_213_A1_P422
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0010xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0010xNotRdIsPcAndS_Sub_Rule_213_A1_P422()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0010xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Sub_Rule_213_A1_P422_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_143_P286
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0011xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_143_P286()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0011xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Rsb_Rule_143_P286_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0100xNotRdIsPcAndS_Add_Rule_6_A1_P24
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0100xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0100xNotRdIsPcAndS_Add_Rule_6_A1_P24()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0100xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0101xNotRdIsPcAndS_Adc_Rule_2_A1_P16
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0101xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0101xNotRdIsPcAndS_Adc_Rule_2_A1_P16()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0101xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_152_A1_P304
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0110xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_152_A1_P304()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0110xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_146_A1_P292
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0111xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_146_A1_P292()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0111xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292_instance_)
  {}
};

class Binary2RegisterImmedShiftedTestTester_op1_24To20Is10001_Tst_Rule_231_A1_P456
    : public Binary2RegisterImmedShiftedTestTesterop1_24To20Is10001 {
 public:
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10001_Tst_Rule_231_A1_P456()
    : Binary2RegisterImmedShiftedTestTesterop1_24To20Is10001(
      state_.Binary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456_instance_)
  {}
};

class Binary2RegisterImmedShiftedTestTester_op1_24To20Is10011_Teq_Rule_228_A1_P450
    : public Binary2RegisterImmedShiftedTestTesterop1_24To20Is10011 {
 public:
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10011_Teq_Rule_228_A1_P450()
    : Binary2RegisterImmedShiftedTestTesterop1_24To20Is10011(
      state_.Binary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450_instance_)
  {}
};

class Binary2RegisterImmedShiftedTestTester_op1_24To20Is10101_Cmp_Rule_36_A1_P82
    : public Binary2RegisterImmedShiftedTestTesterop1_24To20Is10101 {
 public:
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10101_Cmp_Rule_36_A1_P82()
    : Binary2RegisterImmedShiftedTestTesterop1_24To20Is10101(
      state_.Binary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82_instance_)
  {}
};

class Binary2RegisterImmedShiftedTestTester_op1_24To20Is10111_Cmn_Rule_33_A1_P76
    : public Binary2RegisterImmedShiftedTestTesterop1_24To20Is10111 {
 public:
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10111_Cmn_Rule_33_A1_P76()
    : Binary2RegisterImmedShiftedTestTesterop1_24To20Is10111(
      state_.Binary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_op1_24To20Is1100xNotRdIsPcAndS_Orr_Rule_114_A1_P230
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is1100xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is1100xNotRdIsPcAndS_Orr_Rule_114_A1_P230()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is1100xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230_instance_)
  {}
};

class Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is00NotRdIsPcAndS_Mov_Rule_97_A1_P196
    : public Unary2RegisterOpTesterop1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is00NotRdIsPcAndS {
 public:
  Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is00NotRdIsPcAndS_Mov_Rule_97_A1_P196()
    : Unary2RegisterOpTesterop1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is00NotRdIsPcAndS(
      state_.Unary2RegisterOp_Mov_Rule_97_A1_P196_instance_)
  {}
};

class Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS_Lsl_Rule_88_A1_P178
    : public Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS_Lsl_Rule_88_A1_P178()
    : Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS(
      state_.Unary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178_instance_)
  {}
};

class Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is01NotRdIsPcAndS_Lsr_Rule_90_A1_P182
    : public Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op3_6To5Is01NotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is01NotRdIsPcAndS_Lsr_Rule_90_A1_P182()
    : Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op3_6To5Is01NotRdIsPcAndS(
      state_.Unary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182_instance_)
  {}
};

class Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is10NotRdIsPcAndS_Asr_Rule_14_A1_P40
    : public Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op3_6To5Is10NotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is10NotRdIsPcAndS_Asr_Rule_14_A1_P40()
    : Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op3_6To5Is10NotRdIsPcAndS(
      state_.Unary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40_instance_)
  {}
};

class Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is11NotRdIsPcAndS_Rrx_Rule_141_A1_P282
    : public Unary2RegisterOpTesterop1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is11NotRdIsPcAndS {
 public:
  Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is11NotRdIsPcAndS_Rrx_Rule_141_A1_P282()
    : Unary2RegisterOpTesterop1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is11NotRdIsPcAndS(
      state_.Unary2RegisterOp_Rrx_Rule_141_A1_P282_instance_)
  {}
};

class Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS_Ror_Rule_139_A1_P278
    : public Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS_Ror_Rule_139_A1_P278()
    : Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS(
      state_.Unary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpTester_op1_24To20Is1110xNotRdIsPcAndS_Bic_Rule_20_A1_P52
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is1110xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is1110xNotRdIsPcAndS_Bic_Rule_20_A1_P52()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is1110xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52_instance_)
  {}
};

class Unary2RegisterImmedShiftedOpTester_op1_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_107_A1_P216
    : public Unary2RegisterImmedShiftedOpTesterop1_24To20Is1111xNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_107_A1_P216()
    : Unary2RegisterImmedShiftedOpTesterop1_24To20Is1111xNotRdIsPcAndS(
      state_.Unary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_op1_24To20Is0000xRegsNotPc_And_Rule_13_A1_P38
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0000xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0000xRegsNotPc_And_Rule_13_A1_P38()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0000xRegsNotPc(
      state_.Binary4RegisterShiftedOp_And_Rule_13_A1_P38_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_op1_24To20Is0001xRegsNotPc_Eor_Rule_46_A1_P98
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0001xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0001xRegsNotPc_Eor_Rule_46_A1_P98()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0001xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Eor_Rule_46_A1_P98_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_op1_24To20Is0010xRegsNotPc_Sub_Rule_214_A1_P424
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0010xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0010xRegsNotPc_Sub_Rule_214_A1_P424()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0010xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Sub_Rule_214_A1_P424_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_op1_24To20Is0011xRegsNotPc_Rsb_Rule_144_A1_P288
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0011xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0011xRegsNotPc_Rsb_Rule_144_A1_P288()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0011xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Rsb_Rule_144_A1_P288_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_op1_24To20Is0100xRegsNotPc_Add_Rule_7_A1_P26
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0100xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0100xRegsNotPc_Add_Rule_7_A1_P26()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0100xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Add_Rule_7_A1_P26_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_op1_24To20Is0101xRegsNotPc_Adc_Rule_3_A1_P18
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0101xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0101xRegsNotPc_Adc_Rule_3_A1_P18()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0101xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Adc_Rule_3_A1_P18_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_op1_24To20Is0110xRegsNotPc_Sbc_Rule_153_A1_P306
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0110xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0110xRegsNotPc_Sbc_Rule_153_A1_P306()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0110xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Sbc_Rule_153_A1_P306_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_op1_24To20Is0111xRegsNotPc_Rsc_Rule_147_A1_P294
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0111xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0111xRegsNotPc_Rsc_Rule_147_A1_P294()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0111xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Rsc_Rule_147_A1_P294_instance_)
  {}
};

class Binary3RegisterShiftedTestTester_op1_24To20Is10001RegsNotPc_Tst_Rule_232_A1_P458
    : public Binary3RegisterShiftedTestTesterop1_24To20Is10001RegsNotPc {
 public:
  Binary3RegisterShiftedTestTester_op1_24To20Is10001RegsNotPc_Tst_Rule_232_A1_P458()
    : Binary3RegisterShiftedTestTesterop1_24To20Is10001RegsNotPc(
      state_.Binary3RegisterShiftedTest_Tst_Rule_232_A1_P458_instance_)
  {}
};

class Binary3RegisterShiftedTestTester_op1_24To20Is10011RegsNotPc_Teq_Rule_229_A1_P452
    : public Binary3RegisterShiftedTestTesterop1_24To20Is10011RegsNotPc {
 public:
  Binary3RegisterShiftedTestTester_op1_24To20Is10011RegsNotPc_Teq_Rule_229_A1_P452()
    : Binary3RegisterShiftedTestTesterop1_24To20Is10011RegsNotPc(
      state_.Binary3RegisterShiftedTest_Teq_Rule_229_A1_P452_instance_)
  {}
};

class Binary3RegisterShiftedTestTester_op1_24To20Is10101RegsNotPc_Cmp_Rule_37_A1_P84
    : public Binary3RegisterShiftedTestTesterop1_24To20Is10101RegsNotPc {
 public:
  Binary3RegisterShiftedTestTester_op1_24To20Is10101RegsNotPc_Cmp_Rule_37_A1_P84()
    : Binary3RegisterShiftedTestTesterop1_24To20Is10101RegsNotPc(
      state_.Binary3RegisterShiftedTest_Cmp_Rule_37_A1_P84_instance_)
  {}
};

class Binary3RegisterShiftedTestTester_op1_24To20Is10111RegsNotPc_Cmn_Rule_34_A1_P78
    : public Binary3RegisterShiftedTestTesterop1_24To20Is10111RegsNotPc {
 public:
  Binary3RegisterShiftedTestTester_op1_24To20Is10111RegsNotPc_Cmn_Rule_34_A1_P78()
    : Binary3RegisterShiftedTestTesterop1_24To20Is10111RegsNotPc(
      state_.Binary3RegisterShiftedTest_Cmn_Rule_34_A1_P78_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_op1_24To20Is1100xRegsNotPc_Orr_Rule_115_A1_P212
    : public Binary4RegisterShiftedOpTesterop1_24To20Is1100xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is1100xRegsNotPc_Orr_Rule_115_A1_P212()
    : Binary4RegisterShiftedOpTesterop1_24To20Is1100xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Orr_Rule_115_A1_P212_instance_)
  {}
};

class Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is00RegsNotPc_Lsl_Rule_89_A1_P180
    : public Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is00RegsNotPc {
 public:
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is00RegsNotPc_Lsl_Rule_89_A1_P180()
    : Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is00RegsNotPc(
      state_.Binary3RegisterOp_Lsl_Rule_89_A1_P180_instance_)
  {}
};

class Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is01RegsNotPc_Lsr_Rule_91_A1_P184
    : public Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is01RegsNotPc {
 public:
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is01RegsNotPc_Lsr_Rule_91_A1_P184()
    : Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is01RegsNotPc(
      state_.Binary3RegisterOp_Lsr_Rule_91_A1_P184_instance_)
  {}
};

class Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is10RegsNotPc_Asr_Rule_15_A1_P42
    : public Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is10RegsNotPc {
 public:
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is10RegsNotPc_Asr_Rule_15_A1_P42()
    : Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is10RegsNotPc(
      state_.Binary3RegisterOp_Asr_Rule_15_A1_P42_instance_)
  {}
};

class Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is11RegsNotPc_Ror_Rule_140_A1_P280
    : public Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is11RegsNotPc {
 public:
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is11RegsNotPc_Ror_Rule_140_A1_P280()
    : Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is11RegsNotPc(
      state_.Binary3RegisterOp_Ror_Rule_140_A1_P280_instance_)
  {}
};

class Binary4RegisterShiftedOpTester_op1_24To20Is1110xRegsNotPc_Bic_Rule_21_A1_P54
    : public Binary4RegisterShiftedOpTesterop1_24To20Is1110xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is1110xRegsNotPc_Bic_Rule_21_A1_P54()
    : Binary4RegisterShiftedOpTesterop1_24To20Is1110xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Bic_Rule_21_A1_P54_instance_)
  {}
};

class Unary3RegisterShiftedOpTester_op1_24To20Is1111xRegsNotPc_Mvn_Rule_108_A1_P218
    : public Unary3RegisterShiftedOpTesterop1_24To20Is1111xRegsNotPc {
 public:
  Unary3RegisterShiftedOpTester_op1_24To20Is1111xRegsNotPc_Mvn_Rule_108_A1_P218()
    : Unary3RegisterShiftedOpTesterop1_24To20Is1111xRegsNotPc(
      state_.Unary3RegisterShiftedOp_Mvn_Rule_108_A1_P218_instance_)
  {}
};

class StoreVectorRegisterListTester_opcode_24To20Is01x00_Vstm_Rule_399_A1_A2_P784
    : public StoreVectorRegisterListTesteropcode_24To20Is01x00 {
 public:
  StoreVectorRegisterListTester_opcode_24To20Is01x00_Vstm_Rule_399_A1_A2_P784()
    : StoreVectorRegisterListTesteropcode_24To20Is01x00(
      state_.StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_)
  {}
};

class StoreVectorRegisterListTester_opcode_24To20Is01x10_Vstm_Rule_399_A1_A2_P784
    : public StoreVectorRegisterListTesteropcode_24To20Is01x10 {
 public:
  StoreVectorRegisterListTester_opcode_24To20Is01x10_Vstm_Rule_399_A1_A2_P784()
    : StoreVectorRegisterListTesteropcode_24To20Is01x10(
      state_.StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_)
  {}
};

class StoreVectorRegisterTester_opcode_24To20Is1xx00_Vstr_Rule_400_A1_A2_P786
    : public StoreVectorRegisterTesteropcode_24To20Is1xx00 {
 public:
  StoreVectorRegisterTester_opcode_24To20Is1xx00_Vstr_Rule_400_A1_A2_P786()
    : StoreVectorRegisterTesteropcode_24To20Is1xx00(
      state_.StoreVectorRegister_Vstr_Rule_400_A1_A2_P786_instance_)
  {}
};

class StoreVectorRegisterListTester_opcode_24To20Is10x10_NotRn_19To16Is1101NotRnIsSp_Vstm_Rule_399_A1_A2_P784
    : public StoreVectorRegisterListTesteropcode_24To20Is10x10_NotRn_19To16Is1101NotRnIsSp {
 public:
  StoreVectorRegisterListTester_opcode_24To20Is10x10_NotRn_19To16Is1101NotRnIsSp_Vstm_Rule_399_A1_A2_P784()
    : StoreVectorRegisterListTesteropcode_24To20Is10x10_NotRn_19To16Is1101NotRnIsSp(
      state_.StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_)
  {}
};

class StoreVectorRegisterListTester_opcode_24To20Is10x10_Rn_19To16Is1101_Vpush_355_A1_A2_P696
    : public StoreVectorRegisterListTesteropcode_24To20Is10x10_Rn_19To16Is1101 {
 public:
  StoreVectorRegisterListTester_opcode_24To20Is10x10_Rn_19To16Is1101_Vpush_355_A1_A2_P696()
    : StoreVectorRegisterListTesteropcode_24To20Is10x10_Rn_19To16Is1101(
      state_.StoreVectorRegisterList_Vpush_355_A1_A2_P696_instance_)
  {}
};

class LoadVectorRegisterListTester_opcode_24To20Is01x01_Vldm_Rule_319_A1_A2_P626
    : public LoadStoreVectorRegisterListTesteropcode_24To20Is01x01 {
 public:
  LoadVectorRegisterListTester_opcode_24To20Is01x01_Vldm_Rule_319_A1_A2_P626()
    : LoadStoreVectorRegisterListTesteropcode_24To20Is01x01(
      state_.LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626_instance_)
  {}
};

class LoadVectorRegisterListTester_opcode_24To20Is01x11_NotRn_19To16Is1101NotRnIsSp_Vldm_Rule_319_A1_A2_P626
    : public LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_NotRn_19To16Is1101NotRnIsSp {
 public:
  LoadVectorRegisterListTester_opcode_24To20Is01x11_NotRn_19To16Is1101NotRnIsSp_Vldm_Rule_319_A1_A2_P626()
    : LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_NotRn_19To16Is1101NotRnIsSp(
      state_.LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626_instance_)
  {}
};

class LoadVectorRegisterListTester_opcode_24To20Is01x11_Rn_19To16Is1101_Vpop_Rule_354_A1_A2_P694
    : public LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_Rn_19To16Is1101 {
 public:
  LoadVectorRegisterListTester_opcode_24To20Is01x11_Rn_19To16Is1101_Vpop_Rule_354_A1_A2_P694()
    : LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_Rn_19To16Is1101(
      state_.LoadVectorRegisterList_Vpop_Rule_354_A1_A2_P694_instance_)
  {}
};

class LoadVectorRegisterTester_opcode_24To20Is1xx01_Vldr_Rule_320_A1_A2_P628
    : public LoadStoreVectorOpTesteropcode_24To20Is1xx01 {
 public:
  LoadVectorRegisterTester_opcode_24To20Is1xx01_Vldr_Rule_320_A1_A2_P628()
    : LoadStoreVectorOpTesteropcode_24To20Is1xx01(
      state_.LoadVectorRegister_Vldr_Rule_320_A1_A2_P628_instance_)
  {}
};

class LoadVectorRegisterListTester_opcode_24To20Is10x11_Vldm_Rule_318_A1_A2_P626
    : public LoadStoreVectorRegisterListTesteropcode_24To20Is10x11 {
 public:
  LoadVectorRegisterListTester_opcode_24To20Is10x11_Vldm_Rule_318_A1_A2_P626()
    : LoadStoreVectorRegisterListTesteropcode_24To20Is10x11(
      state_.LoadVectorRegisterList_Vldm_Rule_318_A1_A2_P626_instance_)
  {}
};

class Store3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x0_Strh_Rule_208_A1_P412
    : public LoadStore3RegisterOpTesterop2_6To5Is01_op1_24To20Isxx0x0 {
 public:
  Store3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x0_Strh_Rule_208_A1_P412()
    : LoadStore3RegisterOpTesterop2_6To5Is01_op1_24To20Isxx0x0(
      state_.Store3RegisterOp_Strh_Rule_208_A1_P412_instance_)
  {}
};

class Load3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x1_Ldrh_Rule_76_A1_P156
    : public LoadStore3RegisterOpTesterop2_6To5Is01_op1_24To20Isxx0x1 {
 public:
  Load3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x1_Ldrh_Rule_76_A1_P156()
    : LoadStore3RegisterOpTesterop2_6To5Is01_op1_24To20Isxx0x1(
      state_.Load3RegisterOp_Ldrh_Rule_76_A1_P156_instance_)
  {}
};

class Store2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x0_Strh_Rule_207_A1_P410
    : public LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x0 {
 public:
  Store2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x0_Strh_Rule_207_A1_P410()
    : LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x0(
      state_.Store2RegisterImm8Op_Strh_Rule_207_A1_P410_instance_)
  {}
};

class Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_NotRn_19To16Is1111_Ldrh_Rule_74_A1_P152
    : public LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_NotRn_19To16Is1111 {
 public:
  Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_NotRn_19To16Is1111_Ldrh_Rule_74_A1_P152()
    : LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_NotRn_19To16Is1111(
      state_.Load2RegisterImm8Op_Ldrh_Rule_74_A1_P152_instance_)
  {}
};

class Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrh_Rule_75_A1_P154
    : public LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16Is1111 {
 public:
  Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrh_Rule_75_A1_P154()
    : LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16Is1111(
      state_.Load2RegisterImm8Op_Ldrh_Rule_75_A1_P154_instance_)
  {}
};

class Load3RegisterDoubleOpTester_op2_6To5Is10_op1_24To20Isxx0x0_Ldrd_Rule_68_A1_P140
    : public LoadStore3RegisterDoubleOpTesterop2_6To5Is10_op1_24To20Isxx0x0 {
 public:
  Load3RegisterDoubleOpTester_op2_6To5Is10_op1_24To20Isxx0x0_Ldrd_Rule_68_A1_P140()
    : LoadStore3RegisterDoubleOpTesterop2_6To5Is10_op1_24To20Isxx0x0(
      state_.Load3RegisterDoubleOp_Ldrd_Rule_68_A1_P140_instance_)
  {}
};

class Load3RegisterOpTester_op2_6To5Is10_op1_24To20Isxx0x1_Ldrsb_Rule_80_A1_P164
    : public LoadStore3RegisterOpTesterop2_6To5Is10_op1_24To20Isxx0x1 {
 public:
  Load3RegisterOpTester_op2_6To5Is10_op1_24To20Isxx0x1_Ldrsb_Rule_80_A1_P164()
    : LoadStore3RegisterOpTesterop2_6To5Is10_op1_24To20Isxx0x1(
      state_.Load3RegisterOp_Ldrsb_Rule_80_A1_P164_instance_)
  {}
};

class Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_NotRn_19To16Is1111_Ldrd_Rule_66_A1_P136
    : public LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_NotRn_19To16Is1111 {
 public:
  Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_NotRn_19To16Is1111_Ldrd_Rule_66_A1_P136()
    : LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_NotRn_19To16Is1111(
      state_.Load2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136_instance_)
  {}
};

class Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16Is1111_Ldrd_Rule_67_A1_P138
    : public LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16Is1111 {
 public:
  Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16Is1111_Ldrd_Rule_67_A1_P138()
    : LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16Is1111(
      state_.Load2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138_instance_)
  {}
};

class Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_NotRn_19To16Is1111_Ldrsb_Rule_78_A1_P160
    : public LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_NotRn_19To16Is1111 {
 public:
  Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_NotRn_19To16Is1111_Ldrsb_Rule_78_A1_P160()
    : LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_NotRn_19To16Is1111(
      state_.Load2RegisterImm8Op_Ldrsb_Rule_78_A1_P160_instance_)
  {}
};

class Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16Is1111_ldrsb_Rule_79_A1_162
    : public LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16Is1111 {
 public:
  Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16Is1111_ldrsb_Rule_79_A1_162()
    : LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16Is1111(
      state_.Load2RegisterImm8Op_ldrsb_Rule_79_A1_162_instance_)
  {}
};

class Store3RegisterDoubleOpTester_op2_6To5Is11_op1_24To20Isxx0x0_Strd_Rule_201_A1_P398
    : public LoadStore3RegisterDoubleOpTesterop2_6To5Is11_op1_24To20Isxx0x0 {
 public:
  Store3RegisterDoubleOpTester_op2_6To5Is11_op1_24To20Isxx0x0_Strd_Rule_201_A1_P398()
    : LoadStore3RegisterDoubleOpTesterop2_6To5Is11_op1_24To20Isxx0x0(
      state_.Store3RegisterDoubleOp_Strd_Rule_201_A1_P398_instance_)
  {}
};

class Load3RegisterOpTester_op2_6To5Is11_op1_24To20Isxx0x1_Ldrsh_Rule_84_A1_P172
    : public LoadStore3RegisterOpTesterop2_6To5Is11_op1_24To20Isxx0x1 {
 public:
  Load3RegisterOpTester_op2_6To5Is11_op1_24To20Isxx0x1_Ldrsh_Rule_84_A1_P172()
    : LoadStore3RegisterOpTesterop2_6To5Is11_op1_24To20Isxx0x1(
      state_.Load3RegisterOp_Ldrsh_Rule_84_A1_P172_instance_)
  {}
};

class Store2RegisterImm8DoubleOpTester_op2_6To5Is11_op1_24To20Isxx1x0_Strd_Rule_200_A1_P396
    : public LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is11_op1_24To20Isxx1x0 {
 public:
  Store2RegisterImm8DoubleOpTester_op2_6To5Is11_op1_24To20Isxx1x0_Strd_Rule_200_A1_P396()
    : LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is11_op1_24To20Isxx1x0(
      state_.Store2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396_instance_)
  {}
};

class Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_NotRn_19To16Is1111_Ldrsh_Rule_82_A1_P168
    : public LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_NotRn_19To16Is1111 {
 public:
  Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_NotRn_19To16Is1111_Ldrsh_Rule_82_A1_P168()
    : LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_NotRn_19To16Is1111(
      state_.Load2RegisterImm8Op_Ldrsh_Rule_82_A1_P168_instance_)
  {}
};

class Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrsh_Rule_83_A1_P170
    : public LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16Is1111 {
 public:
  Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrsh_Rule_83_A1_P170()
    : LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16Is1111(
      state_.Load2RegisterImm8Op_Ldrsh_Rule_83_A1_P170_instance_)
  {}
};

class CondVfpOpTester_opc1_23To20Is0x00_Vm_la_ls_Rule_423_A2_P636
    : public CondVfpOpTesteropc1_23To20Is0x00 {
 public:
  CondVfpOpTester_opc1_23To20Is0x00_Vm_la_ls_Rule_423_A2_P636()
    : CondVfpOpTesteropc1_23To20Is0x00(
      state_.CondVfpOp_Vm_la_ls_Rule_423_A2_P636_instance_)
  {}
};

class CondVfpOpTester_opc1_23To20Is0x01_Vnm_la_ls_ul_Rule_343_A1_P674
    : public CondVfpOpTesteropc1_23To20Is0x01 {
 public:
  CondVfpOpTester_opc1_23To20Is0x01_Vnm_la_ls_ul_Rule_343_A1_P674()
    : CondVfpOpTesteropc1_23To20Is0x01(
      state_.CondVfpOp_Vnm_la_ls_ul_Rule_343_A1_P674_instance_)
  {}
};

class CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx1_Vnm_la_ls_ul_Rule_343_A2_P674
    : public CondVfpOpTesteropc1_23To20Is0x10_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx1_Vnm_la_ls_ul_Rule_343_A2_P674()
    : CondVfpOpTesteropc1_23To20Is0x10_opc3_7To6Isx1(
      state_.CondVfpOp_Vnm_la_ls_ul_Rule_343_A2_P674_instance_)
  {}
};

class CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx0_Vmul_Rule_338_A2_P664
    : public CondVfpOpTesteropc1_23To20Is0x10_opc3_7To6Isx0 {
 public:
  CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx0_Vmul_Rule_338_A2_P664()
    : CondVfpOpTesteropc1_23To20Is0x10_opc3_7To6Isx0(
      state_.CondVfpOp_Vmul_Rule_338_A2_P664_instance_)
  {}
};

class CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx0_Vadd_Rule_271_A2_P536
    : public CondVfpOpTesteropc1_23To20Is0x11_opc3_7To6Isx0 {
 public:
  CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx0_Vadd_Rule_271_A2_P536()
    : CondVfpOpTesteropc1_23To20Is0x11_opc3_7To6Isx0(
      state_.CondVfpOp_Vadd_Rule_271_A2_P536_instance_)
  {}
};

class CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx1_Vsub_Rule_402_A2_P790
    : public CondVfpOpTesteropc1_23To20Is0x11_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx1_Vsub_Rule_402_A2_P790()
    : CondVfpOpTesteropc1_23To20Is0x11_opc3_7To6Isx1(
      state_.CondVfpOp_Vsub_Rule_402_A2_P790_instance_)
  {}
};

class CondVfpOpTester_opc1_23To20Is1x00_opc3_7To6Isx0_Vdiv_Rule_301_A1_P590
    : public CondVfpOpTesteropc1_23To20Is1x00_opc3_7To6Isx0 {
 public:
  CondVfpOpTester_opc1_23To20Is1x00_opc3_7To6Isx0_Vdiv_Rule_301_A1_P590()
    : CondVfpOpTesteropc1_23To20Is1x00_opc3_7To6Isx0(
      state_.CondVfpOp_Vdiv_Rule_301_A1_P590_instance_)
  {}
};

class Binary4RegisterDualOpTester_op1_22To21Is00RegsNotPc_Smlaxx_Rule_166_A1_P330
    : public Binary4RegisterDualOpTesterop1_22To21Is00RegsNotPc {
 public:
  Binary4RegisterDualOpTester_op1_22To21Is00RegsNotPc_Smlaxx_Rule_166_A1_P330()
    : Binary4RegisterDualOpTesterop1_22To21Is00RegsNotPc(
      state_.Binary4RegisterDualOp_Smlaxx_Rule_166_A1_P330_instance_)
  {}
};

class Binary4RegisterDualOpTester_op1_22To21Is01_op_5Is0RegsNotPc_Smlawx_Rule_171_A1_340
    : public Binary4RegisterDualOpTesterop1_22To21Is01_op_5Is0RegsNotPc {
 public:
  Binary4RegisterDualOpTester_op1_22To21Is01_op_5Is0RegsNotPc_Smlawx_Rule_171_A1_340()
    : Binary4RegisterDualOpTesterop1_22To21Is01_op_5Is0RegsNotPc(
      state_.Binary4RegisterDualOp_Smlawx_Rule_171_A1_340_instance_)
  {}
};

class Binary3RegisterOpAltATester_op1_22To21Is01_op_5Is1RegsNotPc_Smulwx_Rule_180_A1_P358
    : public Binary3RegisterOpAltATesterop1_22To21Is01_op_5Is1RegsNotPc {
 public:
  Binary3RegisterOpAltATester_op1_22To21Is01_op_5Is1RegsNotPc_Smulwx_Rule_180_A1_P358()
    : Binary3RegisterOpAltATesterop1_22To21Is01_op_5Is1RegsNotPc(
      state_.Binary3RegisterOpAltA_Smulwx_Rule_180_A1_P358_instance_)
  {}
};

class Binary4RegisterDualResultTester_op1_22To21Is10RegsNotPc_Smlalxx_Rule_169_A1_P336
    : public Binary4RegisterDualResultTesterop1_22To21Is10RegsNotPc {
 public:
  Binary4RegisterDualResultTester_op1_22To21Is10RegsNotPc_Smlalxx_Rule_169_A1_P336()
    : Binary4RegisterDualResultTesterop1_22To21Is10RegsNotPc(
      state_.Binary4RegisterDualResult_Smlalxx_Rule_169_A1_P336_instance_)
  {}
};

class Binary3RegisterOpAltATester_op1_22To21Is11RegsNotPc_Smulxx_Rule_178_P354
    : public Binary3RegisterOpAltATesterop1_22To21Is11RegsNotPc {
 public:
  Binary3RegisterOpAltATester_op1_22To21Is11RegsNotPc_Smulxx_Rule_178_P354()
    : Binary3RegisterOpAltATesterop1_22To21Is11RegsNotPc(
      state_.Binary3RegisterOpAltA_Smulxx_Rule_178_P354_instance_)
  {}
};

class Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x0_B_4Is0_Notop1_repeated_24To20Is0x010_Str_Rule_195_A1_P386
    : public LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x0_B_4Is0_Notop1_repeated_24To20Is0x010 {
 public:
  Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x0_B_4Is0_Notop1_repeated_24To20Is0x010_Str_Rule_195_A1_P386()
    : LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x0_B_4Is0_Notop1_repeated_24To20Is0x010(
      state_.Store3RegisterImm5Op_Str_Rule_195_A1_P386_instance_)
  {}
};

class ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x010_Strt_Rule_A1
    : public UnsafeCondNopTesterA_25Is0_op1_24To20Is0x010 {
 public:
  ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x010_Strt_Rule_A1()
    : UnsafeCondNopTesterA_25Is0_op1_24To20Is0x010(
      state_.ForbiddenCondNop_Strt_Rule_A1_instance_)
  {}
};

class ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x010_B_4Is0_Strt_Rule_A2
    : public UnsafeCondNopTesterA_25Is1_op1_24To20Is0x010_B_4Is0 {
 public:
  ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x010_B_4Is0_Strt_Rule_A2()
    : UnsafeCondNopTesterA_25Is1_op1_24To20Is0x010_B_4Is0(
      state_.ForbiddenCondNop_Strt_Rule_A2_instance_)
  {}
};

class Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x011NotRnIsPc_Ldr_Rule_58_A1_P120
    : public LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x011NotRnIsPc {
 public:
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x011NotRnIsPc_Ldr_Rule_58_A1_P120()
    : LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x011NotRnIsPc(
      state_.Load2RegisterImm12Op_Ldr_Rule_58_A1_P120_instance_)
  {}
};

class Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x011_Ldr_Rule_59_A1_P122
    : public LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x011 {
 public:
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x011_Ldr_Rule_59_A1_P122()
    : LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x011(
      state_.Load2RegisterImm12Op_Ldr_Rule_59_A1_P122_instance_)
  {}
};

class Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x1_B_4Is0_Notop1_repeated_24To20Is0x011_Ldr_Rule_60_A1_P124
    : public LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x1_B_4Is0_Notop1_repeated_24To20Is0x011 {
 public:
  Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x1_B_4Is0_Notop1_repeated_24To20Is0x011_Ldr_Rule_60_A1_P124()
    : LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x1_B_4Is0_Notop1_repeated_24To20Is0x011(
      state_.Load3RegisterImm5Op_Ldr_Rule_60_A1_P124_instance_)
  {}
};

class ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x011_Ldrt_Rule_A1
    : public UnsafeCondNopTesterA_25Is0_op1_24To20Is0x011 {
 public:
  ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x011_Ldrt_Rule_A1()
    : UnsafeCondNopTesterA_25Is0_op1_24To20Is0x011(
      state_.ForbiddenCondNop_Ldrt_Rule_A1_instance_)
  {}
};

class ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x011_B_4Is0_Ldrt_Rule_A2
    : public UnsafeCondNopTesterA_25Is1_op1_24To20Is0x011_B_4Is0 {
 public:
  ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x011_B_4Is0_Ldrt_Rule_A2()
    : UnsafeCondNopTesterA_25Is1_op1_24To20Is0x011_B_4Is0(
      state_.ForbiddenCondNop_Ldrt_Rule_A2_instance_)
  {}
};

class Store2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x0_Notop1_repeated_24To20Is0x110_Strb_Rule_197_A1_P390
    : public LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x0_Notop1_repeated_24To20Is0x110 {
 public:
  Store2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x0_Notop1_repeated_24To20Is0x110_Strb_Rule_197_A1_P390()
    : LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x0_Notop1_repeated_24To20Is0x110(
      state_.Store2RegisterImm12Op_Strb_Rule_197_A1_P390_instance_)
  {}
};

class Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x0_B_4Is0_Notop1_repeated_24To20Is0x110_Strb_Rule_198_A1_P392
    : public LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x0_B_4Is0_Notop1_repeated_24To20Is0x110 {
 public:
  Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x0_B_4Is0_Notop1_repeated_24To20Is0x110_Strb_Rule_198_A1_P392()
    : LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x0_B_4Is0_Notop1_repeated_24To20Is0x110(
      state_.Store3RegisterImm5Op_Strb_Rule_198_A1_P392_instance_)
  {}
};

class ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x110_Strtb_Rule_A1
    : public UnsafeCondNopTesterA_25Is0_op1_24To20Is0x110 {
 public:
  ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x110_Strtb_Rule_A1()
    : UnsafeCondNopTesterA_25Is0_op1_24To20Is0x110(
      state_.ForbiddenCondNop_Strtb_Rule_A1_instance_)
  {}
};

class ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x110_B_4Is0_Strtb_Rule_A2
    : public UnsafeCondNopTesterA_25Is1_op1_24To20Is0x110_B_4Is0 {
 public:
  ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x110_B_4Is0_Strtb_Rule_A2()
    : UnsafeCondNopTesterA_25Is1_op1_24To20Is0x110_B_4Is0(
      state_.ForbiddenCondNop_Strtb_Rule_A2_instance_)
  {}
};

class Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x111NotRnIsPc_Ldrb_Rule_62_A1_P128
    : public LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x111NotRnIsPc {
 public:
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x111NotRnIsPc_Ldrb_Rule_62_A1_P128()
    : LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x111NotRnIsPc(
      state_.Load2RegisterImm12Op_Ldrb_Rule_62_A1_P128_instance_)
  {}
};

class Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x111_Ldrb_Rule_63_A1_P130
    : public LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x111 {
 public:
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x111_Ldrb_Rule_63_A1_P130()
    : LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x111(
      state_.Load2RegisterImm12Op_Ldrb_Rule_63_A1_P130_instance_)
  {}
};

class Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x1_B_4Is0_Notop1_repeated_24To20Is0x111_Ldrb_Rule_64_A1_P132
    : public LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x1_B_4Is0_Notop1_repeated_24To20Is0x111 {
 public:
  Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x1_B_4Is0_Notop1_repeated_24To20Is0x111_Ldrb_Rule_64_A1_P132()
    : LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x1_B_4Is0_Notop1_repeated_24To20Is0x111(
      state_.Load3RegisterImm5Op_Ldrb_Rule_64_A1_P132_instance_)
  {}
};

class ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x111_Ldrtb_Rule_A1
    : public UnsafeCondNopTesterA_25Is0_op1_24To20Is0x111 {
 public:
  ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x111_Ldrtb_Rule_A1()
    : UnsafeCondNopTesterA_25Is0_op1_24To20Is0x111(
      state_.ForbiddenCondNop_Ldrtb_Rule_A1_instance_)
  {}
};

class ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x111_B_4Is0_Ldrtb_Rule_A2
    : public UnsafeCondNopTesterA_25Is1_op1_24To20Is0x111_B_4Is0 {
 public:
  ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x111_B_4Is0_Ldrtb_Rule_A2()
    : UnsafeCondNopTesterA_25Is1_op1_24To20Is0x111_B_4Is0(
      state_.ForbiddenCondNop_Ldrtb_Rule_A2_instance_)
  {}
};

class Store2RegisterImm12OpRnNotRtOnWritebackTester_Flags_24To21Is1001_Rn_19To16Is1101_Imm12_11To0Is000000000100_Push_Rule_123_A2_P248
    : public LoadStore2RegisterImm12OpTesterFlags_24To21Is1001_Rn_19To16Is1101_Imm12_11To0Is000000000100 {
 public:
  Store2RegisterImm12OpRnNotRtOnWritebackTester_Flags_24To21Is1001_Rn_19To16Is1101_Imm12_11To0Is000000000100_Push_Rule_123_A2_P248()
    : LoadStore2RegisterImm12OpTesterFlags_24To21Is1001_Rn_19To16Is1101_Imm12_11To0Is000000000100(
      state_.Store2RegisterImm12OpRnNotRtOnWriteback_Push_Rule_123_A2_P248_instance_)
  {}
};

class Store2RegisterImm12OpTester_Notcccc010100101101tttt000000000100_Str_Rule_194_A1_P384
    : public LoadStore2RegisterImm12OpTesterNotcccc010100101101tttt000000000100 {
 public:
  Store2RegisterImm12OpTester_Notcccc010100101101tttt000000000100_Str_Rule_194_A1_P384()
    : LoadStore2RegisterImm12OpTesterNotcccc010100101101tttt000000000100(
      state_.Store2RegisterImm12Op_Str_Rule_194_A1_P384_instance_)
  {}
};

class Binary3RegisterOpAltATester_op1_24To20Is11000_op2_7To5Is000_Rd_15To12Is1111RegsNotPc_Usad8_Rule_253_A1_P500
    : public Binary3RegisterOpAltATesterop1_24To20Is11000_op2_7To5Is000_Rd_15To12Is1111RegsNotPc {
 public:
  Binary3RegisterOpAltATester_op1_24To20Is11000_op2_7To5Is000_Rd_15To12Is1111RegsNotPc_Usad8_Rule_253_A1_P500()
    : Binary3RegisterOpAltATesterop1_24To20Is11000_op2_7To5Is000_Rd_15To12Is1111RegsNotPc(
      state_.Binary3RegisterOpAltA_Usad8_Rule_253_A1_P500_instance_)
  {}
};

class Binary4RegisterDualOpTester_op1_24To20Is11000_op2_7To5Is000_NotRd_15To12Is1111RegsNotPc_Usada8_Rule_254_A1_P502
    : public Binary4RegisterDualOpTesterop1_24To20Is11000_op2_7To5Is000_NotRd_15To12Is1111RegsNotPc {
 public:
  Binary4RegisterDualOpTester_op1_24To20Is11000_op2_7To5Is000_NotRd_15To12Is1111RegsNotPc_Usada8_Rule_254_A1_P502()
    : Binary4RegisterDualOpTesterop1_24To20Is11000_op2_7To5Is000_NotRd_15To12Is1111RegsNotPc(
      state_.Binary4RegisterDualOp_Usada8_Rule_254_A1_P502_instance_)
  {}
};

class Binary2RegisterBitRangeNotRnIsPcTester_op1_24To20Is1101x_op2_7To5Isx10_Sbfx_Rule_154_A1_P308
    : public Binary2RegisterBitRangeNotRnIsPcTesterop1_24To20Is1101x_op2_7To5Isx10 {
 public:
  Binary2RegisterBitRangeNotRnIsPcTester_op1_24To20Is1101x_op2_7To5Isx10_Sbfx_Rule_154_A1_P308()
    : Binary2RegisterBitRangeNotRnIsPcTesterop1_24To20Is1101x_op2_7To5Isx10(
      state_.Binary2RegisterBitRangeNotRnIsPc_Sbfx_Rule_154_A1_P308_instance_)
  {}
};

class Unary1RegisterBitRangeTester_op1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111_Bfc_17_A1_P46
    : public Unary1RegisterBitRangeTesterop1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111 {
 public:
  Unary1RegisterBitRangeTester_op1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111_Bfc_17_A1_P46()
    : Unary1RegisterBitRangeTesterop1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111(
      state_.Unary1RegisterBitRange_Bfc_17_A1_P46_instance_)
  {}
};

class Binary2RegisterBitRangeTester_op1_24To20Is1110x_op2_7To5Isx00_NotRn_3To0Is1111_Bfi_Rule_18_A1_P48
    : public Binary2RegisterBitRangeTesterop1_24To20Is1110x_op2_7To5Isx00_NotRn_3To0Is1111 {
 public:
  Binary2RegisterBitRangeTester_op1_24To20Is1110x_op2_7To5Isx00_NotRn_3To0Is1111_Bfi_Rule_18_A1_P48()
    : Binary2RegisterBitRangeTesterop1_24To20Is1110x_op2_7To5Isx00_NotRn_3To0Is1111(
      state_.Binary2RegisterBitRange_Bfi_Rule_18_A1_P48_instance_)
  {}
};

class Binary2RegisterBitRangeNotRnIsPcTester_op1_24To20Is1111x_op2_7To5Isx10_Ubfx_Rule_236_A1_P466
    : public Binary2RegisterBitRangeNotRnIsPcTesterop1_24To20Is1111x_op2_7To5Isx10 {
 public:
  Binary2RegisterBitRangeNotRnIsPcTester_op1_24To20Is1111x_op2_7To5Isx10_Ubfx_Rule_236_A1_P466()
    : Binary2RegisterBitRangeNotRnIsPcTesterop1_24To20Is1111x_op2_7To5Isx10(
      state_.Binary2RegisterBitRangeNotRnIsPc_Ubfx_Rule_236_A1_P466_instance_)
  {}
};

class ForbiddenCondNopTester_op2_6To4Is000_B_9Is1_op_22To21Isx0_Msr_Rule_Banked_register_A1_B9_1990
    : public UnsafeCondNopTesterop2_6To4Is000_B_9Is1_op_22To21Isx0 {
 public:
  ForbiddenCondNopTester_op2_6To4Is000_B_9Is1_op_22To21Isx0_Msr_Rule_Banked_register_A1_B9_1990()
    : UnsafeCondNopTesterop2_6To4Is000_B_9Is1_op_22To21Isx0(
      state_.ForbiddenCondNop_Msr_Rule_Banked_register_A1_B9_1990_instance_)
  {}
};

class ForbiddenCondNopTester_op2_6To4Is000_B_9Is1_op_22To21Isx1_Msr_Rule_Banked_register_A1_B9_1992
    : public UnsafeCondNopTesterop2_6To4Is000_B_9Is1_op_22To21Isx1 {
 public:
  ForbiddenCondNopTester_op2_6To4Is000_B_9Is1_op_22To21Isx1_Msr_Rule_Banked_register_A1_B9_1992()
    : UnsafeCondNopTesterop2_6To4Is000_B_9Is1_op_22To21Isx1(
      state_.ForbiddenCondNop_Msr_Rule_Banked_register_A1_B9_1992_instance_)
  {}
};

class Unary1RegisterSetTester_op2_6To4Is000_B_9Is0_op_22To21Isx0_Mrs_Rule_102_A1_P206_Or_B6_10
    : public Unary1RegisterSetTesterop2_6To4Is000_B_9Is0_op_22To21Isx0 {
 public:
  Unary1RegisterSetTester_op2_6To4Is000_B_9Is0_op_22To21Isx0_Mrs_Rule_102_A1_P206_Or_B6_10()
    : Unary1RegisterSetTesterop2_6To4Is000_B_9Is0_op_22To21Isx0(
      state_.Unary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10_instance_)
  {}
};

class Unary1RegisterUseTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx00_Msr_Rule_104_A1_P210
    : public Unary1RegisterUseTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx00 {
 public:
  Unary1RegisterUseTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx00_Msr_Rule_104_A1_P210()
    : Unary1RegisterUseTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx00(
      state_.Unary1RegisterUse_Msr_Rule_104_A1_P210_instance_)
  {}
};

class ForbiddenCondNopTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01_Msr_Rule_B6_1_7_P14
    : public UnsafeCondNopTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01 {
 public:
  ForbiddenCondNopTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01_Msr_Rule_B6_1_7_P14()
    : UnsafeCondNopTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01(
      state_.ForbiddenCondNop_Msr_Rule_B6_1_7_P14_instance_)
  {}
};

class ForbiddenCondNopTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x_Msr_Rule_B6_1_7_P14
    : public UnsafeCondNopTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x {
 public:
  ForbiddenCondNopTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x_Msr_Rule_B6_1_7_P14()
    : UnsafeCondNopTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x(
      state_.ForbiddenCondNop_Msr_Rule_B6_1_7_P14_instance_)
  {}
};

class ForbiddenCondNopTester_op2_6To4Is000_B_9Is0_op_22To21Is11_Msr_Rule_B6_1_7_P14
    : public UnsafeCondNopTesterop2_6To4Is000_B_9Is0_op_22To21Is11 {
 public:
  ForbiddenCondNopTester_op2_6To4Is000_B_9Is0_op_22To21Is11_Msr_Rule_B6_1_7_P14()
    : UnsafeCondNopTesterop2_6To4Is000_B_9Is0_op_22To21Is11(
      state_.ForbiddenCondNop_Msr_Rule_B6_1_7_P14_instance_)
  {}
};

class BranchToRegisterTester_op2_6To4Is001_op_22To21Is01_Bx_Rule_25_A1_P62
    : public BranchToRegisterTesterop2_6To4Is001_op_22To21Is01 {
 public:
  BranchToRegisterTester_op2_6To4Is001_op_22To21Is01_Bx_Rule_25_A1_P62()
    : BranchToRegisterTesterop2_6To4Is001_op_22To21Is01(
      state_.BranchToRegister_Bx_Rule_25_A1_P62_instance_)
  {}
};

class Unary2RegisterOpNotRmIsPcTester_op2_6To4Is001_op_22To21Is11_Clz_Rule_31_A1_P72
    : public Unary2RegisterOpNotRmIsPcTesterop2_6To4Is001_op_22To21Is11 {
 public:
  Unary2RegisterOpNotRmIsPcTester_op2_6To4Is001_op_22To21Is11_Clz_Rule_31_A1_P72()
    : Unary2RegisterOpNotRmIsPcTesterop2_6To4Is001_op_22To21Is11(
      state_.Unary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72_instance_)
  {}
};

class ForbiddenCondNopTester_op2_6To4Is010_op_22To21Is01_Bxj_Rule_26_A1_P64
    : public UnsafeCondNopTesterop2_6To4Is010_op_22To21Is01 {
 public:
  ForbiddenCondNopTester_op2_6To4Is010_op_22To21Is01_Bxj_Rule_26_A1_P64()
    : UnsafeCondNopTesterop2_6To4Is010_op_22To21Is01(
      state_.ForbiddenCondNop_Bxj_Rule_26_A1_P64_instance_)
  {}
};

class BranchToRegisterTester_op2_6To4Is011_op_22To21Is01RegsNotPc_Blx_Rule_24_A1_P60
    : public BranchToRegisterTesterop2_6To4Is011_op_22To21Is01RegsNotPc {
 public:
  BranchToRegisterTester_op2_6To4Is011_op_22To21Is01RegsNotPc_Blx_Rule_24_A1_P60()
    : BranchToRegisterTesterop2_6To4Is011_op_22To21Is01RegsNotPc(
      state_.BranchToRegister_Blx_Rule_24_A1_P60_instance_)
  {}
};

class ForbiddenCondNopTester_op2_6To4Is110_op_22To21Is11_Eret_Rule_A1
    : public UnsafeCondNopTesterop2_6To4Is110_op_22To21Is11 {
 public:
  ForbiddenCondNopTester_op2_6To4Is110_op_22To21Is11_Eret_Rule_A1()
    : UnsafeCondNopTesterop2_6To4Is110_op_22To21Is11(
      state_.ForbiddenCondNop_Eret_Rule_A1_instance_)
  {}
};

class BreakPointAndConstantPoolHeadTester_op2_6To4Is111_op_22To21Is01_Bkpt_Rule_22_A1_P56
    : public Immediate16UseTesterop2_6To4Is111_op_22To21Is01 {
 public:
  BreakPointAndConstantPoolHeadTester_op2_6To4Is111_op_22To21Is01_Bkpt_Rule_22_A1_P56()
    : Immediate16UseTesterop2_6To4Is111_op_22To21Is01(
      state_.BreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56_instance_)
  {}
};

class ForbiddenCondNopTester_op2_6To4Is111_op_22To21Is10_Hvc_Rule_A1
    : public UnsafeCondNopTesterop2_6To4Is111_op_22To21Is10 {
 public:
  ForbiddenCondNopTester_op2_6To4Is111_op_22To21Is10_Hvc_Rule_A1()
    : UnsafeCondNopTesterop2_6To4Is111_op_22To21Is10(
      state_.ForbiddenCondNop_Hvc_Rule_A1_instance_)
  {}
};

class ForbiddenCondNopTester_op2_6To4Is111_op_22To21Is11_Smc_Rule_B6_1_9_P18
    : public UnsafeCondNopTesterop2_6To4Is111_op_22To21Is11 {
 public:
  ForbiddenCondNopTester_op2_6To4Is111_op_22To21Is11_Smc_Rule_B6_1_9_P18()
    : UnsafeCondNopTesterop2_6To4Is111_op_22To21Is11(
      state_.ForbiddenCondNop_Smc_Rule_B6_1_9_P18_instance_)
  {}
};

class CondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000000_Nop_Rule_110_A1_P222
    : public CondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000000 {
 public:
  CondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000000_Nop_Rule_110_A1_P222()
    : CondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000000(
      state_.CondNop_Nop_Rule_110_A1_P222_instance_)
  {}
};

class CondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000001_Yield_Rule_413_A1_P812
    : public CondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000001 {
 public:
  CondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000001_Yield_Rule_413_A1_P812()
    : CondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000001(
      state_.CondNop_Yield_Rule_413_A1_P812_instance_)
  {}
};

class ForbiddenCondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000010_Wfe_Rule_411_A1_P808
    : public UnsafeCondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000010 {
 public:
  ForbiddenCondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000010_Wfe_Rule_411_A1_P808()
    : UnsafeCondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000010(
      state_.ForbiddenCondNop_Wfe_Rule_411_A1_P808_instance_)
  {}
};

class ForbiddenCondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000011_Wfi_Rule_412_A1_P810
    : public UnsafeCondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000011 {
 public:
  ForbiddenCondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000011_Wfi_Rule_412_A1_P810()
    : UnsafeCondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000011(
      state_.ForbiddenCondNop_Wfi_Rule_412_A1_P810_instance_)
  {}
};

class ForbiddenCondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000100_Sev_Rule_158_A1_P316
    : public UnsafeCondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000100 {
 public:
  ForbiddenCondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000100_Sev_Rule_158_A1_P316()
    : UnsafeCondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000100(
      state_.ForbiddenCondNop_Sev_Rule_158_A1_P316_instance_)
  {}
};

class CondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx_Dbg_Rule_40_A1_P88
    : public CondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx {
 public:
  CondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx_Dbg_Rule_40_A1_P88()
    : CondNopTesterop_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx(
      state_.CondNop_Dbg_Rule_40_A1_P88_instance_)
  {}
};

class MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is0100_Msr_Rule_103_A1_P208
    : public MoveImmediate12ToApsrTesterop_22Is0_op1_19To16Is0100 {
 public:
  MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is0100_Msr_Rule_103_A1_P208()
    : MoveImmediate12ToApsrTesterop_22Is0_op1_19To16Is0100(
      state_.MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_)
  {}
};

class MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is1x00_Msr_Rule_103_A1_P208
    : public MoveImmediate12ToApsrTesterop_22Is0_op1_19To16Is1x00 {
 public:
  MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is1x00_Msr_Rule_103_A1_P208()
    : MoveImmediate12ToApsrTesterop_22Is0_op1_19To16Is1x00(
      state_.MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_)
  {}
};

class ForbiddenCondNopTester_op_22Is0_op1_19To16Isxx01_Msr_Rule_B6_1_6_A1_PB6_12
    : public UnsafeCondNopTesterop_22Is0_op1_19To16Isxx01 {
 public:
  ForbiddenCondNopTester_op_22Is0_op1_19To16Isxx01_Msr_Rule_B6_1_6_A1_PB6_12()
    : UnsafeCondNopTesterop_22Is0_op1_19To16Isxx01(
      state_.ForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12_instance_)
  {}
};

class ForbiddenCondNopTester_op_22Is0_op1_19To16Isxx1x_Msr_Rule_B6_1_6_A1_PB6_12
    : public UnsafeCondNopTesterop_22Is0_op1_19To16Isxx1x {
 public:
  ForbiddenCondNopTester_op_22Is0_op1_19To16Isxx1x_Msr_Rule_B6_1_6_A1_PB6_12()
    : UnsafeCondNopTesterop_22Is0_op1_19To16Isxx1x(
      state_.ForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12_instance_)
  {}
};

class ForbiddenCondNopTester_op_22Is1_Msr_Rule_B6_1_6_A1_PB6_12
    : public UnsafeCondNopTesterop_22Is1 {
 public:
  ForbiddenCondNopTester_op_22Is1_Msr_Rule_B6_1_6_A1_PB6_12()
    : UnsafeCondNopTesterop_22Is1(
      state_.ForbiddenCondNop_Msr_Rule_B6_1_6_A1_PB6_12_instance_)
  {}
};

class Binary3RegisterOpAltATester_op_23To20Is000xRegsNotPc_Mul_Rule_105_A1_P212
    : public Binary3RegisterOpAltATesterop_23To20Is000xRegsNotPc {
 public:
  Binary3RegisterOpAltATester_op_23To20Is000xRegsNotPc_Mul_Rule_105_A1_P212()
    : Binary3RegisterOpAltATesterop_23To20Is000xRegsNotPc(
      state_.Binary3RegisterOpAltA_Mul_Rule_105_A1_P212_instance_)
  {}
};

class Binary4RegisterDualOpTester_op_23To20Is001xRegsNotPc_Mla_Rule_94_A1_P190
    : public Binary4RegisterDualOpTesterop_23To20Is001xRegsNotPc {
 public:
  Binary4RegisterDualOpTester_op_23To20Is001xRegsNotPc_Mla_Rule_94_A1_P190()
    : Binary4RegisterDualOpTesterop_23To20Is001xRegsNotPc(
      state_.Binary4RegisterDualOp_Mla_Rule_94_A1_P190_instance_)
  {}
};

class Binary4RegisterDualResultTester_op_23To20Is0100RegsNotPc_Umaal_Rule_244_A1_P482
    : public Binary4RegisterDualResultTesterop_23To20Is0100RegsNotPc {
 public:
  Binary4RegisterDualResultTester_op_23To20Is0100RegsNotPc_Umaal_Rule_244_A1_P482()
    : Binary4RegisterDualResultTesterop_23To20Is0100RegsNotPc(
      state_.Binary4RegisterDualResult_Umaal_Rule_244_A1_P482_instance_)
  {}
};

class Binary4RegisterDualOpTester_op_23To20Is0110RegsNotPc_Mls_Rule_95_A1_P192
    : public Binary4RegisterDualOpTesterop_23To20Is0110RegsNotPc {
 public:
  Binary4RegisterDualOpTester_op_23To20Is0110RegsNotPc_Mls_Rule_95_A1_P192()
    : Binary4RegisterDualOpTesterop_23To20Is0110RegsNotPc(
      state_.Binary4RegisterDualOp_Mls_Rule_95_A1_P192_instance_)
  {}
};

class Binary4RegisterDualResultTester_op_23To20Is100xRegsNotPc_Umull_Rule_246_A1_P486
    : public Binary4RegisterDualResultTesterop_23To20Is100xRegsNotPc {
 public:
  Binary4RegisterDualResultTester_op_23To20Is100xRegsNotPc_Umull_Rule_246_A1_P486()
    : Binary4RegisterDualResultTesterop_23To20Is100xRegsNotPc(
      state_.Binary4RegisterDualResult_Umull_Rule_246_A1_P486_instance_)
  {}
};

class Binary4RegisterDualResultTester_op_23To20Is101xRegsNotPc_Umlal_Rule_245_A1_P484
    : public Binary4RegisterDualResultTesterop_23To20Is101xRegsNotPc {
 public:
  Binary4RegisterDualResultTester_op_23To20Is101xRegsNotPc_Umlal_Rule_245_A1_P484()
    : Binary4RegisterDualResultTesterop_23To20Is101xRegsNotPc(
      state_.Binary4RegisterDualResult_Umlal_Rule_245_A1_P484_instance_)
  {}
};

class Binary4RegisterDualResultTester_op_23To20Is110xRegsNotPc_Smull_Rule_179_A1_P356
    : public Binary4RegisterDualResultTesterop_23To20Is110xRegsNotPc {
 public:
  Binary4RegisterDualResultTester_op_23To20Is110xRegsNotPc_Smull_Rule_179_A1_P356()
    : Binary4RegisterDualResultTesterop_23To20Is110xRegsNotPc(
      state_.Binary4RegisterDualResult_Smull_Rule_179_A1_P356_instance_)
  {}
};

class Binary4RegisterDualResultTester_op_23To20Is111xRegsNotPc_Smlal_Rule_168_A1_P334
    : public Binary4RegisterDualResultTesterop_23To20Is111xRegsNotPc {
 public:
  Binary4RegisterDualResultTester_op_23To20Is111xRegsNotPc_Smlal_Rule_168_A1_P334()
    : Binary4RegisterDualResultTesterop_23To20Is111xRegsNotPc(
      state_.Binary4RegisterDualResult_Smlal_Rule_168_A1_P334_instance_)
  {}
};

class CondVfpOpTester_opc3_7To6Isx0_Vmov_Rule_326_A2_P640
    : public CondVfpOpTesteropc3_7To6Isx0 {
 public:
  CondVfpOpTester_opc3_7To6Isx0_Vmov_Rule_326_A2_P640()
    : CondVfpOpTesteropc3_7To6Isx0(
      state_.CondVfpOp_Vmov_Rule_326_A2_P640_instance_)
  {}
};

class CondVfpOpTester_opc2_19To16Is0000_opc3_7To6Is01_Vmov_Rule_327_A2_P642
    : public CondVfpOpTesteropc2_19To16Is0000_opc3_7To6Is01 {
 public:
  CondVfpOpTester_opc2_19To16Is0000_opc3_7To6Is01_Vmov_Rule_327_A2_P642()
    : CondVfpOpTesteropc2_19To16Is0000_opc3_7To6Is01(
      state_.CondVfpOp_Vmov_Rule_327_A2_P642_instance_)
  {}
};

class CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is01_Vneg_Rule_342_A2_P672
    : public CondVfpOpTesteropc2_19To16Is0001_opc3_7To6Is01 {
 public:
  CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is01_Vneg_Rule_342_A2_P672()
    : CondVfpOpTesteropc2_19To16Is0001_opc3_7To6Is01(
      state_.CondVfpOp_Vneg_Rule_342_A2_P672_instance_)
  {}
};

class CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is11_Vsqrt_Rule_388_A1_P762
    : public CondVfpOpTesteropc2_19To16Is0001_opc3_7To6Is11 {
 public:
  CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is11_Vsqrt_Rule_388_A1_P762()
    : CondVfpOpTesteropc2_19To16Is0001_opc3_7To6Is11(
      state_.CondVfpOp_Vsqrt_Rule_388_A1_P762_instance_)
  {}
};

class CondVfpOpTester_opc2_19To16Is001x_opc3_7To6Isx1_Vcvtb_Vcvtt_Rule_300_A1_P588
    : public CondVfpOpTesteropc2_19To16Is001x_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc2_19To16Is001x_opc3_7To6Isx1_Vcvtb_Vcvtt_Rule_300_A1_P588()
    : CondVfpOpTesteropc2_19To16Is001x_opc3_7To6Isx1(
      state_.CondVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588_instance_)
  {}
};

class CondVfpOpTester_opc2_19To16Is0100_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_292_A1_P572
    : public CondVfpOpTesteropc2_19To16Is0100_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc2_19To16Is0100_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_292_A1_P572()
    : CondVfpOpTesteropc2_19To16Is0100_opc3_7To6Isx1(
      state_.CondVfpOp_Vcmp_Vcmpe_Rule_292_A1_P572_instance_)
  {}
};

class CondVfpOpTester_opc2_19To16Is0101_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_292_A2_P572
    : public CondVfpOpTesteropc2_19To16Is0101_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc2_19To16Is0101_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_292_A2_P572()
    : CondVfpOpTesteropc2_19To16Is0101_opc3_7To6Isx1(
      state_.CondVfpOp_Vcmp_Vcmpe_Rule_292_A2_P572_instance_)
  {}
};

class CondVfpOpTester_opc2_19To16Is0111_opc3_7To6Is11_Vcvt_Rule_298_A1_P584
    : public CondVfpOpTesteropc2_19To16Is0111_opc3_7To6Is11 {
 public:
  CondVfpOpTester_opc2_19To16Is0111_opc3_7To6Is11_Vcvt_Rule_298_A1_P584()
    : CondVfpOpTesteropc2_19To16Is0111_opc3_7To6Is11(
      state_.CondVfpOp_Vcvt_Rule_298_A1_P584_instance_)
  {}
};

class CondVfpOpTester_opc2_19To16Is1000_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578
    : public CondVfpOpTesteropc2_19To16Is1000_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc2_19To16Is1000_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578()
    : CondVfpOpTesteropc2_19To16Is1000_opc3_7To6Isx1(
      state_.CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578_instance_)
  {}
};

class CondVfpOpTester_opc2_19To16Is101x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582
    : public CondVfpOpTesteropc2_19To16Is101x_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc2_19To16Is101x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582()
    : CondVfpOpTesteropc2_19To16Is101x_opc3_7To6Isx1(
      state_.CondVfpOp_Vcvt_Rule_297_A1_P582_instance_)
  {}
};

class CondVfpOpTester_opc2_19To16Is110x_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578
    : public CondVfpOpTesteropc2_19To16Is110x_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc2_19To16Is110x_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578()
    : CondVfpOpTesteropc2_19To16Is110x_opc3_7To6Isx1(
      state_.CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578_instance_)
  {}
};

class CondVfpOpTester_opc2_19To16Is111x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582
    : public CondVfpOpTesteropc2_19To16Is111x_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc2_19To16Is111x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582()
    : CondVfpOpTesteropc2_19To16Is111x_opc3_7To6Isx1(
      state_.CondVfpOp_Vcvt_Rule_297_A1_P582_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Isxx0RegsNotPc_Pkh_Rule_116_A1_P234
    : public Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Isxx0RegsNotPc {
 public:
  Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Isxx0RegsNotPc_Pkh_Rule_116_A1_P234()
    : Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Isxx0RegsNotPc(
      state_.Binary3RegisterImmedShiftedOpRegsNotPc_Pkh_Rule_116_A1_P234_instance_)
  {}
};

class Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is01x_op2_7To5Isxx0_Ssat_Rule_183_A1_P362
    : public Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is01x_op2_7To5Isxx0 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is01x_op2_7To5Isxx0_Ssat_Rule_183_A1_P362()
    : Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is01x_op2_7To5Isxx0(
      state_.Unary2RegisterSatImmedShiftedOp_Ssat_Rule_183_A1_P362_instance_)
  {}
};

class Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is11x_op2_7To5Isxx0_Usat_Rule_255_A1_P504
    : public Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is11x_op2_7To5Isxx0 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is11x_op2_7To5Isxx0_Usat_Rule_255_A1_P504()
    : Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is11x_op2_7To5Isxx0(
      state_.Unary2RegisterSatImmedShiftedOp_Usat_Rule_255_A1_P504_instance_)
  {}
};

class Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc_Sxtab16_Rule_221_A1_P436
    : public Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc {
 public:
  Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc_Sxtab16_Rule_221_A1_P436()
    : Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc(
      state_.Binary3RegisterImmedShiftedOpRegsNotPc_Sxtab16_Rule_221_A1_P436_instance_)
  {}
};

class Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_A_19To16Is1111_Sxtb16_Rule_224_A1_P442
    : public Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is000_op2_7To5Is011_A_19To16Is1111 {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_A_19To16Is1111_Sxtb16_Rule_224_A1_P442()
    : Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is000_op2_7To5Is011_A_19To16Is1111(
      state_.Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb16_Rule_224_A1_P442_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op1_22To20Is000_op2_7To5Is101RegsNotPc_Sel_Rule_156_A1_P312
    : public Binary3RegisterOpAltBTesterop1_22To20Is000_op2_7To5Is101RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op1_22To20Is000_op2_7To5Is101RegsNotPc_Sel_Rule_156_A1_P312()
    : Binary3RegisterOpAltBTesterop1_22To20Is000_op2_7To5Is101RegsNotPc(
      state_.Binary3RegisterOpAltB_Sel_Rule_156_A1_P312_instance_)
  {}
};

class Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is010_op2_7To5Is001_Ssat16_Rule_184_A1_P364
    : public Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is010_op2_7To5Is001 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is010_op2_7To5Is001_Ssat16_Rule_184_A1_P364()
    : Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is010_op2_7To5Is001(
      state_.Unary2RegisterSatImmedShiftedOp_Ssat16_Rule_184_A1_P364_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op1_22To20Is010_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc_Sxtab_Rule_220_A1_P434
    : public Binary3RegisterOpAltBTesterop1_22To20Is010_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op1_22To20Is010_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc_Sxtab_Rule_220_A1_P434()
    : Binary3RegisterOpAltBTesterop1_22To20Is010_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc(
      state_.Binary3RegisterOpAltB_Sxtab_Rule_220_A1_P434_instance_)
  {}
};

class Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is010_op2_7To5Is011_A_19To16Is1111_Sxtb_Rule_223_A1_P440
    : public Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is010_op2_7To5Is011_A_19To16Is1111 {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is010_op2_7To5Is011_A_19To16Is1111_Sxtb_Rule_223_A1_P440()
    : Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is010_op2_7To5Is011_A_19To16Is1111(
      state_.Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb_Rule_223_A1_P440_instance_)
  {}
};

class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is001_Rev_Rule_135_A1_P272
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is001 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is001_Rev_Rule_135_A1_P272()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is001(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev_Rule_135_A1_P272_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Sxtah_Rule_222_A1_P438
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is011_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Sxtah_Rule_222_A1_P438()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is011_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sxtah_Rule_222_A1_P438_instance_)
  {}
};

class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_A_19To16Is1111_Sxth_Rule_225_A1_P444
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is011_A_19To16Is1111 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_A_19To16Is1111_Sxth_Rule_225_A1_P444()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is011_A_19To16Is1111(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Sxth_Rule_225_A1_P444_instance_)
  {}
};

class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is101_Rev16_Rule_136_A1_P274
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is101 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is101_Rev16_Rule_136_A1_P274()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is101(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev16_Rule_136_A1_P274_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is100_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Uxtah_Rule_262_A1_P516
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is100_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is100_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Uxtah_Rule_262_A1_P516()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is100_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uxtah_Rule_262_A1_P516_instance_)
  {}
};

class Unary2RegisterOpNotRmIsPcTester_op1_22To20Is100_op2_7To5Is011_A_19To16Is1111_Uxtb16_Rule_264_A1_P522
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is100_op2_7To5Is011_A_19To16Is1111 {
 public:
  Unary2RegisterOpNotRmIsPcTester_op1_22To20Is100_op2_7To5Is011_A_19To16Is1111_Uxtb16_Rule_264_A1_P522()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is100_op2_7To5Is011_A_19To16Is1111(
      state_.Unary2RegisterOpNotRmIsPc_Uxtb16_Rule_264_A1_P522_instance_)
  {}
};

class Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is110_op2_7To5Is001_Usat16_Rule_256_A1_P506
    : public Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is110_op2_7To5Is001 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is110_op2_7To5Is001_Usat16_Rule_256_A1_P506()
    : Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is110_op2_7To5Is001(
      state_.Unary2RegisterSatImmedShiftedOp_Usat16_Rule_256_A1_P506_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is110_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Uxtab_Rule_260_A1_P514
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is110_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is110_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Uxtab_Rule_260_A1_P514()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is110_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uxtab_Rule_260_A1_P514_instance_)
  {}
};

class Unary2RegisterOpNotRmIsPcTester_op1_22To20Is110_op2_7To5Is011_A_19To16Is1111_Uxtb_Rule_263_A1_P520
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is110_op2_7To5Is011_A_19To16Is1111 {
 public:
  Unary2RegisterOpNotRmIsPcTester_op1_22To20Is110_op2_7To5Is011_A_19To16Is1111_Uxtb_Rule_263_A1_P520()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is110_op2_7To5Is011_A_19To16Is1111(
      state_.Unary2RegisterOpNotRmIsPc_Uxtb_Rule_263_A1_P520_instance_)
  {}
};

class Unary2RegisterOpNotRmIsPcTester_op1_22To20Is111_op2_7To5Is001_Rbit_Rule_134_A1_P270
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is001 {
 public:
  Unary2RegisterOpNotRmIsPcTester_op1_22To20Is111_op2_7To5Is001_Rbit_Rule_134_A1_P270()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is001(
      state_.Unary2RegisterOpNotRmIsPc_Rbit_Rule_134_A1_P270_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Uxtah_Rule_262_A1_P518
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is111_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Uxtah_Rule_262_A1_P518()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is111_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uxtah_Rule_262_A1_P518_instance_)
  {}
};

class Unary2RegisterOpNotRmIsPcTester_op1_22To20Is111_op2_7To5Is011_A_19To16Is1111_Uxth_Rule_265_A1_P524
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is011_A_19To16Is1111 {
 public:
  Unary2RegisterOpNotRmIsPcTester_op1_22To20Is111_op2_7To5Is011_A_19To16Is1111_Uxth_Rule_265_A1_P524()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is011_A_19To16Is1111(
      state_.Unary2RegisterOpNotRmIsPc_Uxth_Rule_265_A1_P524_instance_)
  {}
};

class Unary2RegisterOpNotRmIsPcTester_op1_22To20Is111_op2_7To5Is101_Revsh_Rule_137_A1_P276
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is101 {
 public:
  Unary2RegisterOpNotRmIsPcTester_op1_22To20Is111_op2_7To5Is101_Revsh_Rule_137_A1_P276()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is101(
      state_.Unary2RegisterOpNotRmIsPc_Revsh_Rule_137_A1_P276_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is000RegsNotPc_Sadd16_Rule_148_A1_P296
    : public Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is000RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is000RegsNotPc_Sadd16_Rule_148_A1_P296()
    : Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is000RegsNotPc(
      state_.Binary3RegisterOpAltB_Sadd16_Rule_148_A1_P296_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is001RegsNotPc_Sasx_Rule_150_A1_P300
    : public Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is001RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is001RegsNotPc_Sasx_Rule_150_A1_P300()
    : Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is001RegsNotPc(
      state_.Binary3RegisterOpAltB_Sasx_Rule_150_A1_P300_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is010RegsNotPc_Ssax_Rule_185_A1_P366
    : public Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is010RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is010RegsNotPc_Ssax_Rule_185_A1_P366()
    : Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is010RegsNotPc(
      state_.Binary3RegisterOpAltB_Ssax_Rule_185_A1_P366_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is011RegsNotPc_Ssub16_Rule_186_A1_P368
    : public Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is011RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is011RegsNotPc_Ssub16_Rule_186_A1_P368()
    : Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is011RegsNotPc(
      state_.Binary3RegisterOpAltB_Ssub16_Rule_186_A1_P368_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is100RegsNotPc_Ssad8_Rule_149_A1_P298
    : public Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is100RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is100RegsNotPc_Ssad8_Rule_149_A1_P298()
    : Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is100RegsNotPc(
      state_.Binary3RegisterOpAltB_Ssad8_Rule_149_A1_P298_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is111RegsNotPc_Ssub8_Rule_187_A1_P370
    : public Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is111RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is111RegsNotPc_Ssub8_Rule_187_A1_P370()
    : Binary3RegisterOpAltBTesterop1_21To20Is01_op2_7To5Is111RegsNotPc(
      state_.Binary3RegisterOpAltB_Ssub8_Rule_187_A1_P370_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is000RegsNotPc_Qadd16_Rule_125_A1_P252
    : public Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is000RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is000RegsNotPc_Qadd16_Rule_125_A1_P252()
    : Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is000RegsNotPc(
      state_.Binary3RegisterOpAltB_Qadd16_Rule_125_A1_P252_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is001RegsNotPc_Qasx_Rule_127_A1_P256
    : public Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is001RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is001RegsNotPc_Qasx_Rule_127_A1_P256()
    : Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is001RegsNotPc(
      state_.Binary3RegisterOpAltB_Qasx_Rule_127_A1_P256_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is010RegsNotPc_Qsax_Rule_130_A1_P262
    : public Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is010RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is010RegsNotPc_Qsax_Rule_130_A1_P262()
    : Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is010RegsNotPc(
      state_.Binary3RegisterOpAltB_Qsax_Rule_130_A1_P262_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is011RegsNotPc_Qsub16_Rule_132_A1_P266
    : public Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is011RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is011RegsNotPc_Qsub16_Rule_132_A1_P266()
    : Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is011RegsNotPc(
      state_.Binary3RegisterOpAltB_Qsub16_Rule_132_A1_P266_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is100RegsNotPc_Qadd8_Rule_126_A1_P254
    : public Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is100RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is100RegsNotPc_Qadd8_Rule_126_A1_P254()
    : Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is100RegsNotPc(
      state_.Binary3RegisterOpAltB_Qadd8_Rule_126_A1_P254_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is111RegsNotPc_Qsub8_Rule_133_A1_P268
    : public Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is111RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is111RegsNotPc_Qsub8_Rule_133_A1_P268()
    : Binary3RegisterOpAltBTesterop1_21To20Is10_op2_7To5Is111RegsNotPc(
      state_.Binary3RegisterOpAltB_Qsub8_Rule_133_A1_P268_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is000RegsNotPc_Shadd16_Rule_159_A1_P318
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is000RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is000RegsNotPc_Shadd16_Rule_159_A1_P318()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is000RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is001RegsNotPc_Shasx_Rule_161_A1_P322
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is001RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is001RegsNotPc_Shasx_Rule_161_A1_P322()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is001RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is010RegsNotPc_Shsax_Rule_162_A1_P324
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is010RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is010RegsNotPc_Shsax_Rule_162_A1_P324()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is010RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is011RegsNotPc_Shsub16_Rule_163_A1_P326
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is011RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is011RegsNotPc_Shsub16_Rule_163_A1_P326()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is011RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is100RegsNotPc_Shadd8_Rule_160_A1_P320
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is100RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is100RegsNotPc_Shadd8_Rule_160_A1_P320()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is100RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320_instance_)
  {}
};

class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is111RegsNotPc_Shsub8_Rule_164_A1_P328
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is111RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is111RegsNotPc_Shsub8_Rule_164_A1_P328()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is111RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op_22To21Is00RegsNotPc_Qadd_Rule_124_A1_P250
    : public Binary3RegisterOpAltBTesterop_22To21Is00RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op_22To21Is00RegsNotPc_Qadd_Rule_124_A1_P250()
    : Binary3RegisterOpAltBTesterop_22To21Is00RegsNotPc(
      state_.Binary3RegisterOpAltB_Qadd_Rule_124_A1_P250_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op_22To21Is01RegsNotPc_Qsub_Rule_131_A1_P264
    : public Binary3RegisterOpAltBTesterop_22To21Is01RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op_22To21Is01RegsNotPc_Qsub_Rule_131_A1_P264()
    : Binary3RegisterOpAltBTesterop_22To21Is01RegsNotPc(
      state_.Binary3RegisterOpAltB_Qsub_Rule_131_A1_P264_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op_22To21Is10RegsNotPc_Qdadd_Rule_128_A1_P258
    : public Binary3RegisterOpAltBTesterop_22To21Is10RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op_22To21Is10RegsNotPc_Qdadd_Rule_128_A1_P258()
    : Binary3RegisterOpAltBTesterop_22To21Is10RegsNotPc(
      state_.Binary3RegisterOpAltB_Qdadd_Rule_128_A1_P258_instance_)
  {}
};

class Binary3RegisterOpAltBTester_op_22To21Is11RegsNotPc_Qdsub_Rule_129_A1_P260
    : public Binary3RegisterOpAltBTesterop_22To21Is11RegsNotPc {
 public:
  Binary3RegisterOpAltBTester_op_22To21Is11RegsNotPc_Qdsub_Rule_129_A1_P260()
    : Binary3RegisterOpAltBTesterop_22To21Is11RegsNotPc(
      state_.Binary3RegisterOpAltB_Qdsub_Rule_129_A1_P260_instance_)
  {}
};

class Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc_Smlad_Rule_167_P332
    : public Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc {
 public:
  Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc_Smlad_Rule_167_P332()
    : Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc(
      state_.Binary4RegisterDualOp_Smlad_Rule_167_P332_instance_)
  {}
};

class Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smuad_Rule_177_P352
    : public Binary3RegisterOpAltATesterop1_22To20Is000_op2_7To5Is00x_A_15To12Is1111RegsNotPc {
 public:
  Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smuad_Rule_177_P352()
    : Binary3RegisterOpAltATesterop1_22To20Is000_op2_7To5Is00x_A_15To12Is1111RegsNotPc(
      state_.Binary3RegisterOpAltA_Smuad_Rule_177_P352_instance_)
  {}
};

class Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is01x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc_Smlsd_Rule_172_P342
    : public Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is01x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc {
 public:
  Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is01x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc_Smlsd_Rule_172_P342()
    : Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is01x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc(
      state_.Binary4RegisterDualOp_Smlsd_Rule_172_P342_instance_)
  {}
};

class Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is01x_A_15To12Is1111RegsNotPc_Smusd_Rule_181_P360
    : public Binary3RegisterOpAltATesterop1_22To20Is000_op2_7To5Is01x_A_15To12Is1111RegsNotPc {
 public:
  Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is01x_A_15To12Is1111RegsNotPc_Smusd_Rule_181_P360()
    : Binary3RegisterOpAltATesterop1_22To20Is000_op2_7To5Is01x_A_15To12Is1111RegsNotPc(
      state_.Binary3RegisterOpAltA_Smusd_Rule_181_P360_instance_)
  {}
};

class Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is00xRegsNotPc_Smlald_Rule_170_P336
    : public Binary4RegisterDualResultTesterop1_22To20Is100_op2_7To5Is00xRegsNotPc {
 public:
  Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is00xRegsNotPc_Smlald_Rule_170_P336()
    : Binary4RegisterDualResultTesterop1_22To20Is100_op2_7To5Is00xRegsNotPc(
      state_.Binary4RegisterDualResult_Smlald_Rule_170_P336_instance_)
  {}
};

class Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is01xRegsNotPc_Smlsld_Rule_173_P344
    : public Binary4RegisterDualResultTesterop1_22To20Is100_op2_7To5Is01xRegsNotPc {
 public:
  Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is01xRegsNotPc_Smlsld_Rule_173_P344()
    : Binary4RegisterDualResultTesterop1_22To20Is100_op2_7To5Is01xRegsNotPc(
      state_.Binary4RegisterDualResult_Smlsld_Rule_173_P344_instance_)
  {}
};

class Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc_Smmla_Rule_174_P346
    : public Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc {
 public:
  Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc_Smmla_Rule_174_P346()
    : Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc(
      state_.Binary4RegisterDualOp_Smmla_Rule_174_P346_instance_)
  {}
};

class Binary3RegisterOpAltATester_op1_22To20Is101_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smmul_Rule_176_P350
    : public Binary3RegisterOpAltATesterop1_22To20Is101_op2_7To5Is00x_A_15To12Is1111RegsNotPc {
 public:
  Binary3RegisterOpAltATester_op1_22To20Is101_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smmul_Rule_176_P350()
    : Binary3RegisterOpAltATesterop1_22To20Is101_op2_7To5Is00x_A_15To12Is1111RegsNotPc(
      state_.Binary3RegisterOpAltA_Smmul_Rule_176_P350_instance_)
  {}
};

class Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is11xRegsNotPc_Smmls_Rule_175_P348
    : public Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is11xRegsNotPc {
 public:
  Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is11xRegsNotPc_Smmls_Rule_175_P348()
    : Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is11xRegsNotPc(
      state_.Binary4RegisterDualOp_Smmls_Rule_175_P348_instance_)
  {}
};

class DeprecatedTester_op_23To20Is0x00_Swp_Swpb_Rule_A1
    : public UnsafeCondNopTesterop_23To20Is0x00 {
 public:
  DeprecatedTester_op_23To20Is0x00_Swp_Swpb_Rule_A1()
    : UnsafeCondNopTesterop_23To20Is0x00(
      state_.Deprecated_Swp_Swpb_Rule_A1_instance_)
  {}
};

class StoreExclusive3RegisterOpTester_op_23To20Is1000_Strex_Rule_202_A1_P400
    : public StoreExclusive3RegisterOpTesterop_23To20Is1000 {
 public:
  StoreExclusive3RegisterOpTester_op_23To20Is1000_Strex_Rule_202_A1_P400()
    : StoreExclusive3RegisterOpTesterop_23To20Is1000(
      state_.StoreExclusive3RegisterOp_Strex_Rule_202_A1_P400_instance_)
  {}
};

class LoadExclusive2RegisterOpTester_op_23To20Is1001_Ldrex_Rule_69_A1_P142
    : public LoadExclusive2RegisterOpTesterop_23To20Is1001 {
 public:
  LoadExclusive2RegisterOpTester_op_23To20Is1001_Ldrex_Rule_69_A1_P142()
    : LoadExclusive2RegisterOpTesterop_23To20Is1001(
      state_.LoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142_instance_)
  {}
};

class StoreExclusive3RegisterDoubleOpTester_op_23To20Is1010_Strexd_Rule_204_A1_P404
    : public StoreExclusive3RegisterDoubleOpTesterop_23To20Is1010 {
 public:
  StoreExclusive3RegisterDoubleOpTester_op_23To20Is1010_Strexd_Rule_204_A1_P404()
    : StoreExclusive3RegisterDoubleOpTesterop_23To20Is1010(
      state_.StoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404_instance_)
  {}
};

class LoadExclusive2RegisterDoubleOpTester_op_23To20Is1011_Ldrexd_Rule_71_A1_P146
    : public LoadExclusive2RegisterDoubleOpTesterop_23To20Is1011 {
 public:
  LoadExclusive2RegisterDoubleOpTester_op_23To20Is1011_Ldrexd_Rule_71_A1_P146()
    : LoadExclusive2RegisterDoubleOpTesterop_23To20Is1011(
      state_.LoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146_instance_)
  {}
};

class StoreExclusive3RegisterOpTester_op_23To20Is1100_Strexb_Rule_203_A1_P402
    : public StoreExclusive3RegisterOpTesterop_23To20Is1100 {
 public:
  StoreExclusive3RegisterOpTester_op_23To20Is1100_Strexb_Rule_203_A1_P402()
    : StoreExclusive3RegisterOpTesterop_23To20Is1100(
      state_.StoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402_instance_)
  {}
};

class LoadExclusive2RegisterOpTester_op_23To20Is1101_Ldrexb_Rule_70_A1_P144
    : public LoadExclusive2RegisterOpTesterop_23To20Is1101 {
 public:
  LoadExclusive2RegisterOpTester_op_23To20Is1101_Ldrexb_Rule_70_A1_P144()
    : LoadExclusive2RegisterOpTesterop_23To20Is1101(
      state_.LoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144_instance_)
  {}
};

class StoreExclusive3RegisterOpTester_op_23To20Is1110_Strexh_Rule_205_A1_P406
    : public StoreExclusive3RegisterOpTesterop_23To20Is1110 {
 public:
  StoreExclusive3RegisterOpTester_op_23To20Is1110_Strexh_Rule_205_A1_P406()
    : StoreExclusive3RegisterOpTesterop_23To20Is1110(
      state_.StoreExclusive3RegisterOp_Strexh_Rule_205_A1_P406_instance_)
  {}
};

class LoadExclusive2RegisterOpTester_op_23To20Is1111_Ldrexh_Rule_72_A1_P148
    : public LoadExclusive2RegisterOpTesterop_23To20Is1111 {
 public:
  LoadExclusive2RegisterOpTester_op_23To20Is1111_Ldrexh_Rule_72_A1_P148()
    : LoadExclusive2RegisterOpTesterop_23To20Is1111(
      state_.LoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148_instance_)
  {}
};

class MoveVfpRegisterOpTester_L_20Is0_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648
    : public MoveVfpRegisterOpTesterL_20Is0_C_8Is0_A_23To21Is000 {
 public:
  MoveVfpRegisterOpTester_L_20Is0_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648()
    : MoveVfpRegisterOpTesterL_20Is0_C_8Is0_A_23To21Is000(
      state_.MoveVfpRegisterOp_Vmov_Rule_330_A1_P648_instance_)
  {}
};

class VfpUsesRegOpTester_L_20Is0_C_8Is0_A_23To21Is111_Vmsr_Rule_336_A1_P660
    : public VfpUsesRegOpTesterL_20Is0_C_8Is0_A_23To21Is111 {
 public:
  VfpUsesRegOpTester_L_20Is0_C_8Is0_A_23To21Is111_Vmsr_Rule_336_A1_P660()
    : VfpUsesRegOpTesterL_20Is0_C_8Is0_A_23To21Is111(
      state_.VfpUsesRegOp_Vmsr_Rule_336_A1_P660_instance_)
  {}
};

class MoveVfpRegisterOpWithTypeSelTester_L_20Is0_C_8Is1_A_23To21Is0xx_Vmov_Rule_328_A1_P644
    : public MoveVfpRegisterOpWithTypeSelTesterL_20Is0_C_8Is1_A_23To21Is0xx {
 public:
  MoveVfpRegisterOpWithTypeSelTester_L_20Is0_C_8Is1_A_23To21Is0xx_Vmov_Rule_328_A1_P644()
    : MoveVfpRegisterOpWithTypeSelTesterL_20Is0_C_8Is1_A_23To21Is0xx(
      state_.MoveVfpRegisterOpWithTypeSel_Vmov_Rule_328_A1_P644_instance_)
  {}
};

class DuplicateToVfpRegistersTester_L_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x_Vdup_Rule_303_A1_P594
    : public DuplicateToVfpRegistersTesterL_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x {
 public:
  DuplicateToVfpRegistersTester_L_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x_Vdup_Rule_303_A1_P594()
    : DuplicateToVfpRegistersTesterL_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x(
      state_.DuplicateToVfpRegisters_Vdup_Rule_303_A1_P594_instance_)
  {}
};

class MoveVfpRegisterOpTester_L_20Is1_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648
    : public MoveVfpRegisterOpTesterL_20Is1_C_8Is0_A_23To21Is000 {
 public:
  MoveVfpRegisterOpTester_L_20Is1_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648()
    : MoveVfpRegisterOpTesterL_20Is1_C_8Is0_A_23To21Is000(
      state_.MoveVfpRegisterOp_Vmov_Rule_330_A1_P648_instance_)
  {}
};

class VfpMrsOpTester_L_20Is1_C_8Is0_A_23To21Is111_Vmrs_Rule_335_A1_P658
    : public VfpMrsOpTesterL_20Is1_C_8Is0_A_23To21Is111 {
 public:
  VfpMrsOpTester_L_20Is1_C_8Is0_A_23To21Is111_Vmrs_Rule_335_A1_P658()
    : VfpMrsOpTesterL_20Is1_C_8Is0_A_23To21Is111(
      state_.VfpMrsOp_Vmrs_Rule_335_A1_P658_instance_)
  {}
};

class MoveVfpRegisterOpWithTypeSelTester_L_20Is1_C_8Is1_Vmov_Rule_329_A1_P646
    : public MoveVfpRegisterOpWithTypeSelTesterL_20Is1_C_8Is1 {
 public:
  MoveVfpRegisterOpWithTypeSelTester_L_20Is1_C_8Is1_Vmov_Rule_329_A1_P646()
    : MoveVfpRegisterOpWithTypeSelTesterL_20Is1_C_8Is1(
      state_.MoveVfpRegisterOpWithTypeSel_Vmov_Rule_329_A1_P646_instance_)
  {}
};

class ForbiddenUncondNopTester_op1_27To20Is101xxxxx_Blx_Rule_23_A2_P58
    : public UnsafeUncondNopTesterop1_27To20Is101xxxxx {
 public:
  ForbiddenUncondNopTester_op1_27To20Is101xxxxx_Blx_Rule_23_A2_P58()
    : UnsafeUncondNopTesterop1_27To20Is101xxxxx(
      state_.ForbiddenUncondNop_Blx_Rule_23_A2_P58_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_op_25To20Is0000x0_Stmda_Stmed_Rule_190_A1_P376_cccc100000w0nnnnrrrrrrrrrrrrrrrr_Test) {
  StoreRegisterListTester_op_25To20Is0000x0_Stmda_Stmed_Rule_190_A1_P376 tester;
  tester.Test("cccc100000w0nnnnrrrrrrrrrrrrrrrr");
}

TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_op_25To20Is0000x1_Ldmda_Ldmfa_Rule_54_A1_P112_cccc100000w1nnnnrrrrrrrrrrrrrrrr_Test) {
  LoadRegisterListTester_op_25To20Is0000x1_Ldmda_Ldmfa_Rule_54_A1_P112 baseline_tester;
  NamedLoadMultiple_Ldmda_Ldmfa_Rule_54_A1_P112 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100000w1nnnnrrrrrrrrrrrrrrrr");
}

TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_op_25To20Is0010x0_Stm_Stmia_Stmea_Rule_189_A1_P374_cccc100010w0nnnnrrrrrrrrrrrrrrrr_Test) {
  StoreRegisterListTester_op_25To20Is0010x0_Stm_Stmia_Stmea_Rule_189_A1_P374 tester;
  tester.Test("cccc100010w0nnnnrrrrrrrrrrrrrrrr");
}

TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_op_25To20Is0010x1_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110_cccc100010w1nnnnrrrrrrrrrrrrrrrr_Test) {
  LoadRegisterListTester_op_25To20Is0010x1_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110 baseline_tester;
  NamedLoadMultiple_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100010w1nnnnrrrrrrrrrrrrrrrr");
}

TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_op_25To20Is0100x0_Stmdb_Stmfd_Rule_191_A1_P378_cccc100100w0nnnnrrrrrrrrrrrrrrrr_Test) {
  StoreRegisterListTester_op_25To20Is0100x0_Stmdb_Stmfd_Rule_191_A1_P378 tester;
  tester.Test("cccc100100w0nnnnrrrrrrrrrrrrrrrr");
}

TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_op_25To20Is0100x1_Ldmdb_Ldmea_Rule_55_A1_P114_cccc100100w1nnnnrrrrrrrrrrrrrrrr_Test) {
  LoadRegisterListTester_op_25To20Is0100x1_Ldmdb_Ldmea_Rule_55_A1_P114 baseline_tester;
  NamedLoadMultiple_Ldmdb_Ldmea_Rule_55_A1_P114 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100100w1nnnnrrrrrrrrrrrrrrrr");
}

TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_op_25To20Is0110x0_Stmid_Stmfa_Rule_192_A1_P380_cccc100110w0nnnnrrrrrrrrrrrrrrrr_Test) {
  StoreRegisterListTester_op_25To20Is0110x0_Stmid_Stmfa_Rule_192_A1_P380 tester;
  tester.Test("cccc100110w0nnnnrrrrrrrrrrrrrrrr");
}

TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_op_25To20Is0110x1_Ldmib_Ldmed_Rule_56_A1_P116_cccc100110w1nnnnrrrrrrrrrrrrrrrr_Test) {
  LoadRegisterListTester_op_25To20Is0110x1_Ldmib_Ldmed_Rule_56_A1_P116 baseline_tester;
  NamedLoadMultiple_Ldmib_Ldmed_Rule_56_A1_P116 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100110w1nnnnrrrrrrrrrrrrrrrr");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op_25To20Is0xx1x0_Stm_Rule_11_B6_A1_P22_cccc100pu100nnnnrrrrrrrrrrrrrrrr_Test) {
  ForbiddenCondNopTester_op_25To20Is0xx1x0_Stm_Rule_11_B6_A1_P22 baseline_tester;
  NamedForbidden_Stm_Rule_11_B6_A1_P22 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100pu100nnnnrrrrrrrrrrrrrrrr");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op_25To20Is0xx1x1_R_15Is0_Ldm_Rule_3_B6_A1_P7_cccc100pu101nnnn0rrrrrrrrrrrrrrr_Test) {
  ForbiddenCondNopTester_op_25To20Is0xx1x1_R_15Is0_Ldm_Rule_3_B6_A1_P7 baseline_tester;
  NamedForbidden_Ldm_Rule_3_B6_A1_P7 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100pu101nnnn0rrrrrrrrrrrrrrr");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op_25To20Is0xx1x1_R_15Is1_Ldm_Rule_2_B6_A1_P5_cccc100pu1w1nnnn1rrrrrrrrrrrrrrr_Test) {
  ForbiddenCondNopTester_op_25To20Is0xx1x1_R_15Is1_Ldm_Rule_2_B6_A1_P5 baseline_tester;
  NamedForbidden_Ldm_Rule_2_B6_A1_P5 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100pu1w1nnnn1rrrrrrrrrrrrrrr");
}

TEST_F(Arm32DecoderStateTests,
       BranchImmediate24Tester_op_25To20Is10xxxx_B_Rule_16_A1_P44_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_Test) {
  BranchImmediate24Tester_op_25To20Is10xxxx_B_Rule_16_A1_P44 baseline_tester;
  NamedBranch_B_Rule_16_A1_P44 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1010iiiiiiiiiiiiiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       BranchImmediate24Tester_op_25To20Is11xxxx_Bl_Blx_Rule_23_A1_P58_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_Test) {
  BranchImmediate24Tester_op_25To20Is11xxxx_Bl_Blx_Rule_23_A1_P58 baseline_tester;
  NamedBranch_Bl_Blx_Rule_23_A1_P58 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1011iiiiiiiiiiiiiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011_extra_load_store_instructions_unpriviledged_cccc0000xx1xxxxxxxxxxxxx1011xxxx_Test) {
  ForbiddenCondNopTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011_extra_load_store_instructions_unpriviledged baseline_tester;
  NamedForbidden_extra_load_store_instructions_unpriviledged actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000xx1xxxxxxxxxxxxx1011xxxx");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1_extra_load_store_instructions_unpriviledged_cccc0000xx1xxxxxxxxxxxxx11x1xxxx_Test) {
  ForbiddenCondNopTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1_extra_load_store_instructions_unpriviledged baseline_tester;
  NamedForbidden_extra_load_store_instructions_unpriviledged actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000xx1xxxxxxxxxxxxx11x1xxxx");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10000RegsNotPc_Mov_Rule_96_A2_P194_cccc00110000iiiiddddIIIIIIIIIIII_Test) {
  Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10000RegsNotPc_Mov_Rule_96_A2_P194 baseline_tester;
  NamedDefs12To15_Mov_Rule_96_A2_P194 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110000iiiiddddIIIIIIIIIIII");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10100RegsNotPc_Mov_Rule_99_A1_P200_cccc00110100iiiiddddIIIIIIIIIIII_Test) {
  Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10100RegsNotPc_Mov_Rule_99_A1_P200 baseline_tester;
  NamedDefs12To15_Mov_Rule_99_A1_P200 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110100iiiiddddIIIIIIIIIIII");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0000xNotRdIsPcAndS_And_Rule_11_A1_P34_cccc0010000snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0000xNotRdIsPcAndS_And_Rule_11_A1_P34 baseline_tester;
  NamedDefs12To15_And_Rule_11_A1_P34 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010000snnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0001xNotRdIsPcAndS_Eor_Rule_44_A1_P94_cccc0010001snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0001xNotRdIsPcAndS_Eor_Rule_44_A1_P94 baseline_tester;
  NamedDefs12To15_Eor_Rule_44_A1_P94 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010001snnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0010x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Sub_Rule_212_A1_P420_cccc0010010snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0010x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Sub_Rule_212_A1_P420 baseline_tester;
  NamedDefs12To15_Sub_Rule_212_A1_P420 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010010snnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_op_24To20Is00100_Rn_19To16Is1111_Adr_Rule_10_A2_P32_cccc001001001111ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_op_24To20Is00100_Rn_19To16Is1111_Adr_Rule_10_A2_P32 baseline_tester;
  NamedDefs12To15_Adr_Rule_10_A2_P32 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001001001111ddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op_24To20Is00101_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1a_cccc00100101nnnn1111iiiiiiiiiiii_Test) {
  ForbiddenCondNopTester_op_24To20Is00101_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1a baseline_tester;
  NamedForbidden_Subs_Pc_Lr_and_related_instructions_Rule_A1a actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00100101nnnn1111iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_142_A1_P284_cccc0010011snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_142_A1_P284 baseline_tester;
  NamedDefs12To15_Rsb_Rule_142_A1_P284 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010011snnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0100x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Add_Rule_5_A1_P22_cccc0010100snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0100x_NotRn_19To16Is1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Add_Rule_5_A1_P22 baseline_tester;
  NamedDefs12To15_Add_Rule_5_A1_P22 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010100snnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_op_24To20Is01000_Rn_19To16Is1111_Adr_Rule_10_A1_P32_cccc001010001111ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_op_24To20Is01000_Rn_19To16Is1111_Adr_Rule_10_A1_P32 baseline_tester;
  NamedDefs12To15_Adr_Rule_10_A1_P32 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001010001111ddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op_24To20Is01001_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1b_cccc00101001nnnn1111iiiiiiiiiiii_Test) {
  ForbiddenCondNopTester_op_24To20Is01001_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1b baseline_tester;
  NamedForbidden_Subs_Pc_Lr_and_related_instructions_Rule_A1b actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00101001nnnn1111iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0101xNotRdIsPcAndS_Adc_Rule_6_A1_P14_cccc0010101snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0101xNotRdIsPcAndS_Adc_Rule_6_A1_P14 baseline_tester;
  NamedDefs12To15_Adc_Rule_6_A1_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010101snnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_151_A1_P302_cccc0010110snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_151_A1_P302 baseline_tester;
  NamedDefs12To15_Sbc_Rule_151_A1_P302 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010110snnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_145_A1_P290_cccc0010111snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_145_A1_P290 baseline_tester;
  NamedDefs12To15_Rsc_Rule_145_A1_P290 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010111snnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       MaskedBinaryRegisterImmediateTestTester_op_24To20Is10001_Tst_Rule_230_A1_P454_cccc00110001nnnn0000iiiiiiiiiiii_Test) {
  MaskedBinaryRegisterImmediateTestTester_op_24To20Is10001_Tst_Rule_230_A1_P454 baseline_tester;
  NamedTestIfAddressMasked_Tst_Rule_230_A1_P454 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110001nnnn0000iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_op_24To20Is10011_Teq_Rule_227_A1_P448_cccc00110011nnnn0000iiiiiiiiiiii_Test) {
  BinaryRegisterImmediateTestTester_op_24To20Is10011_Teq_Rule_227_A1_P448 baseline_tester;
  NamedDontCareInst_Teq_Rule_227_A1_P448 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110011nnnn0000iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_op_24To20Is10101_Cmp_Rule_35_A1_P80_cccc00110101nnnn0000iiiiiiiiiiii_Test) {
  BinaryRegisterImmediateTestTester_op_24To20Is10101_Cmp_Rule_35_A1_P80 baseline_tester;
  NamedDontCareInst_Cmp_Rule_35_A1_P80 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110101nnnn0000iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_op_24To20Is10111_Cmn_Rule_32_A1_P74_cccc00110111nnnn0000iiiiiiiiiiii_Test) {
  BinaryRegisterImmediateTestTester_op_24To20Is10111_Cmn_Rule_32_A1_P74 baseline_tester;
  NamedDontCareInst_Cmn_Rule_32_A1_P74 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110111nnnn0000iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is1100xNotRdIsPcAndS_Orr_Rule_113_A1_P228_cccc0011100snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is1100xNotRdIsPcAndS_Orr_Rule_113_A1_P228 baseline_tester;
  NamedDefs12To15_Orr_Rule_113_A1_P228 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011100snnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_op_24To20Is1101xNotRdIsPcAndS_Mov_Rule_96_A1_P194_cccc0011101s0000ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_op_24To20Is1101xNotRdIsPcAndS_Mov_Rule_96_A1_P194 baseline_tester;
  NamedDefs12To15_Mov_Rule_96_A1_P194 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011101s0000ddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       MaskedBinary2RegisterImmediateOpTester_op_24To20Is1110xNotRdIsPcAndS_Bic_Rule_19_A1_P50_cccc0011110snnnnddddiiiiiiiiiiii_Test) {
  MaskedBinary2RegisterImmediateOpTester_op_24To20Is1110xNotRdIsPcAndS_Bic_Rule_19_A1_P50 baseline_tester;
  NamedMaskAddress_Bic_Rule_19_A1_P50 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011110snnnnddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_op_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_106_A1_P214_cccc0011111s0000ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_op_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_106_A1_P214 baseline_tester;
  NamedDefs12To15_Mvn_Rule_106_A1_P214 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011111s0000ddddiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0000xNotRdIsPcAndS_And_Rule_7_A1_P36_cccc0000000unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0000xNotRdIsPcAndS_And_Rule_7_A1_P36 baseline_tester;
  NamedDefs12To15_And_Rule_7_A1_P36 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0001xNotRdIsPcAndS_Eor_Rule_45_A1_P96_cccc0000001unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0001xNotRdIsPcAndS_Eor_Rule_45_A1_P96 baseline_tester;
  NamedDefs12To15CondsDontCare_Eor_Rule_45_A1_P96 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0010xNotRdIsPcAndS_Sub_Rule_213_A1_P422_cccc0000010unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0010xNotRdIsPcAndS_Sub_Rule_213_A1_P422 baseline_tester;
  NamedDefs12To15_Sub_Rule_213_A1_P422 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000010unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_143_P286_cccc0000011unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_143_P286 baseline_tester;
  NamedDefs12To15_Rsb_Rule_143_P286 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000011unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0100xNotRdIsPcAndS_Add_Rule_6_A1_P24_cccc0000100unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0100xNotRdIsPcAndS_Add_Rule_6_A1_P24 baseline_tester;
  NamedDefs12To15_Add_Rule_6_A1_P24 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0101xNotRdIsPcAndS_Adc_Rule_2_A1_P16_cccc0000101unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0101xNotRdIsPcAndS_Adc_Rule_2_A1_P16 baseline_tester;
  NamedDefs12To15_Adc_Rule_2_A1_P16 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_152_A1_P304_cccc0000110unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_152_A1_P304 baseline_tester;
  NamedDefs12To15_Sbc_Rule_152_A1_P304 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_146_A1_P292_cccc0000111unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_146_A1_P292 baseline_tester;
  NamedDefs12To15_Rsc_Rule_146_A1_P292 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_op1_24To20Is10001_Tst_Rule_231_A1_P456_cccc00010001nnnn0000iiiiitt0mmmm_Test) {
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10001_Tst_Rule_231_A1_P456 baseline_tester;
  NamedDontCareInst_Tst_Rule_231_A1_P456 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010001nnnn0000iiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_op1_24To20Is10011_Teq_Rule_228_A1_P450_cccc00010011nnnn0000iiiiitt0mmmm_Test) {
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10011_Teq_Rule_228_A1_P450 baseline_tester;
  NamedDontCareInst_Teq_Rule_228_A1_P450 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010011nnnn0000iiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_op1_24To20Is10101_Cmp_Rule_36_A1_P82_cccc00010101nnnn0000iiiiitt0mmmm_Test) {
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10101_Cmp_Rule_36_A1_P82 baseline_tester;
  NamedDontCareInst_Cmp_Rule_36_A1_P82 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010101nnnn0000iiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_op1_24To20Is10111_Cmn_Rule_33_A1_P76_cccc00010111nnnn0000iiiiitt0mmmm_Test) {
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10111_Cmn_Rule_33_A1_P76 baseline_tester;
  NamedDontCareInst_Cmn_Rule_33_A1_P76 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010111nnnn0000iiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is1100xNotRdIsPcAndS_Orr_Rule_114_A1_P230_cccc0001100unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is1100xNotRdIsPcAndS_Orr_Rule_114_A1_P230 baseline_tester;
  NamedDefs12To15_Orr_Rule_114_A1_P230 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001100unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is00NotRdIsPcAndS_Mov_Rule_97_A1_P196_cccc0001101u0000dddd00000000mmmm_Test) {
  Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is00NotRdIsPcAndS_Mov_Rule_97_A1_P196 baseline_tester;
  NamedDefs12To15_Mov_Rule_97_A1_P196 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000dddd00000000mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS_Lsl_Rule_88_A1_P178_cccc0001101u0000ddddiiiii000mmmm_Test) {
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS_Lsl_Rule_88_A1_P178 baseline_tester;
  NamedDefs12To15_Lsl_Rule_88_A1_P178 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii000mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is01NotRdIsPcAndS_Lsr_Rule_90_A1_P182_cccc0001101u0000ddddiiiii010mmmm_Test) {
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is01NotRdIsPcAndS_Lsr_Rule_90_A1_P182 baseline_tester;
  NamedDefs12To15_Lsr_Rule_90_A1_P182 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii010mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is10NotRdIsPcAndS_Asr_Rule_14_A1_P40_cccc0001101u0000ddddiiiii100mmmm_Test) {
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is10NotRdIsPcAndS_Asr_Rule_14_A1_P40 baseline_tester;
  NamedDefs12To15_Asr_Rule_14_A1_P40 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii100mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is11NotRdIsPcAndS_Rrx_Rule_141_A1_P282_cccc0001101u0000dddd00000110mmmm_Test) {
  Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is11NotRdIsPcAndS_Rrx_Rule_141_A1_P282 baseline_tester;
  NamedDefs12To15_Rrx_Rule_141_A1_P282 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000dddd00000110mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS_Ror_Rule_139_A1_P278_cccc0001101u0000ddddiiiii110mmmm_Test) {
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_Notop2_11To7Is00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS_Ror_Rule_139_A1_P278 baseline_tester;
  NamedDefs12To15_Ror_Rule_139_A1_P278 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii110mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is1110xNotRdIsPcAndS_Bic_Rule_20_A1_P52_cccc0001110unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is1110xNotRdIsPcAndS_Bic_Rule_20_A1_P52 baseline_tester;
  NamedDefs12To15_Bic_Rule_20_A1_P52 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001110unnnnddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_op1_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_107_A1_P216_cccc0001111u0000ddddiiiiitt0mmmm_Test) {
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_107_A1_P216 baseline_tester;
  NamedDefs12To15_Mvn_Rule_107_A1_P216 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001111u0000ddddiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0000xRegsNotPc_And_Rule_13_A1_P38_cccc0000000snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0000xRegsNotPc_And_Rule_13_A1_P38 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_And_Rule_13_A1_P38 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000snnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0001xRegsNotPc_Eor_Rule_46_A1_P98_cccc0000001snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0001xRegsNotPc_Eor_Rule_46_A1_P98 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnRsRmNotPc_Eor_Rule_46_A1_P98 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001snnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0010xRegsNotPc_Sub_Rule_214_A1_P424_cccc0000010snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0010xRegsNotPc_Sub_Rule_214_A1_P424 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Sub_Rule_214_A1_P424 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000010snnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0011xRegsNotPc_Rsb_Rule_144_A1_P288_cccc0000011snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0011xRegsNotPc_Rsb_Rule_144_A1_P288 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Rsb_Rule_144_A1_P288 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000011snnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0100xRegsNotPc_Add_Rule_7_A1_P26_cccc0000100snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0100xRegsNotPc_Add_Rule_7_A1_P26 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Add_Rule_7_A1_P26 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100snnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0101xRegsNotPc_Adc_Rule_3_A1_P18_cccc0000101snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0101xRegsNotPc_Adc_Rule_3_A1_P18 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Adc_Rule_3_A1_P18 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101snnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0110xRegsNotPc_Sbc_Rule_153_A1_P306_cccc0000110snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0110xRegsNotPc_Sbc_Rule_153_A1_P306 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Sbc_Rule_153_A1_P306 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110snnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0111xRegsNotPc_Rsc_Rule_147_A1_P294_cccc0000111snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0111xRegsNotPc_Rsc_Rule_147_A1_P294 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Rsc_Rule_147_A1_P294 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111snnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_op1_24To20Is10001RegsNotPc_Tst_Rule_232_A1_P458_cccc00010001nnnn0000ssss0tt1mmmm_Test) {
  Binary3RegisterShiftedTestTester_op1_24To20Is10001RegsNotPc_Tst_Rule_232_A1_P458 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Tst_Rule_232_A1_P458 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010001nnnn0000ssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_op1_24To20Is10011RegsNotPc_Teq_Rule_229_A1_P452_cccc00010011nnnn0000ssss0tt1mmmm_Test) {
  Binary3RegisterShiftedTestTester_op1_24To20Is10011RegsNotPc_Teq_Rule_229_A1_P452 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Teq_Rule_229_A1_P452 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010011nnnn0000ssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_op1_24To20Is10101RegsNotPc_Cmp_Rule_37_A1_P84_cccc00010101nnnn0000ssss0tt1mmmm_Test) {
  Binary3RegisterShiftedTestTester_op1_24To20Is10101RegsNotPc_Cmp_Rule_37_A1_P84 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Cmp_Rule_37_A1_P84 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010101nnnn0000ssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_op1_24To20Is10111RegsNotPc_Cmn_Rule_34_A1_P78_cccc00010111nnnn0000ssss0tt1mmmm_Test) {
  Binary3RegisterShiftedTestTester_op1_24To20Is10111RegsNotPc_Cmn_Rule_34_A1_P78 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Cmn_Rule_34_A1_P78 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010111nnnn0000ssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is1100xRegsNotPc_Orr_Rule_115_A1_P212_cccc0001100snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is1100xRegsNotPc_Orr_Rule_115_A1_P212 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Orr_Rule_115_A1_P212 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001100snnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is00RegsNotPc_Lsl_Rule_89_A1_P180_cccc0001101s0000ddddmmmm0001nnnn_Test) {
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is00RegsNotPc_Lsl_Rule_89_A1_P180 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Lsl_Rule_89_A1_P180 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is01RegsNotPc_Lsr_Rule_91_A1_P184_cccc0001101s0000ddddmmmm0011nnnn_Test) {
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is01RegsNotPc_Lsr_Rule_91_A1_P184 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Lsr_Rule_91_A1_P184 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0011nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is10RegsNotPc_Asr_Rule_15_A1_P42_cccc0001101s0000ddddmmmm0101nnnn_Test) {
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is10RegsNotPc_Asr_Rule_15_A1_P42 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Asr_Rule_15_A1_P42 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0101nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is11RegsNotPc_Ror_Rule_140_A1_P280_cccc0001101s0000ddddmmmm0111nnnn_Test) {
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is11RegsNotPc_Ror_Rule_140_A1_P280 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Ror_Rule_140_A1_P280 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0111nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is1110xRegsNotPc_Bic_Rule_21_A1_P54_cccc0001110snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is1110xRegsNotPc_Bic_Rule_21_A1_P54 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Bic_Rule_21_A1_P54 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001110snnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary3RegisterShiftedOpTester_op1_24To20Is1111xRegsNotPc_Mvn_Rule_108_A1_P218_cccc0001111s0000ddddssss0tt1mmmm_Test) {
  Unary3RegisterShiftedOpTester_op1_24To20Is1111xRegsNotPc_Mvn_Rule_108_A1_P218 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Mvn_Rule_108_A1_P218 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001111s0000ddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_opcode_24To20Is01x00_Vstm_Rule_399_A1_A2_P784_cccc11001d00nnnndddd101xiiiiiiii_Test) {
  StoreVectorRegisterListTester_opcode_24To20Is01x00_Vstm_Rule_399_A1_A2_P784 tester;
  tester.Test("cccc11001d00nnnndddd101xiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_opcode_24To20Is01x10_Vstm_Rule_399_A1_A2_P784_cccc11001d10nnnndddd101xiiiiiiii_Test) {
  StoreVectorRegisterListTester_opcode_24To20Is01x10_Vstm_Rule_399_A1_A2_P784 tester;
  tester.Test("cccc11001d10nnnndddd101xiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterTester_opcode_24To20Is1xx00_Vstr_Rule_400_A1_A2_P786_cccc1101ud00nnnndddd101xiiiiiiii_Test) {
  StoreVectorRegisterTester_opcode_24To20Is1xx00_Vstr_Rule_400_A1_A2_P786 tester;
  tester.Test("cccc1101ud00nnnndddd101xiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_opcode_24To20Is10x10_NotRn_19To16Is1101NotRnIsSp_Vstm_Rule_399_A1_A2_P784_cccc11010d10nnnndddd101xiiiiiiii_Test) {
  StoreVectorRegisterListTester_opcode_24To20Is10x10_NotRn_19To16Is1101NotRnIsSp_Vstm_Rule_399_A1_A2_P784 tester;
  tester.Test("cccc11010d10nnnndddd101xiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_opcode_24To20Is10x10_Rn_19To16Is1101_Vpush_355_A1_A2_P696_cccc11010d101101dddd101xiiiiiiii_Test) {
  StoreVectorRegisterListTester_opcode_24To20Is10x10_Rn_19To16Is1101_Vpush_355_A1_A2_P696 tester;
  tester.Test("cccc11010d101101dddd101xiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_opcode_24To20Is01x01_Vldm_Rule_319_A1_A2_P626_cccc11001d01nnnndddd101xiiiiiiii_Test) {
  LoadVectorRegisterListTester_opcode_24To20Is01x01_Vldm_Rule_319_A1_A2_P626 tester;
  tester.Test("cccc11001d01nnnndddd101xiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_opcode_24To20Is01x11_NotRn_19To16Is1101NotRnIsSp_Vldm_Rule_319_A1_A2_P626_cccc11001d11nnnndddd101xiiiiiiii_Test) {
  LoadVectorRegisterListTester_opcode_24To20Is01x11_NotRn_19To16Is1101NotRnIsSp_Vldm_Rule_319_A1_A2_P626 tester;
  tester.Test("cccc11001d11nnnndddd101xiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_opcode_24To20Is01x11_Rn_19To16Is1101_Vpop_Rule_354_A1_A2_P694_cccc11001d111101dddd101xiiiiiiii_Test) {
  LoadVectorRegisterListTester_opcode_24To20Is01x11_Rn_19To16Is1101_Vpop_Rule_354_A1_A2_P694 tester;
  tester.Test("cccc11001d111101dddd101xiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterTester_opcode_24To20Is1xx01_Vldr_Rule_320_A1_A2_P628_cccc1101ud01nnnndddd101xiiiiiiii_Test) {
  LoadVectorRegisterTester_opcode_24To20Is1xx01_Vldr_Rule_320_A1_A2_P628 tester;
  tester.Test("cccc1101ud01nnnndddd101xiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_opcode_24To20Is10x11_Vldm_Rule_318_A1_A2_P626_cccc11010d11nnnndddd101xiiiiiiii_Test) {
  LoadVectorRegisterListTester_opcode_24To20Is10x11_Vldm_Rule_318_A1_A2_P626 tester;
  tester.Test("cccc11010d11nnnndddd101xiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Store3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x0_Strh_Rule_208_A1_P412_cccc000pu0w0nnnntttt00001011mmmm_Test) {
  Store3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x0_Strh_Rule_208_A1_P412 baseline_tester;
  NamedStoreBasedOffsetMemory_Strh_Rule_208_A1_P412 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w0nnnntttt00001011mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x1_Ldrh_Rule_76_A1_P156_cccc000pu0w1nnnntttt00001011mmmm_Test) {
  Load3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x1_Ldrh_Rule_76_A1_P156 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrh_Rule_76_A1_P156 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w1nnnntttt00001011mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x0_Strh_Rule_207_A1_P410_cccc000pu1w0nnnnttttiiii1011iiii_Test) {
  Store2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x0_Strh_Rule_207_A1_P410 baseline_tester;
  NamedStoreBasedImmedMemory_Strh_Rule_207_A1_P410 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w0nnnnttttiiii1011iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_NotRn_19To16Is1111_Ldrh_Rule_74_A1_P152_cccc000pu1w1nnnnttttiiii1011iiii_Test) {
  Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_NotRn_19To16Is1111_Ldrh_Rule_74_A1_P152 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrh_Rule_74_A1_P152 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w1nnnnttttiiii1011iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrh_Rule_75_A1_P154_cccc0001u1011111ttttiiii1011iiii_Test) {
  Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrh_Rule_75_A1_P154 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrh_Rule_75_A1_P154 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1011111ttttiiii1011iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load3RegisterDoubleOpTester_op2_6To5Is10_op1_24To20Isxx0x0_Ldrd_Rule_68_A1_P140_cccc000pu0w0nnnntttt00001101mmmm_Test) {
  Load3RegisterDoubleOpTester_op2_6To5Is10_op1_24To20Isxx0x0_Ldrd_Rule_68_A1_P140 baseline_tester;
  NamedLoadBasedOffsetMemoryDouble_Ldrd_Rule_68_A1_P140 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w0nnnntttt00001101mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_op2_6To5Is10_op1_24To20Isxx0x1_Ldrsb_Rule_80_A1_P164_cccc000pu0w1nnnntttt00001101mmmm_Test) {
  Load3RegisterOpTester_op2_6To5Is10_op1_24To20Isxx0x1_Ldrsb_Rule_80_A1_P164 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrsb_Rule_80_A1_P164 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w1nnnntttt00001101mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_NotRn_19To16Is1111_Ldrd_Rule_66_A1_P136_cccc000pu1w0nnnnttttiiii1101iiii_Test) {
  Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_NotRn_19To16Is1111_Ldrd_Rule_66_A1_P136 baseline_tester;
  NamedLoadBasedImmedMemoryDouble_Ldrd_Rule_66_A1_P136 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w0nnnnttttiiii1101iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16Is1111_Ldrd_Rule_67_A1_P138_cccc0001u1001111ttttiiii1101iiii_Test) {
  Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16Is1111_Ldrd_Rule_67_A1_P138 baseline_tester;
  NamedLoadBasedImmedMemoryDouble_Ldrd_Rule_67_A1_P138 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1001111ttttiiii1101iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_NotRn_19To16Is1111_Ldrsb_Rule_78_A1_P160_cccc000pu1w1nnnnttttiiii1101iiii_Test) {
  Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_NotRn_19To16Is1111_Ldrsb_Rule_78_A1_P160 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrsb_Rule_78_A1_P160 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w1nnnnttttiiii1101iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16Is1111_ldrsb_Rule_79_A1_162_cccc0001u1011111ttttiiii1101iiii_Test) {
  Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16Is1111_ldrsb_Rule_79_A1_162 baseline_tester;
  NamedLoadBasedImmedMemory_ldrsb_Rule_79_A1_162 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1011111ttttiiii1101iiii");
}

TEST_F(Arm32DecoderStateTests,
       Store3RegisterDoubleOpTester_op2_6To5Is11_op1_24To20Isxx0x0_Strd_Rule_201_A1_P398_cccc000pu0w0nnnntttt00001111mmmm_Test) {
  Store3RegisterDoubleOpTester_op2_6To5Is11_op1_24To20Isxx0x0_Strd_Rule_201_A1_P398 baseline_tester;
  NamedStoreBasedOffsetMemoryDouble_Strd_Rule_201_A1_P398 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w0nnnntttt00001111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_op2_6To5Is11_op1_24To20Isxx0x1_Ldrsh_Rule_84_A1_P172_cccc000pu0w1nnnntttt00001111mmmm_Test) {
  Load3RegisterOpTester_op2_6To5Is11_op1_24To20Isxx0x1_Ldrsh_Rule_84_A1_P172 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrsh_Rule_84_A1_P172 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w1nnnntttt00001111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm8DoubleOpTester_op2_6To5Is11_op1_24To20Isxx1x0_Strd_Rule_200_A1_P396_cccc000pu1w0nnnnttttiiii1111iiii_Test) {
  Store2RegisterImm8DoubleOpTester_op2_6To5Is11_op1_24To20Isxx1x0_Strd_Rule_200_A1_P396 baseline_tester;
  NamedStoreBasedImmedMemoryDouble_Strd_Rule_200_A1_P396 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w0nnnnttttiiii1111iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_NotRn_19To16Is1111_Ldrsh_Rule_82_A1_P168_cccc000pu1w1nnnnttttiiii1111iiii_Test) {
  Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_NotRn_19To16Is1111_Ldrsh_Rule_82_A1_P168 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrsh_Rule_82_A1_P168 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w1nnnnttttiiii1111iiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrsh_Rule_83_A1_P170_cccc0001u1011111ttttiiii1111iiii_Test) {
  Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrsh_Rule_83_A1_P170 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrsh_Rule_83_A1_P170 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1011111ttttiiii1111iiii");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is0x00_Vm_la_ls_Rule_423_A2_P636_cccc11100d00nnnndddd101snpm0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is0x00_Vm_la_ls_Rule_423_A2_P636 baseline_tester;
  NamedVfpOp_Vm_la_ls_Rule_423_A2_P636 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d00nnnndddd101snpm0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is0x01_Vnm_la_ls_ul_Rule_343_A1_P674_cccc11100d01nnnndddd101snpm0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is0x01_Vnm_la_ls_ul_Rule_343_A1_P674 baseline_tester;
  NamedVfpOp_Vnm_la_ls_ul_Rule_343_A1_P674 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d01nnnndddd101snpm0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx1_Vnm_la_ls_ul_Rule_343_A2_P674_cccc11100d10nnnndddd101sn1m0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx1_Vnm_la_ls_ul_Rule_343_A2_P674 baseline_tester;
  NamedVfpOp_Vnm_la_ls_ul_Rule_343_A2_P674 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d10nnnndddd101sn1m0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx0_Vmul_Rule_338_A2_P664_cccc11100d10nnnndddd101sn0m0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx0_Vmul_Rule_338_A2_P664 baseline_tester;
  NamedVfpOp_Vmul_Rule_338_A2_P664 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d10nnnndddd101sn0m0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx0_Vadd_Rule_271_A2_P536_cccc11100d11nnnndddd101sn0m0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx0_Vadd_Rule_271_A2_P536 baseline_tester;
  NamedVfpOp_Vadd_Rule_271_A2_P536 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d11nnnndddd101sn0m0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx1_Vsub_Rule_402_A2_P790_cccc11100d11nnnndddd101sn1m0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx1_Vsub_Rule_402_A2_P790 baseline_tester;
  NamedVfpOp_Vsub_Rule_402_A2_P790 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d11nnnndddd101sn1m0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is1x00_opc3_7To6Isx0_Vdiv_Rule_301_A1_P590_cccc11101d00nnnndddd101sn0m0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is1x00_opc3_7To6Isx0_Vdiv_Rule_301_A1_P590 baseline_tester;
  NamedVfpOp_Vdiv_Rule_301_A1_P590 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d00nnnndddd101sn0m0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op1_22To21Is00RegsNotPc_Smlaxx_Rule_166_A1_P330_cccc00010000ddddaaaammmm1xx0nnnn_Test) {
  Binary4RegisterDualOpTester_op1_22To21Is00RegsNotPc_Smlaxx_Rule_166_A1_P330 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010000ddddaaaammmm1xx0nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op1_22To21Is01_op_5Is0RegsNotPc_Smlawx_Rule_171_A1_340_cccc00010010ddddaaaammmm1x00nnnn_Test) {
  Binary4RegisterDualOpTester_op1_22To21Is01_op_5Is0RegsNotPc_Smlawx_Rule_171_A1_340 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010ddddaaaammmm1x00nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op1_22To21Is01_op_5Is1RegsNotPc_Smulwx_Rule_180_A1_P358_cccc00010010dddd0000mmmm1x10nnnn_Test) {
  Binary3RegisterOpAltATester_op1_22To21Is01_op_5Is1RegsNotPc_Smulwx_Rule_180_A1_P358 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010dddd0000mmmm1x10nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op1_22To21Is10RegsNotPc_Smlalxx_Rule_169_A1_P336_cccc00010100hhhhllllmmmm1xx0nnnn_Test) {
  Binary4RegisterDualResultTester_op1_22To21Is10RegsNotPc_Smlalxx_Rule_169_A1_P336 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010100hhhhllllmmmm1xx0nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op1_22To21Is11RegsNotPc_Smulxx_Rule_178_P354_cccc00010110dddd0000mmmm1xx0nnnn_Test) {
  Binary3RegisterOpAltATester_op1_22To21Is11RegsNotPc_Smulxx_Rule_178_P354 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010110dddd0000mmmm1xx0nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x0_B_4Is0_Notop1_repeated_24To20Is0x010_Str_Rule_195_A1_P386_cccc011pd0w0nnnnttttiiiiitt0mmmm_Test) {
  Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x0_B_4Is0_Notop1_repeated_24To20Is0x010_Str_Rule_195_A1_P386 baseline_tester;
  NamedStoreBasedOffsetMemory_Str_Rule_195_A1_P386 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pd0w0nnnnttttiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x010_Strt_Rule_A1_cccc0100u010nnnnttttiiiiiiiiiiii_Test) {
  ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x010_Strt_Rule_A1 baseline_tester;
  NamedForbidden_Strt_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0100u010nnnnttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x010_B_4Is0_Strt_Rule_A2_cccc0110u010nnnnttttiiiiitt0mmmm_Test) {
  ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x010_B_4Is0_Strt_Rule_A2 baseline_tester;
  NamedForbidden_Strt_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110u010nnnnttttiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x011NotRnIsPc_Ldr_Rule_58_A1_P120_cccc010pu0w1nnnnttttiiiiiiiiiiii_Test) {
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x011NotRnIsPc_Ldr_Rule_58_A1_P120 baseline_tester;
  NamedLoadBasedImmedMemory_Ldr_Rule_58_A1_P120 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pu0w1nnnnttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x011_Ldr_Rule_59_A1_P122_cccc0101u0011111ttttiiiiiiiiiiii_Test) {
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x011_Ldr_Rule_59_A1_P122 baseline_tester;
  NamedLoadBasedImmedMemory_Ldr_Rule_59_A1_P122 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0101u0011111ttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x1_B_4Is0_Notop1_repeated_24To20Is0x011_Ldr_Rule_60_A1_P124_cccc011pu0w1nnnnttttiiiiitt0mmmm_Test) {
  Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x1_B_4Is0_Notop1_repeated_24To20Is0x011_Ldr_Rule_60_A1_P124 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldr_Rule_60_A1_P124 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pu0w1nnnnttttiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x011_Ldrt_Rule_A1_cccc0100u011nnnnttttiiiiiiiiiiii_Test) {
  ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x011_Ldrt_Rule_A1 baseline_tester;
  NamedForbidden_Ldrt_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0100u011nnnnttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x011_B_4Is0_Ldrt_Rule_A2_cccc0110u011nnnnttttiiiiitt0mmmm_Test) {
  ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x011_B_4Is0_Ldrt_Rule_A2 baseline_tester;
  NamedForbidden_Ldrt_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110u011nnnnttttiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x0_Notop1_repeated_24To20Is0x110_Strb_Rule_197_A1_P390_cccc010pu1w0nnnnttttiiiiiiiiiiii_Test) {
  Store2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x0_Notop1_repeated_24To20Is0x110_Strb_Rule_197_A1_P390 baseline_tester;
  NamedStoreBasedImmedMemory_Strb_Rule_197_A1_P390 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pu1w0nnnnttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x0_B_4Is0_Notop1_repeated_24To20Is0x110_Strb_Rule_198_A1_P392_cccc011pu1w0nnnnttttiiiiitt0mmmm_Test) {
  Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x0_B_4Is0_Notop1_repeated_24To20Is0x110_Strb_Rule_198_A1_P392 baseline_tester;
  NamedStoreBasedOffsetMemory_Strb_Rule_198_A1_P392 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pu1w0nnnnttttiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x110_Strtb_Rule_A1_cccc0100u110nnnnttttiiiiiiiiiiii_Test) {
  ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x110_Strtb_Rule_A1 baseline_tester;
  NamedForbidden_Strtb_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0100u110nnnnttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x110_B_4Is0_Strtb_Rule_A2_cccc0110u110nnnnttttiiiiitt0mmmm_Test) {
  ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x110_B_4Is0_Strtb_Rule_A2 baseline_tester;
  NamedForbidden_Strtb_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110u110nnnnttttiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x111NotRnIsPc_Ldrb_Rule_62_A1_P128_cccc010pu1w1nnnnttttiiiiiiiiiiii_Test) {
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_NotRn_19To16Is1111_Notop1_repeated_24To20Is0x111NotRnIsPc_Ldrb_Rule_62_A1_P128 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrb_Rule_62_A1_P128 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pu1w1nnnnttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x111_Ldrb_Rule_63_A1_P130_cccc0101u1011111ttttiiiiiiiiiiii_Test) {
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_Notop1_repeated_24To20Is0x111_Ldrb_Rule_63_A1_P130 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrb_Rule_63_A1_P130 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0101u1011111ttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x1_B_4Is0_Notop1_repeated_24To20Is0x111_Ldrb_Rule_64_A1_P132_cccc011pu1w1nnnnttttiiiiitt0mmmm_Test) {
  Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x1_B_4Is0_Notop1_repeated_24To20Is0x111_Ldrb_Rule_64_A1_P132 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrb_Rule_64_A1_P132 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pu1w1nnnnttttiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x111_Ldrtb_Rule_A1_cccc0100u111nnnnttttiiiiiiiiiiii_Test) {
  ForbiddenCondNopTester_A_25Is0_op1_24To20Is0x111_Ldrtb_Rule_A1 baseline_tester;
  NamedForbidden_Ldrtb_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0100u111nnnnttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x111_B_4Is0_Ldrtb_Rule_A2_cccc0110u111nnnnttttiiiiitt0mmmm_Test) {
  ForbiddenCondNopTester_A_25Is1_op1_24To20Is0x111_B_4Is0_Ldrtb_Rule_A2 baseline_tester;
  NamedForbidden_Ldrtb_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110u111nnnnttttiiiiitt0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm12OpRnNotRtOnWritebackTester_Flags_24To21Is1001_Rn_19To16Is1101_Imm12_11To0Is000000000100_Push_Rule_123_A2_P248_cccc010100101101tttt000000000100_Test) {
  Store2RegisterImm12OpRnNotRtOnWritebackTester_Flags_24To21Is1001_Rn_19To16Is1101_Imm12_11To0Is000000000100_Push_Rule_123_A2_P248 tester;
  tester.Test("cccc010100101101tttt000000000100");
}

TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm12OpTester_Notcccc010100101101tttt000000000100_Str_Rule_194_A1_P384_cccc010pu0w0nnnnttttiiiiiiiiiiii_Test) {
  Store2RegisterImm12OpTester_Notcccc010100101101tttt000000000100_Str_Rule_194_A1_P384 baseline_tester;
  NamedStoreBasedImmedMemory_Str_Rule_194_A1_P384 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pu0w0nnnnttttiiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op1_24To20Is11000_op2_7To5Is000_Rd_15To12Is1111RegsNotPc_Usad8_Rule_253_A1_P500_cccc01111000dddd1111mmmm0001nnnn_Test) {
  Binary3RegisterOpAltATester_op1_24To20Is11000_op2_7To5Is000_Rd_15To12Is1111RegsNotPc_Usad8_Rule_253_A1_P500 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01111000dddd1111mmmm0001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op1_24To20Is11000_op2_7To5Is000_NotRd_15To12Is1111RegsNotPc_Usada8_Rule_254_A1_P502_cccc01111000ddddaaaammmm0001nnnn_Test) {
  Binary4RegisterDualOpTester_op1_24To20Is11000_op2_7To5Is000_NotRd_15To12Is1111RegsNotPc_Usada8_Rule_254_A1_P502 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Usada8_Rule_254_A1_P502 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01111000ddddaaaammmm0001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterBitRangeNotRnIsPcTester_op1_24To20Is1101x_op2_7To5Isx10_Sbfx_Rule_154_A1_P308_cccc0111101wwwwwddddlllll101nnnn_Test) {
  Binary2RegisterBitRangeNotRnIsPcTester_op1_24To20Is1101x_op2_7To5Isx10_Sbfx_Rule_154_A1_P308 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sbfx_Rule_154_A1_P308 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0111101wwwwwddddlllll101nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterBitRangeTester_op1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111_Bfc_17_A1_P46_cccc0111110mmmmmddddlllll0011111_Test) {
  Unary1RegisterBitRangeTester_op1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111_Bfc_17_A1_P46 tester;
  tester.Test("cccc0111110mmmmmddddlllll0011111");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterBitRangeTester_op1_24To20Is1110x_op2_7To5Isx00_NotRn_3To0Is1111_Bfi_Rule_18_A1_P48_cccc0111110mmmmmddddlllll001nnnn_Test) {
  Binary2RegisterBitRangeTester_op1_24To20Is1110x_op2_7To5Isx00_NotRn_3To0Is1111_Bfi_Rule_18_A1_P48 baseline_tester;
  NamedDefs12To15CondsDontCare_Bfi_Rule_18_A1_P48 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0111110mmmmmddddlllll001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary2RegisterBitRangeNotRnIsPcTester_op1_24To20Is1111x_op2_7To5Isx10_Ubfx_Rule_236_A1_P466_cccc0111111mmmmmddddlllll101nnnn_Test) {
  Binary2RegisterBitRangeNotRnIsPcTester_op1_24To20Is1111x_op2_7To5Isx10_Ubfx_Rule_236_A1_P466 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Ubfx_Rule_236_A1_P466 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0111111mmmmmddddlllll101nnnn");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op2_6To4Is000_B_9Is1_op_22To21Isx0_Msr_Rule_Banked_register_A1_B9_1990_cccc00010r00mmmmdddd001m00000000_Test) {
  ForbiddenCondNopTester_op2_6To4Is000_B_9Is1_op_22To21Isx0_Msr_Rule_Banked_register_A1_B9_1990 baseline_tester;
  NamedForbidden_Msr_Rule_Banked_register_A1_B9_1990 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r00mmmmdddd001m00000000");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op2_6To4Is000_B_9Is1_op_22To21Isx1_Msr_Rule_Banked_register_A1_B9_1992_cccc00010r10mmmm1111001m0000nnnn_Test) {
  ForbiddenCondNopTester_op2_6To4Is000_B_9Is1_op_22To21Isx1_Msr_Rule_Banked_register_A1_B9_1992 baseline_tester;
  NamedForbidden_Msr_Rule_Banked_register_A1_B9_1992 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r10mmmm1111001m0000nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterSetTester_op2_6To4Is000_B_9Is0_op_22To21Isx0_Mrs_Rule_102_A1_P206_Or_B6_10_cccc00010r001111dddd000000000000_Test) {
  Unary1RegisterSetTester_op2_6To4Is000_B_9Is0_op_22To21Isx0_Mrs_Rule_102_A1_P206_Or_B6_10 tester;
  tester.Test("cccc00010r001111dddd000000000000");
}

TEST_F(Arm32DecoderStateTests,
       Unary1RegisterUseTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx00_Msr_Rule_104_A1_P210_cccc00010010mm00111100000000nnnn_Test) {
  Unary1RegisterUseTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx00_Msr_Rule_104_A1_P210 tester;
  tester.Test("cccc00010010mm00111100000000nnnn");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01_Msr_Rule_B6_1_7_P14_cccc00010010mm01111100000000nnnn_Test) {
  ForbiddenCondNopTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01_Msr_Rule_B6_1_7_P14 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_7_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010mm01111100000000nnnn");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x_Msr_Rule_B6_1_7_P14_cccc00010010mm1m111100000000nnnn_Test) {
  ForbiddenCondNopTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x_Msr_Rule_B6_1_7_P14 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_7_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010mm1m111100000000nnnn");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op2_6To4Is000_B_9Is0_op_22To21Is11_Msr_Rule_B6_1_7_P14_cccc00010110mmmm111100000000nnnn_Test) {
  ForbiddenCondNopTester_op2_6To4Is000_B_9Is0_op_22To21Is11_Msr_Rule_B6_1_7_P14 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_7_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010110mmmm111100000000nnnn");
}

TEST_F(Arm32DecoderStateTests,
       BranchToRegisterTester_op2_6To4Is001_op_22To21Is01_Bx_Rule_25_A1_P62_cccc000100101111111111110001mmmm_Test) {
  BranchToRegisterTester_op2_6To4Is001_op_22To21Is01_Bx_Rule_25_A1_P62 baseline_tester;
  NamedBxBlx_Bx_Rule_25_A1_P62 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110001mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcTester_op2_6To4Is001_op_22To21Is11_Clz_Rule_31_A1_P72_cccc000101101111dddd11110001mmmm_Test) {
  Unary2RegisterOpNotRmIsPcTester_op2_6To4Is001_op_22To21Is11_Clz_Rule_31_A1_P72 baseline_tester;
  NamedDefs12To15RdRnNotPc_Clz_Rule_31_A1_P72 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000101101111dddd11110001mmmm");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op2_6To4Is010_op_22To21Is01_Bxj_Rule_26_A1_P64_cccc000100101111111111110010mmmm_Test) {
  ForbiddenCondNopTester_op2_6To4Is010_op_22To21Is01_Bxj_Rule_26_A1_P64 baseline_tester;
  NamedForbidden_Bxj_Rule_26_A1_P64 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110010mmmm");
}

TEST_F(Arm32DecoderStateTests,
       BranchToRegisterTester_op2_6To4Is011_op_22To21Is01RegsNotPc_Blx_Rule_24_A1_P60_cccc000100101111111111110011mmmm_Test) {
  BranchToRegisterTester_op2_6To4Is011_op_22To21Is01RegsNotPc_Blx_Rule_24_A1_P60 baseline_tester;
  NamedBxBlx_Blx_Rule_24_A1_P60 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110011mmmm");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op2_6To4Is110_op_22To21Is11_Eret_Rule_A1_cccc0001011000000000000001101110_Test) {
  ForbiddenCondNopTester_op2_6To4Is110_op_22To21Is11_Eret_Rule_A1 baseline_tester;
  NamedForbidden_Eret_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001011000000000000001101110");
}

TEST_F(Arm32DecoderStateTests,
       BreakPointAndConstantPoolHeadTester_op2_6To4Is111_op_22To21Is01_Bkpt_Rule_22_A1_P56_cccc00010010iiiiiiiiiiii0111iiii_Test) {
  BreakPointAndConstantPoolHeadTester_op2_6To4Is111_op_22To21Is01_Bkpt_Rule_22_A1_P56 baseline_tester;
  NamedBreakpoint_Bkpt_Rule_22_A1_P56 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010iiiiiiiiiiii0111iiii");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op2_6To4Is111_op_22To21Is10_Hvc_Rule_A1_cccc00010100iiiiiiiiiiii0111iiii_Test) {
  ForbiddenCondNopTester_op2_6To4Is111_op_22To21Is10_Hvc_Rule_A1 baseline_tester;
  NamedForbidden_Hvc_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010100iiiiiiiiiiii0111iiii");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op2_6To4Is111_op_22To21Is11_Smc_Rule_B6_1_9_P18_cccc000101100000000000000111iiii_Test) {
  ForbiddenCondNopTester_op2_6To4Is111_op_22To21Is11_Smc_Rule_B6_1_9_P18 baseline_tester;
  NamedForbidden_Smc_Rule_B6_1_9_P18 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000101100000000000000111iiii");
}

TEST_F(Arm32DecoderStateTests,
       CondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000000_Nop_Rule_110_A1_P222_cccc0011001000001111000000000000_Test) {
  CondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000000_Nop_Rule_110_A1_P222 baseline_tester;
  NamedDontCareInst_Nop_Rule_110_A1_P222 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000000");
}

TEST_F(Arm32DecoderStateTests,
       CondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000001_Yield_Rule_413_A1_P812_cccc0011001000001111000000000001_Test) {
  CondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000001_Yield_Rule_413_A1_P812 baseline_tester;
  NamedDontCareInst_Yield_Rule_413_A1_P812 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000001");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000010_Wfe_Rule_411_A1_P808_cccc0011001000001111000000000010_Test) {
  ForbiddenCondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000010_Wfe_Rule_411_A1_P808 baseline_tester;
  NamedForbidden_Wfe_Rule_411_A1_P808 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000010");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000011_Wfi_Rule_412_A1_P810_cccc0011001000001111000000000011_Test) {
  ForbiddenCondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000011_Wfi_Rule_412_A1_P810 baseline_tester;
  NamedForbidden_Wfi_Rule_412_A1_P810 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000011");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000100_Sev_Rule_158_A1_P316_cccc0011001000001111000000000100_Test) {
  ForbiddenCondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000100_Sev_Rule_158_A1_P316 baseline_tester;
  NamedForbidden_Sev_Rule_158_A1_P316 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000100");
}

TEST_F(Arm32DecoderStateTests,
       CondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx_Dbg_Rule_40_A1_P88_cccc001100100000111100001111iiii_Test) {
  CondNopTester_op_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx_Dbg_Rule_40_A1_P88 baseline_tester;
  NamedDontCareInst_Dbg_Rule_40_A1_P88 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001100100000111100001111iiii");
}

TEST_F(Arm32DecoderStateTests,
       MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is0100_Msr_Rule_103_A1_P208_cccc0011001001001111iiiiiiiiiiii_Test) {
  MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is0100_Msr_Rule_103_A1_P208 baseline_tester;
  NamedDontCareInst_Msr_Rule_103_A1_P208 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001001001111iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is1x00_Msr_Rule_103_A1_P208_cccc001100101x001111iiiiiiiiiiii_Test) {
  MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is1x00_Msr_Rule_103_A1_P208 baseline_tester;
  NamedDontCareInst_Msr_Rule_103_A1_P208 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001100101x001111iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op_22Is0_op1_19To16Isxx01_Msr_Rule_B6_1_6_A1_PB6_12_cccc00110010ii011111iiiiiiiiiiii_Test) {
  ForbiddenCondNopTester_op_22Is0_op1_19To16Isxx01_Msr_Rule_B6_1_6_A1_PB6_12 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110010ii011111iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op_22Is0_op1_19To16Isxx1x_Msr_Rule_B6_1_6_A1_PB6_12_cccc00110010ii1i1111iiiiiiiiiiii_Test) {
  ForbiddenCondNopTester_op_22Is0_op1_19To16Isxx1x_Msr_Rule_B6_1_6_A1_PB6_12 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110010ii1i1111iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenCondNopTester_op_22Is1_Msr_Rule_B6_1_6_A1_PB6_12_cccc00110110iiii1111iiiiiiiiiiii_Test) {
  ForbiddenCondNopTester_op_22Is1_Msr_Rule_B6_1_6_A1_PB6_12 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110110iiii1111iiiiiiiiiiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op_23To20Is000xRegsNotPc_Mul_Rule_105_A1_P212_cccc0000000sdddd0000mmmm1001nnnn_Test) {
  Binary3RegisterOpAltATester_op_23To20Is000xRegsNotPc_Mul_Rule_105_A1_P212 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Mul_Rule_105_A1_P212 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000sdddd0000mmmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op_23To20Is001xRegsNotPc_Mla_Rule_94_A1_P190_cccc0000001sddddaaaammmm1001nnnn_Test) {
  Binary4RegisterDualOpTester_op_23To20Is001xRegsNotPc_Mla_Rule_94_A1_P190 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Mla_Rule_94_A1_P190 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001sddddaaaammmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op_23To20Is0100RegsNotPc_Umaal_Rule_244_A1_P482_cccc00000100hhhhllllmmmm1001nnnn_Test) {
  Binary4RegisterDualResultTester_op_23To20Is0100RegsNotPc_Umaal_Rule_244_A1_P482 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Umaal_Rule_244_A1_P482 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00000100hhhhllllmmmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op_23To20Is0110RegsNotPc_Mls_Rule_95_A1_P192_cccc00000110ddddaaaammmm1001nnnn_Test) {
  Binary4RegisterDualOpTester_op_23To20Is0110RegsNotPc_Mls_Rule_95_A1_P192 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Mls_Rule_95_A1_P192 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00000110ddddaaaammmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op_23To20Is100xRegsNotPc_Umull_Rule_246_A1_P486_cccc0000100shhhhllllmmmm1001nnnn_Test) {
  Binary4RegisterDualResultTester_op_23To20Is100xRegsNotPc_Umull_Rule_246_A1_P486 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Umull_Rule_246_A1_P486 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100shhhhllllmmmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op_23To20Is101xRegsNotPc_Umlal_Rule_245_A1_P484_cccc0000101shhhhllllmmmm1001nnnn_Test) {
  Binary4RegisterDualResultTester_op_23To20Is101xRegsNotPc_Umlal_Rule_245_A1_P484 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Umlal_Rule_245_A1_P484 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101shhhhllllmmmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op_23To20Is110xRegsNotPc_Smull_Rule_179_A1_P356_cccc0000110shhhhllllmmmm1001nnnn_Test) {
  Binary4RegisterDualResultTester_op_23To20Is110xRegsNotPc_Smull_Rule_179_A1_P356 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smull_Rule_179_A1_P356 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110shhhhllllmmmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op_23To20Is111xRegsNotPc_Smlal_Rule_168_A1_P334_cccc0000111shhhhllllmmmm1001nnnn_Test) {
  Binary4RegisterDualResultTester_op_23To20Is111xRegsNotPc_Smlal_Rule_168_A1_P334 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlal_Rule_168_A1_P334 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111shhhhllllmmmm1001nnnn");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc3_7To6Isx0_Vmov_Rule_326_A2_P640_cccc11101d11iiiidddd101s0000iiii_Test) {
  CondVfpOpTester_opc3_7To6Isx0_Vmov_Rule_326_A2_P640 baseline_tester;
  NamedVfpOp_Vmov_Rule_326_A2_P640 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11iiiidddd101s0000iiii");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is0000_opc3_7To6Is01_Vmov_Rule_327_A2_P642_cccc11101d110000dddd101s01m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is0000_opc3_7To6Is01_Vmov_Rule_327_A2_P642 baseline_tester;
  NamedVfpOp_Vmov_Rule_327_A2_P642 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110000dddd101s01m0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is01_Vneg_Rule_342_A2_P672_cccc11101d110001dddd101s01m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is01_Vneg_Rule_342_A2_P672 baseline_tester;
  NamedVfpOp_Vneg_Rule_342_A2_P672 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110001dddd101s01m0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is11_Vsqrt_Rule_388_A1_P762_cccc11101d110001dddd101s11m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is11_Vsqrt_Rule_388_A1_P762 baseline_tester;
  NamedVfpOp_Vsqrt_Rule_388_A1_P762 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110001dddd101s11m0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is001x_opc3_7To6Isx1_Vcvtb_Vcvtt_Rule_300_A1_P588_cccc11101d11001pdddd1010t1m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is001x_opc3_7To6Isx1_Vcvtb_Vcvtt_Rule_300_A1_P588 baseline_tester;
  NamedVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11001pdddd1010t1m0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is0100_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_292_A1_P572_cccc11101d110100dddd101se1m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is0100_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_292_A1_P572 baseline_tester;
  NamedVfpOp_Vcmp_Vcmpe_Rule_292_A1_P572 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110100dddd101se1m0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is0101_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_292_A2_P572_cccc11101d110101dddd101se1000000_Test) {
  CondVfpOpTester_opc2_19To16Is0101_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_292_A2_P572 baseline_tester;
  NamedVfpOp_Vcmp_Vcmpe_Rule_292_A2_P572 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110101dddd101se1000000");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is0111_opc3_7To6Is11_Vcvt_Rule_298_A1_P584_cccc11101d110111dddd101s11m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is0111_opc3_7To6Is11_Vcvt_Rule_298_A1_P584 baseline_tester;
  NamedVfpOp_Vcvt_Rule_298_A1_P584 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110111dddd101s11m0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is1000_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578_cccc11101d111000dddd101sp1m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is1000_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578 baseline_tester;
  NamedVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d111000dddd101sp1m0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is101x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582_cccc11101d11101udddd101fx1i0iiii_Test) {
  CondVfpOpTester_opc2_19To16Is101x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582 baseline_tester;
  NamedVfpOp_Vcvt_Rule_297_A1_P582 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11101udddd101fx1i0iiii");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is110x_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578_cccc11101d11110xdddd101sp1m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is110x_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578 baseline_tester;
  NamedVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11110xdddd101sp1m0mmmm");
}

TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is111x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582_cccc11101d11111udddd101fx1i0iiii_Test) {
  CondVfpOpTester_opc2_19To16Is111x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582 baseline_tester;
  NamedVfpOp_Vcvt_Rule_297_A1_P582 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11111udddd101fx1i0iiii");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Isxx0RegsNotPc_Pkh_Rule_116_A1_P234_cccc01101000nnnnddddiiiiit01mmmm_Test) {
  Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Isxx0RegsNotPc_Pkh_Rule_116_A1_P234 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Pkh_Rule_116_A1_P234 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101000nnnnddddiiiiit01mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is01x_op2_7To5Isxx0_Ssat_Rule_183_A1_P362_cccc0110101iiiiiddddiiiiis01nnnn_Test) {
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is01x_op2_7To5Isxx0_Ssat_Rule_183_A1_P362 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Ssat_Rule_183_A1_P362 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110101iiiiiddddiiiiis01nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is11x_op2_7To5Isxx0_Usat_Rule_255_A1_P504_cccc0110111iiiiiddddiiiiis01nnnn_Test) {
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is11x_op2_7To5Isxx0_Usat_Rule_255_A1_P504 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Usat_Rule_255_A1_P504 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110111iiiiiddddiiiiis01nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc_Sxtab16_Rule_221_A1_P436_cccc01101000nnnnddddrr000111mmmm_Test) {
  Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc_Sxtab16_Rule_221_A1_P436 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxtab16_Rule_221_A1_P436 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101000nnnnddddrr000111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_A_19To16Is1111_Sxtb16_Rule_224_A1_P442_cccc011010001111ddddrr000111mmmm_Test) {
  Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_A_19To16Is1111_Sxtb16_Rule_224_A1_P442 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxtb16_Rule_224_A1_P442 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010001111ddddrr000111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op1_22To20Is000_op2_7To5Is101RegsNotPc_Sel_Rule_156_A1_P312_cccc01101000nnnndddd11111011mmmm_Test) {
  Binary3RegisterOpAltBTester_op1_22To20Is000_op2_7To5Is101RegsNotPc_Sel_Rule_156_A1_P312 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sel_Rule_156_A1_P312 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101000nnnndddd11111011mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is010_op2_7To5Is001_Ssat16_Rule_184_A1_P364_cccc01101010iiiidddd11110011nnnn_Test) {
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is010_op2_7To5Is001_Ssat16_Rule_184_A1_P364 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Ssat16_Rule_184_A1_P364 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101010iiiidddd11110011nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op1_22To20Is010_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc_Sxtab_Rule_220_A1_P434_cccc01101010nnnnddddrr000111mmmm_Test) {
  Binary3RegisterOpAltBTester_op1_22To20Is010_op2_7To5Is011_NotA_19To16Is1111NotRnIsPcAndRegsNotPc_Sxtab_Rule_220_A1_P434 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxtab_Rule_220_A1_P434 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101010nnnnddddrr000111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is010_op2_7To5Is011_A_19To16Is1111_Sxtb_Rule_223_A1_P440_cccc011010101111ddddrr000111mmmm_Test) {
  Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is010_op2_7To5Is011_A_19To16Is1111_Sxtb_Rule_223_A1_P440 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxtb_Rule_223_A1_P440 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010101111ddddrr000111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is001_Rev_Rule_135_A1_P272_cccc011010111111dddd11110011mmmm_Test) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is001_Rev_Rule_135_A1_P272 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Rev_Rule_135_A1_P272 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010111111dddd11110011mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Sxtah_Rule_222_A1_P438_cccc01101011nnnnddddrr000111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Sxtah_Rule_222_A1_P438 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxtah_Rule_222_A1_P438 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101011nnnnddddrr000111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_A_19To16Is1111_Sxth_Rule_225_A1_P444_cccc011010111111ddddrr000111mmmm_Test) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_A_19To16Is1111_Sxth_Rule_225_A1_P444 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxth_Rule_225_A1_P444 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010111111ddddrr000111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is101_Rev16_Rule_136_A1_P274_cccc011010111111dddd11111011mmmm_Test) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is101_Rev16_Rule_136_A1_P274 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Rev16_Rule_136_A1_P274 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010111111dddd11111011mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is100_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Uxtah_Rule_262_A1_P516_cccc01101100nnnnddddrr000111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is100_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Uxtah_Rule_262_A1_P516 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxtah_Rule_262_A1_P516 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101100nnnnddddrr000111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcTester_op1_22To20Is100_op2_7To5Is011_A_19To16Is1111_Uxtb16_Rule_264_A1_P522_cccc011011001111ddddrr000111mmmm_Test) {
  Unary2RegisterOpNotRmIsPcTester_op1_22To20Is100_op2_7To5Is011_A_19To16Is1111_Uxtb16_Rule_264_A1_P522 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxtb16_Rule_264_A1_P522 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011001111ddddrr000111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is110_op2_7To5Is001_Usat16_Rule_256_A1_P506_cccc01101110iiiidddd11110011nnnn_Test) {
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is110_op2_7To5Is001_Usat16_Rule_256_A1_P506 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Usat16_Rule_256_A1_P506 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101110iiiidddd11110011nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is110_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Uxtab_Rule_260_A1_P514_cccc01101110nnnnddddrr000111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is110_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Uxtab_Rule_260_A1_P514 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxtab_Rule_260_A1_P514 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101110nnnnddddrr000111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcTester_op1_22To20Is110_op2_7To5Is011_A_19To16Is1111_Uxtb_Rule_263_A1_P520_cccc011011101111ddddrr000111mmmm_Test) {
  Unary2RegisterOpNotRmIsPcTester_op1_22To20Is110_op2_7To5Is011_A_19To16Is1111_Uxtb_Rule_263_A1_P520 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxtb_Rule_263_A1_P520 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011101111ddddrr000111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcTester_op1_22To20Is111_op2_7To5Is001_Rbit_Rule_134_A1_P270_cccc011011111111dddd11110011mmmm_Test) {
  Unary2RegisterOpNotRmIsPcTester_op1_22To20Is111_op2_7To5Is001_Rbit_Rule_134_A1_P270 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Rbit_Rule_134_A1_P270 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011111111dddd11110011mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Uxtah_Rule_262_A1_P518_cccc01101111nnnnddddrr000111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is011_NotA_19To16Is1111NotRnIsPc_Uxtah_Rule_262_A1_P518 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxtah_Rule_262_A1_P518 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101111nnnnddddrr000111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcTester_op1_22To20Is111_op2_7To5Is011_A_19To16Is1111_Uxth_Rule_265_A1_P524_cccc011011111111ddddrr000111mmmm_Test) {
  Unary2RegisterOpNotRmIsPcTester_op1_22To20Is111_op2_7To5Is011_A_19To16Is1111_Uxth_Rule_265_A1_P524 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxth_Rule_265_A1_P524 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011111111ddddrr000111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcTester_op1_22To20Is111_op2_7To5Is101_Revsh_Rule_137_A1_P276_cccc011011111111dddd11111011mmmm_Test) {
  Unary2RegisterOpNotRmIsPcTester_op1_22To20Is111_op2_7To5Is101_Revsh_Rule_137_A1_P276 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Revsh_Rule_137_A1_P276 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011111111dddd11111011mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is000RegsNotPc_Sadd16_Rule_148_A1_P296_cccc01100001nnnndddd11110001mmmm_Test) {
  Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is000RegsNotPc_Sadd16_Rule_148_A1_P296 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110001mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is001RegsNotPc_Sasx_Rule_150_A1_P300_cccc01100001nnnndddd11110011mmmm_Test) {
  Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is001RegsNotPc_Sasx_Rule_150_A1_P300 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110011mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is010RegsNotPc_Ssax_Rule_185_A1_P366_cccc01100001nnnndddd11110101mmmm_Test) {
  Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is010RegsNotPc_Ssax_Rule_185_A1_P366 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110101mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is011RegsNotPc_Ssub16_Rule_186_A1_P368_cccc01100001nnnndddd11110111mmmm_Test) {
  Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is011RegsNotPc_Ssub16_Rule_186_A1_P368 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is100RegsNotPc_Ssad8_Rule_149_A1_P298_cccc01100001nnnndddd11111001mmmm_Test) {
  Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is100RegsNotPc_Ssad8_Rule_149_A1_P298 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssad8_Rule_149_A1_P298 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11111001mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is111RegsNotPc_Ssub8_Rule_187_A1_P370_cccc01100001nnnndddd11111111mmmm_Test) {
  Binary3RegisterOpAltBTester_op1_21To20Is01_op2_7To5Is111RegsNotPc_Ssub8_Rule_187_A1_P370 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11111111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is000RegsNotPc_Qadd16_Rule_125_A1_P252_cccc01100010nnnndddd11110001mmmm_Test) {
  Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is000RegsNotPc_Qadd16_Rule_125_A1_P252 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110001mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is001RegsNotPc_Qasx_Rule_127_A1_P256_cccc01100010nnnndddd11110011mmmm_Test) {
  Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is001RegsNotPc_Qasx_Rule_127_A1_P256 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110011mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is010RegsNotPc_Qsax_Rule_130_A1_P262_cccc01100010nnnndddd11110101mmmm_Test) {
  Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is010RegsNotPc_Qsax_Rule_130_A1_P262 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110101mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is011RegsNotPc_Qsub16_Rule_132_A1_P266_cccc01100010nnnndddd11110111mmmm_Test) {
  Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is011RegsNotPc_Qsub16_Rule_132_A1_P266 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is100RegsNotPc_Qadd8_Rule_126_A1_P254_cccc01100010nnnndddd11111001mmmm_Test) {
  Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is100RegsNotPc_Qadd8_Rule_126_A1_P254 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11111001mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is111RegsNotPc_Qsub8_Rule_133_A1_P268_cccc01100010nnnndddd11111111mmmm_Test) {
  Binary3RegisterOpAltBTester_op1_21To20Is10_op2_7To5Is111RegsNotPc_Qsub8_Rule_133_A1_P268 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11111111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is000RegsNotPc_Shadd16_Rule_159_A1_P318_cccc01100011nnnndddd11110001mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is000RegsNotPc_Shadd16_Rule_159_A1_P318 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110001mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is001RegsNotPc_Shasx_Rule_161_A1_P322_cccc01100011nnnndddd11110011mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is001RegsNotPc_Shasx_Rule_161_A1_P322 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110011mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is010RegsNotPc_Shsax_Rule_162_A1_P324_cccc01100011nnnndddd11110101mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is010RegsNotPc_Shsax_Rule_162_A1_P324 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110101mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is011RegsNotPc_Shsub16_Rule_163_A1_P326_cccc01100011nnnndddd11110111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is011RegsNotPc_Shsub16_Rule_163_A1_P326 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is100RegsNotPc_Shadd8_Rule_160_A1_P320_cccc01100011nnnndddd11111001mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is100RegsNotPc_Shadd8_Rule_160_A1_P320 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11111001mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is111RegsNotPc_Shsub8_Rule_164_A1_P328_cccc01100011nnnndddd11111111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is111RegsNotPc_Shsub8_Rule_164_A1_P328 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11111111mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op_22To21Is00RegsNotPc_Qadd_Rule_124_A1_P250_cccc00010000nnnndddd00000101mmmm_Test) {
  Binary3RegisterOpAltBTester_op_22To21Is00RegsNotPc_Qadd_Rule_124_A1_P250 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd_Rule_124_A1_P250 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010000nnnndddd00000101mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op_22To21Is01RegsNotPc_Qsub_Rule_131_A1_P264_cccc00010010nnnndddd00000101mmmm_Test) {
  Binary3RegisterOpAltBTester_op_22To21Is01RegsNotPc_Qsub_Rule_131_A1_P264 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub_Rule_131_A1_P264 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010nnnndddd00000101mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op_22To21Is10RegsNotPc_Qdadd_Rule_128_A1_P258_cccc00010100nnnndddd00000101mmmm_Test) {
  Binary3RegisterOpAltBTester_op_22To21Is10RegsNotPc_Qdadd_Rule_128_A1_P258 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdadd_Rule_128_A1_P258 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010100nnnndddd00000101mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBTester_op_22To21Is11RegsNotPc_Qdsub_Rule_129_A1_P260_cccc00010110nnnndddd00000101mmmm_Test) {
  Binary3RegisterOpAltBTester_op_22To21Is11RegsNotPc_Qdsub_Rule_129_A1_P260 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdsub_Rule_129_A1_P260 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010110nnnndddd00000101mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc_Smlad_Rule_167_P332_cccc01110000ddddaaaammmm00x1nnnn_Test) {
  Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc_Smlad_Rule_167_P332 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000ddddaaaammmm00x1nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smuad_Rule_177_P352_cccc01110000dddd1111mmmm00x1nnnn_Test) {
  Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smuad_Rule_177_P352 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000dddd1111mmmm00x1nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is01x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc_Smlsd_Rule_172_P342_cccc01110000ddddaaaammmm01x1nnnn_Test) {
  Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is01x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc_Smlsd_Rule_172_P342 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000ddddaaaammmm01x1nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is01x_A_15To12Is1111RegsNotPc_Smusd_Rule_181_P360_cccc01110000dddd1111mmmm01x1nnnn_Test) {
  Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is01x_A_15To12Is1111RegsNotPc_Smusd_Rule_181_P360 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000dddd1111mmmm01x1nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is00xRegsNotPc_Smlald_Rule_170_P336_cccc01110100hhhhllllmmmm00x1nnnn_Test) {
  Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is00xRegsNotPc_Smlald_Rule_170_P336 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110100hhhhllllmmmm00x1nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is01xRegsNotPc_Smlsld_Rule_173_P344_cccc01110100hhhhllllmmmm01x1nnnn_Test) {
  Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is01xRegsNotPc_Smlsld_Rule_173_P344 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110100hhhhllllmmmm01x1nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc_Smmla_Rule_174_P346_cccc01110101ddddaaaammmm00x1nnnn_Test) {
  Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is00x_NotA_15To12Is1111NotRaIsPcAndRegsNotPc_Smmla_Rule_174_P346 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110101ddddaaaammmm00x1nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op1_22To20Is101_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smmul_Rule_176_P350_cccc01110101dddd1111mmmm00x1nnnn_Test) {
  Binary3RegisterOpAltATester_op1_22To20Is101_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smmul_Rule_176_P350 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110101dddd1111mmmm00x1nnnn");
}

TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is11xRegsNotPc_Smmls_Rule_175_P348_cccc01110101ddddaaaammmm11x1nnnn_Test) {
  Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is11xRegsNotPc_Smmls_Rule_175_P348 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110101ddddaaaammmm11x1nnnn");
}

TEST_F(Arm32DecoderStateTests,
       DeprecatedTester_op_23To20Is0x00_Swp_Swpb_Rule_A1_cccc00010b00nnnntttt00001001tttt_Test) {
  DeprecatedTester_op_23To20Is0x00_Swp_Swpb_Rule_A1 tester;
  tester.Test("cccc00010b00nnnntttt00001001tttt");
}

TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterOpTester_op_23To20Is1000_Strex_Rule_202_A1_P400_cccc00011000nnnndddd11111001tttt_Test) {
  StoreExclusive3RegisterOpTester_op_23To20Is1000_Strex_Rule_202_A1_P400 baseline_tester;
  NamedStoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011000nnnndddd11111001tttt");
}

TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterOpTester_op_23To20Is1001_Ldrex_Rule_69_A1_P142_cccc00011001nnnntttt111110011111_Test) {
  LoadExclusive2RegisterOpTester_op_23To20Is1001_Ldrex_Rule_69_A1_P142 baseline_tester;
  NamedLoadBasedMemory_Ldrex_Rule_69_A1_P142 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011001nnnntttt111110011111");
}

TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterDoubleOpTester_op_23To20Is1010_Strexd_Rule_204_A1_P404_cccc00011010nnnndddd11111001tttt_Test) {
  StoreExclusive3RegisterDoubleOpTester_op_23To20Is1010_Strexd_Rule_204_A1_P404 baseline_tester;
  NamedStoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011010nnnndddd11111001tttt");
}

TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterDoubleOpTester_op_23To20Is1011_Ldrexd_Rule_71_A1_P146_cccc00011011nnnntttt111110011111_Test) {
  LoadExclusive2RegisterDoubleOpTester_op_23To20Is1011_Ldrexd_Rule_71_A1_P146 baseline_tester;
  NamedLoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011011nnnntttt111110011111");
}

TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterOpTester_op_23To20Is1100_Strexb_Rule_203_A1_P402_cccc00011100nnnndddd11111001tttt_Test) {
  StoreExclusive3RegisterOpTester_op_23To20Is1100_Strexb_Rule_203_A1_P402 baseline_tester;
  NamedStoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011100nnnndddd11111001tttt");
}

TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterOpTester_op_23To20Is1101_Ldrexb_Rule_70_A1_P144_cccc00011101nnnntttt111110011111_Test) {
  LoadExclusive2RegisterOpTester_op_23To20Is1101_Ldrexb_Rule_70_A1_P144 baseline_tester;
  NamedLoadBasedMemory_Ldrexb_Rule_70_A1_P144 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011101nnnntttt111110011111");
}

TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterOpTester_op_23To20Is1110_Strexh_Rule_205_A1_P406_cccc00011110nnnndddd11111001tttt_Test) {
  StoreExclusive3RegisterOpTester_op_23To20Is1110_Strexh_Rule_205_A1_P406 baseline_tester;
  NamedStoreBasedMemoryRtBits0To3_Strexh_Rule_205_A1_P406 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011110nnnndddd11111001tttt");
}

TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterOpTester_op_23To20Is1111_Ldrexh_Rule_72_A1_P148_cccc00011111nnnntttt111110011111_Test) {
  LoadExclusive2RegisterOpTester_op_23To20Is1111_Ldrexh_Rule_72_A1_P148 baseline_tester;
  NamedLoadBasedMemory_Ldrexh_Rule_72_A1_P148 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011111nnnntttt111110011111");
}

TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpTester_L_20Is0_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648_cccc11100000nnnntttt1010n0010000_Test) {
  MoveVfpRegisterOpTester_L_20Is0_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648 tester;
  tester.Test("cccc11100000nnnntttt1010n0010000");
}

TEST_F(Arm32DecoderStateTests,
       VfpUsesRegOpTester_L_20Is0_C_8Is0_A_23To21Is111_Vmsr_Rule_336_A1_P660_cccc111011100001tttt101000010000_Test) {
  VfpUsesRegOpTester_L_20Is0_C_8Is0_A_23To21Is111_Vmsr_Rule_336_A1_P660 baseline_tester;
  NamedDontCareInstRdNotPc_Vmsr_Rule_336_A1_P660 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc111011100001tttt101000010000");
}

TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpWithTypeSelTester_L_20Is0_C_8Is1_A_23To21Is0xx_Vmov_Rule_328_A1_P644_cccc11100ii0ddddtttt1011dii10000_Test) {
  MoveVfpRegisterOpWithTypeSelTester_L_20Is0_C_8Is1_A_23To21Is0xx_Vmov_Rule_328_A1_P644 tester;
  tester.Test("cccc11100ii0ddddtttt1011dii10000");
}

TEST_F(Arm32DecoderStateTests,
       DuplicateToVfpRegistersTester_L_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x_Vdup_Rule_303_A1_P594_cccc11101bq0ddddtttt1011d0e10000_Test) {
  DuplicateToVfpRegistersTester_L_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x_Vdup_Rule_303_A1_P594 tester;
  tester.Test("cccc11101bq0ddddtttt1011d0e10000");
}

TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpTester_L_20Is1_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648_cccc11100001nnnntttt1010n0010000_Test) {
  MoveVfpRegisterOpTester_L_20Is1_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648 tester;
  tester.Test("cccc11100001nnnntttt1010n0010000");
}

TEST_F(Arm32DecoderStateTests,
       VfpMrsOpTester_L_20Is1_C_8Is0_A_23To21Is111_Vmrs_Rule_335_A1_P658_cccc111011110001tttt101000010000_Test) {
  VfpMrsOpTester_L_20Is1_C_8Is0_A_23To21Is111_Vmrs_Rule_335_A1_P658 tester;
  tester.Test("cccc111011110001tttt101000010000");
}

TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpWithTypeSelTester_L_20Is1_C_8Is1_Vmov_Rule_329_A1_P646_cccc1110iii1nnnntttt1011nii10000_Test) {
  MoveVfpRegisterOpWithTypeSelTester_L_20Is1_C_8Is1_Vmov_Rule_329_A1_P646 tester;
  tester.Test("cccc1110iii1nnnntttt1011nii10000");
}

TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondNopTester_op1_27To20Is101xxxxx_Blx_Rule_23_A2_P58_1111101hiiiiiiiiiiiiiiiiiiiiiiii_Test) {
  ForbiddenUncondNopTester_op1_27To20Is101xxxxx_Blx_Rule_23_A2_P58 baseline_tester;
  NamedForbidden_Blx_Rule_23_A2_P58 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111101hiiiiiiiiiiiiiiiiiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
