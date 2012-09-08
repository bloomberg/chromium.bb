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


// op(25:20)=0000x0
//    = StoreRegisterList {constraints: }
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
  if ((inst.Bits() & 0x03D00000) != 0x00000000 /* op(25:20)=~0000x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25:20)=0000x1
//    = LoadRegisterList {constraints: }
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
  if ((inst.Bits() & 0x03D00000) != 0x00100000 /* op(25:20)=~0000x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25:20)=0010x0
//    = StoreRegisterList {constraints: }
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
  if ((inst.Bits() & 0x03D00000) != 0x00800000 /* op(25:20)=~0010x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25:20)=001001
//    = LoadRegisterList {constraints: }
class LoadStoreRegisterListTesterop_25To20Is001001
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterop_25To20Is001001(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterop_25To20Is001001
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03F00000) != 0x00900000 /* op(25:20)=~001001 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25:20)=001011 & Rn(19:16)=~1101
//    = LoadRegisterList {constraints: }
class LoadStoreRegisterListTesterop_25To20Is001011_Rn_19To16IsNot1101
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterop_25To20Is001011_Rn_19To16IsNot1101(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterop_25To20Is001011_Rn_19To16IsNot1101
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

// op(25:20)=001011 & Rn(19:16)=1101
//    = LoadRegisterList {constraints: }
class LoadStoreRegisterListTesterop_25To20Is001011_Rn_19To16Is1101
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterop_25To20Is001011_Rn_19To16Is1101(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterop_25To20Is001011_Rn_19To16Is1101
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

// op(25:20)=010000
//    = StoreRegisterList {constraints: }
class LoadStoreRegisterListTesterop_25To20Is010000
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterop_25To20Is010000(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterop_25To20Is010000
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03F00000) != 0x01000000 /* op(25:20)=~010000 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25:20)=010010 & Rn(19:16)=~1101
//    = StoreRegisterList {constraints: }
class LoadStoreRegisterListTesterop_25To20Is010010_Rn_19To16IsNot1101
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterop_25To20Is010010_Rn_19To16IsNot1101(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterop_25To20Is010010_Rn_19To16IsNot1101
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

// op(25:20)=010010 & Rn(19:16)=1101
//    = StoreRegisterList {constraints: }
class LoadStoreRegisterListTesterop_25To20Is010010_Rn_19To16Is1101
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterop_25To20Is010010_Rn_19To16Is1101(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterop_25To20Is010010_Rn_19To16Is1101
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

// op(25:20)=0100x1
//    = LoadRegisterList {constraints: }
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
  if ((inst.Bits() & 0x03D00000) != 0x01100000 /* op(25:20)=~0100x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25:20)=0110x0
//    = StoreRegisterList {constraints: }
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
  if ((inst.Bits() & 0x03D00000) != 0x01800000 /* op(25:20)=~0110x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25:20)=0110x1
//    = LoadRegisterList {constraints: }
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
  if ((inst.Bits() & 0x03D00000) != 0x01900000 /* op(25:20)=~0110x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25:20)=0xx1x0
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_25To20Is0xx1x0
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_25To20Is0xx1x0(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_25To20Is0xx1x0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02500000) != 0x00400000 /* op(25:20)=~0xx1x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25:20)=0xx1x1 & R(15)=0
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_25To20Is0xx1x1_R_15Is0
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_25To20Is0xx1x1_R_15Is0(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_25To20Is0xx1x1_R_15Is0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02500000) != 0x00500000 /* op(25:20)=~0xx1x1 */) return false;
  if ((inst.Bits() & 0x00008000) != 0x00000000 /* R(15)=~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25:20)=0xx1x1 & R(15)=1
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_25To20Is0xx1x1_R_15Is1
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_25To20Is0xx1x1_R_15Is1(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_25To20Is0xx1x1_R_15Is1
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

// op(25:20)=10xxxx
//    = BranchImmediate24 {constraints: }
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
  if ((inst.Bits() & 0x03000000) != 0x02000000 /* op(25:20)=~10xxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BranchImmediate24Tester::
      PassesParsePreconditions(inst, decoder);
}

// op(25:20)=11xxxx
//    = BranchImmediate24 {constraints: }
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
  if ((inst.Bits() & 0x03000000) != 0x03000000 /* op(25:20)=~11xxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BranchImmediate24Tester::
      PassesParsePreconditions(inst, decoder);
}

// op1(25:20)=00000x
//    = UndefinedCondDecoder {constraints: }
class UnsafeCondDecoderTesterop1_25To20Is00000x
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop1_25To20Is00000x(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop1_25To20Is00000x
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03E00000) != 0x00000000 /* op1(25:20)=~00000x */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(25:20)=11xxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop1_25To20Is11xxxx
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop1_25To20Is11xxxx(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop1_25To20Is11xxxx
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03000000) != 0x03000000 /* op1(25:20)=~11xxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// coproc(11:8)=~101x & op1(25:20)=0xxxx0 & op1_repeated(25:20)=~000x00
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is0xxxx0_op1_repeated_25To20IsNot000x00
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is0xxxx0_op1_repeated_25To20IsNot000x00(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is0xxxx0_op1_repeated_25To20IsNot000x00
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */) return false;
  if ((inst.Bits() & 0x02100000) != 0x00000000 /* op1(25:20)=~0xxxx0 */) return false;
  if ((inst.Bits() & 0x03B00000) == 0x00000000 /* op1_repeated(25:20)=000x00 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=~1111 & op1_repeated(25:20)=~000x01
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16IsNot1111_op1_repeated_25To20IsNot000x01
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16IsNot1111_op1_repeated_25To20IsNot000x01(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16IsNot1111_op1_repeated_25To20IsNot000x01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */) return false;
  if ((inst.Bits() & 0x02100000) != 0x00100000 /* op1(25:20)=~0xxxx1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;
  if ((inst.Bits() & 0x03B00000) == 0x00100000 /* op1_repeated(25:20)=000x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=1111 & op1_repeated(25:20)=~000x01
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16Is1111_op1_repeated_25To20IsNot000x01
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16Is1111_op1_repeated_25To20IsNot000x01(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16Is1111_op1_repeated_25To20IsNot000x01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */) return false;
  if ((inst.Bits() & 0x02100000) != 0x00100000 /* op1(25:20)=~0xxxx1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x03B00000) == 0x00100000 /* op1_repeated(25:20)=000x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// coproc(11:8)=~101x & op1(25:20)=000100
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is000100
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is000100(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is000100
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */) return false;
  if ((inst.Bits() & 0x03F00000) != 0x00400000 /* op1(25:20)=~000100 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// coproc(11:8)=~101x & op1(25:20)=000101
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is000101
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is000101(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is000101
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */) return false;
  if ((inst.Bits() & 0x03F00000) != 0x00500000 /* op1(25:20)=~000101 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// coproc(11:8)=~101x & op1(25:20)=10xxxx & op(4)=0
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is10xxxx_op_4Is0
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is10xxxx_op_4Is0(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is10xxxx_op_4Is0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */) return false;
  if ((inst.Bits() & 0x03000000) != 0x02000000 /* op1(25:20)=~10xxxx */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* op(4)=~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// coproc(11:8)=~101x & op1(25:20)=10xxx0 & op(4)=1
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is10xxx0_op_4Is1
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is10xxx0_op_4Is1(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is10xxx0_op_4Is1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */) return false;
  if ((inst.Bits() & 0x03100000) != 0x02000000 /* op1(25:20)=~10xxx0 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000010 /* op(4)=~1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// coproc(11:8)=~101x & op1(25:20)=10xxx1 & op(4)=1
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is10xxx1_op_4Is1
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is10xxx1_op_4Is1(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is10xxx1_op_4Is1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000E00) == 0x00000A00 /* coproc(11:8)=101x */) return false;
  if ((inst.Bits() & 0x03100000) != 0x02100000 /* op1(25:20)=~10xxx1 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000010 /* op(4)=~1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=1011
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* op(25)=~0 */) return false;
  if ((inst.Bits() & 0x01200000) != 0x00200000 /* op1(24:20)=~0xx1x */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x000000B0 /* op2(7:4)=~1011 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=11x1
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* op(25)=~0 */) return false;
  if ((inst.Bits() & 0x01200000) != 0x00200000 /* op1(24:20)=~0xx1x */) return false;
  if ((inst.Bits() & 0x000000D0) != 0x000000D0 /* op2(7:4)=~11x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25)=1 & op1(24:20)=10000
//    = Unary1RegisterImmediateOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* op(25)=~1 */) return false;
  if ((inst.Bits() & 0x01F00000) != 0x01000000 /* op1(24:20)=~10000 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op(25)=1 & op1(24:20)=10100
//    = Unary1RegisterImmediateOp {constraints: ,
//     safety: [Rd(15:12)=~1111]}
class Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10100Safety_Rd_15To12IsNot1111
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10100Safety_Rd_15To12IsNot1111(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10100Safety_Rd_15To12IsNot1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* op(25)=~1 */) return false;
  if ((inst.Bits() & 0x01F00000) != 0x01400000 /* op1(24:20)=~10100 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10100Safety_Rd_15To12IsNot1111
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary1RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=~1111 */);
  return true;
}

// op(24:20)=0000x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op(24:20)=~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=0001x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op(24:20)=~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=0010x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
class Binary2RegisterImmediateOpTesterop_24To20Is0010x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS
    : public Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS {
 public:
  Binary2RegisterImmediateOpTesterop_24To20Is0010x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterop_24To20Is0010x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op(24:20)=~0010x */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=00100 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp {constraints: }
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
  if ((inst.Bits() & 0x01F00000) != 0x00400000 /* op(24:20)=~00100 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=00101 & Rn(19:16)=1111
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_24To20Is00101_Rn_19To16Is1111
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_24To20Is00101_Rn_19To16Is1111(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_24To20Is00101_Rn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x00500000 /* op(24:20)=~00101 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=0011x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op(24:20)=~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=0100x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
class Binary2RegisterImmediateOpTesterop_24To20Is0100x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS
    : public Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS {
 public:
  Binary2RegisterImmediateOpTesterop_24To20Is0100x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterop_24To20Is0100x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op(24:20)=~0100x */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=01000 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp {constraints: }
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
  if ((inst.Bits() & 0x01F00000) != 0x00800000 /* op(24:20)=~01000 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=01001 & Rn(19:16)=1111
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_24To20Is01001_Rn_19To16Is1111
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_24To20Is01001_Rn_19To16Is1111(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_24To20Is01001_Rn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x00900000 /* op(24:20)=~01001 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=0101x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op(24:20)=~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=0110x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op(24:20)=~0110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=0111x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op(24:20)=~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=10001
//    = MaskedBinaryRegisterImmediateTest {constraints: }
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
  if ((inst.Bits() & 0x01F00000) != 0x01100000 /* op(24:20)=~10001 */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=10011
//    = BinaryRegisterImmediateTest {constraints: }
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
  if ((inst.Bits() & 0x01F00000) != 0x01300000 /* op(24:20)=~10011 */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=10101
//    = BinaryRegisterImmediateTest {constraints: }
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
  if ((inst.Bits() & 0x01F00000) != 0x01500000 /* op(24:20)=~10101 */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=10111
//    = BinaryRegisterImmediateTest {constraints: }
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
  if ((inst.Bits() & 0x01F00000) != 0x01700000 /* op(24:20)=~10111 */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=1100x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op(24:20)=~1100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=1101x
//    = Unary1RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op(24:20)=~1101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=1110x
//    = MaskedBinary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op(24:20)=~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op(24:20)=1111x
//    = Unary1RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op(24:20)=~1111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op1(24:20)=~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0001x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op1(24:20)=~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0010x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op1(24:20)=~0010x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0011x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op1(24:20)=~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0100x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op1(24:20)=~0100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op1(24:20)=~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0110x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op1(24:20)=~0110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op1(24:20)=~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=10001
//    = Binary2RegisterImmedShiftedTest {constraints: }
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
  if ((inst.Bits() & 0x01F00000) != 0x01100000 /* op1(24:20)=~10001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=10011
//    = Binary2RegisterImmedShiftedTest {constraints: }
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
  if ((inst.Bits() & 0x01F00000) != 0x01300000 /* op1(24:20)=~10011 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=10101
//    = Binary2RegisterImmedShiftedTest {constraints: }
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
  if ((inst.Bits() & 0x01F00000) != 0x01500000 /* op1(24:20)=~10101 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=10111
//    = Binary2RegisterImmedShiftedTest {constraints: }
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
  if ((inst.Bits() & 0x01F00000) != 0x01700000 /* op1(24:20)=~10111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1100x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op1(24:20)=~1100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00
//    = Unary2RegisterOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7)=~00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op3(6:5)=~00 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: ['NeitherImm5NotZeroNorNotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS
    : public Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7)=00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op3(6:5)=~00 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op3(6:5)=01
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op3(6:5)=~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op3(6:5)=10
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op3(6:5)=~10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11
//    = Unary2RegisterOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7)=~00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op3(6:5)=~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: ['NeitherImm5NotZeroNorNotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS
    : public Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7)=00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op3(6:5)=~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1110x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20)=~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1111x
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op1(24:20)=~1111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0000x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op1(24:20)=~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0001x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op1(24:20)=~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0010x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op1(24:20)=~0010x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0011x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op1(24:20)=~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op1(24:20)=~0100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0101x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op1(24:20)=~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op1(24:20)=~0110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0111x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op1(24:20)=~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=10001
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01F00000) != 0x01100000 /* op1(24:20)=~10001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=10011
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01F00000) != 0x01300000 /* op1(24:20)=~10011 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=10101
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01F00000) != 0x01500000 /* op1(24:20)=~10101 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=10111
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01F00000) != 0x01700000 /* op1(24:20)=~10111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op1(24:20)=~1100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op2(6:5)=00
//    = Binary3RegisterOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op2(6:5)=~00 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op2(6:5)=01
//    = Binary3RegisterOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op2(6:5)=10
//    = Binary3RegisterOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op2(6:5)=11
//    = Binary3RegisterOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20)=~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1111x
//    = Unary3RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op1(24:20)=~1111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary3RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// opcode(24:20)=01x00
//    = StoreVectorRegisterList {constraints: }
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
  if ((inst.Bits() & 0x01B00000) != 0x00800000 /* opcode(24:20)=~01x00 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// opcode(24:20)=01x10
//    = StoreVectorRegisterList {constraints: }
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
  if ((inst.Bits() & 0x01B00000) != 0x00A00000 /* opcode(24:20)=~01x10 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// opcode(24:20)=1xx00
//    = StoreVectorRegister {constraints: }
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
  if ((inst.Bits() & 0x01300000) != 0x01000000 /* opcode(24:20)=~1xx00 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterTester::
      PassesParsePreconditions(inst, decoder);
}

// opcode(24:20)=10x10 & Rn(19:16)=~1101
//    = StoreVectorRegisterList {constraints: ,
//     safety: ['NotRnIsSp']}
class StoreVectorRegisterListTesteropcode_24To20Is10x10_Rn_19To16IsNot1101NotRnIsSp
    : public StoreVectorRegisterListTesterNotRnIsSp {
 public:
  StoreVectorRegisterListTesteropcode_24To20Is10x10_Rn_19To16IsNot1101NotRnIsSp(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTesterNotRnIsSp(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesteropcode_24To20Is10x10_Rn_19To16IsNot1101NotRnIsSp
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x01200000 /* opcode(24:20)=~10x10 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTesterNotRnIsSp::
      PassesParsePreconditions(inst, decoder);
}

// opcode(24:20)=10x10 & Rn(19:16)=1101
//    = StoreVectorRegisterList {constraints: }
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
  if ((inst.Bits() & 0x01B00000) != 0x01200000 /* opcode(24:20)=~10x10 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// opcode(24:20)=01x01
//    = LoadVectorRegisterList {constraints: }
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
  if ((inst.Bits() & 0x01B00000) != 0x00900000 /* opcode(24:20)=~01x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// opcode(24:20)=01x11 & Rn(19:16)=~1101
//    = LoadVectorRegisterList {constraints: ,
//     safety: ['NotRnIsSp']}
class LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_Rn_19To16IsNot1101NotRnIsSp
    : public LoadStoreVectorRegisterListTesterNotRnIsSp {
 public:
  LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_Rn_19To16IsNot1101NotRnIsSp(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTesterNotRnIsSp(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_Rn_19To16IsNot1101NotRnIsSp
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x00B00000 /* opcode(24:20)=~01x11 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000D0000 /* Rn(19:16)=1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTesterNotRnIsSp::
      PassesParsePreconditions(inst, decoder);
}

// opcode(24:20)=01x11 & Rn(19:16)=1101
//    = LoadVectorRegisterList {constraints: }
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
  if ((inst.Bits() & 0x01B00000) != 0x00B00000 /* opcode(24:20)=~01x11 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// opcode(24:20)=1xx01
//    = LoadVectorRegister {constraints: }
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
  if ((inst.Bits() & 0x01300000) != 0x01100000 /* opcode(24:20)=~1xx01 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opcode(24:20)=10x11
//    = LoadVectorRegisterList {constraints: }
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
  if ((inst.Bits() & 0x01B00000) != 0x01300000 /* opcode(24:20)=~10x11 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=01 & op1(24:20)=xx0x0
//    = Store3RegisterOp {constraints: }
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
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00000000 /* op1(24:20)=~xx0x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=01 & op1(24:20)=xx0x1
//    = Load3RegisterOp {constraints: }
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
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20)=~xx0x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=01 & op1(24:20)=xx1x0
//    = Store2RegisterImm8Op {constraints: }
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
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20)=~xx1x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op {constraints: }
class LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16IsNot1111
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16IsNot1111(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16IsNot1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op {constraints: }
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
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=10 & op1(24:20)=xx0x0
//    = Load3RegisterDoubleOp {constraints: }
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
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00000000 /* op1(24:20)=~xx0x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterDoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=10 & op1(24:20)=xx0x1
//    = Load3RegisterOp {constraints: }
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
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20)=~xx0x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=~1111
//    = Load2RegisterImm8DoubleOp {constraints: }
class LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16IsNot1111
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16IsNot1111(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8DoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16IsNot1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20)=~xx1x0 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8DoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=1111
//    = Load2RegisterImm8DoubleOp {constraints: }
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
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20)=~xx1x0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8DoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op {constraints: }
class LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16IsNot1111
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16IsNot1111(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16IsNot1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op {constraints: }
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
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=11 & op1(24:20)=xx0x0
//    = Store3RegisterDoubleOp {constraints: }
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
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00000000 /* op1(24:20)=~xx0x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterDoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=11 & op1(24:20)=xx0x1
//    = Load3RegisterOp {constraints: }
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
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20)=~xx0x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=11 & op1(24:20)=xx1x0
//    = Store2RegisterImm8DoubleOp {constraints: }
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
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20)=~xx1x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8DoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op {constraints: }
class LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16IsNot1111
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16IsNot1111(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16IsNot1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op {constraints: }
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
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=0x00
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x00B00000) != 0x00000000 /* opc1(23:20)=~0x00 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=0x01
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x00B00000) != 0x00100000 /* opc1(23:20)=~0x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=0x10 & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x00B00000) != 0x00200000 /* opc1(23:20)=~0x10 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6)=~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=0x10 & opc3(7:6)=x0
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x00B00000) != 0x00200000 /* opc1(23:20)=~0x10 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000000 /* opc3(7:6)=~x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=0x11 & opc3(7:6)=x0
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x00B00000) != 0x00300000 /* opc1(23:20)=~0x11 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000000 /* opc3(7:6)=~x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=0x11 & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x00B00000) != 0x00300000 /* opc1(23:20)=~0x11 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6)=~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=1x00 & opc3(7:6)=x0
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x00B00000) != 0x00800000 /* opc1(23:20)=~1x00 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000000 /* opc3(7:6)=~x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=1x01
//    = CondVfpOp {constraints: }
class CondVfpOpTesteropc1_23To20Is1x01
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc1_23To20Is1x01(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesteropc1_23To20Is1x01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00900000 /* opc1(23:20)=~1x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc1(23:20)=1x10
//    = CondVfpOp {constraints: }
class CondVfpOpTesteropc1_23To20Is1x10
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc1_23To20Is1x10(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesteropc1_23To20Is1x10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00A00000 /* opc1(23:20)=~1x10 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:21)=00
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00600000) != 0x00000000 /* op1(22:21)=~00 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:21)=01 & op(5)=0
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op1(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op(5)=~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:21)=01 & op(5)=1
//    = Binary3RegisterOpAltA {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op1(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000020 /* op(5)=~1 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:21)=10
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00600000) != 0x00400000 /* op1(22:21)=~10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:21)=11
//    = Binary3RegisterOpAltA {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op1(22:21)=~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=1 & op1(24:20)=xx0x0 & B(4)=0 & op1_repeated(24:20)=~0x010
//    = Store3RegisterImm5Op {constraints: }
class LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x0_B_4Is0_op1_repeated_24To20IsNot0x010
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x0_B_4Is0_op1_repeated_24To20IsNot0x010(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x0_B_4Is0_op1_repeated_24To20IsNot0x010
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25)=~1 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00000000 /* op1(24:20)=~xx0x0 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4)=~0 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00200000 /* op1_repeated(24:20)=0x010 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterImm5OpTester::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=0 & op1(24:20)=0x010
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x010
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x010(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x010
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25)=~0 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00200000 /* op1(24:20)=~0x010 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=1 & op1(24:20)=0x010 & B(4)=0
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x010_B_4Is0
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x010_B_4Is0(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x010_B_4Is0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25)=~1 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00200000 /* op1(24:20)=~0x010 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4)=~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x011
//    = Load2RegisterImm12Op {constraints: ,
//     safety: ['NotRnIsPc']}
class LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x011NotRnIsPc
    : public LoadStore2RegisterImm12OpTesterNotRnIsPc {
 public:
  LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x011NotRnIsPc(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTesterNotRnIsPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x011NotRnIsPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25)=~0 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20)=~xx0x1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00300000 /* op1_repeated(24:20)=0x011 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTesterNotRnIsPc::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x011
//    = Load2RegisterImm12Op {constraints: }
class LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x011
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x011(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x011
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25)=~0 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20)=~xx0x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00300000 /* op1_repeated(24:20)=0x011 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=1 & op1(24:20)=xx0x1 & B(4)=0 & op1_repeated(24:20)=~0x011
//    = Load3RegisterImm5Op {constraints: }
class LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x1_B_4Is0_op1_repeated_24To20IsNot0x011
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x1_B_4Is0_op1_repeated_24To20IsNot0x011(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x1_B_4Is0_op1_repeated_24To20IsNot0x011
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25)=~1 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20)=~xx0x1 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4)=~0 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00300000 /* op1_repeated(24:20)=0x011 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterImm5OpTester::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=0 & op1(24:20)=0x011
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x011
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x011(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x011
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25)=~0 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00300000 /* op1(24:20)=~0x011 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=1 & op1(24:20)=0x011 & B(4)=0
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x011_B_4Is0
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x011_B_4Is0(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x011_B_4Is0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25)=~1 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00300000 /* op1(24:20)=~0x011 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4)=~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=0 & op1(24:20)=xx1x0 & op1_repeated(24:20)=~0x110
//    = Store2RegisterImm12Op {constraints: }
class LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x0_op1_repeated_24To20IsNot0x110
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x0_op1_repeated_24To20IsNot0x110(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x0_op1_repeated_24To20IsNot0x110
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25)=~0 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20)=~xx1x0 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00600000 /* op1_repeated(24:20)=0x110 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=1 & op1(24:20)=xx1x0 & B(4)=0 & op1_repeated(24:20)=~0x110
//    = Store3RegisterImm5Op {constraints: }
class LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x0_B_4Is0_op1_repeated_24To20IsNot0x110
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x0_B_4Is0_op1_repeated_24To20IsNot0x110(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x0_B_4Is0_op1_repeated_24To20IsNot0x110
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25)=~1 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20)=~xx1x0 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4)=~0 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00600000 /* op1_repeated(24:20)=0x110 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterImm5OpTester::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=0 & op1(24:20)=0x110
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x110
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x110(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x110
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25)=~0 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00600000 /* op1(24:20)=~0x110 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=1 & op1(24:20)=0x110 & B(4)=0
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x110_B_4Is0
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x110_B_4Is0(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x110_B_4Is0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25)=~1 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00600000 /* op1(24:20)=~0x110 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4)=~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x111
//    = Load2RegisterImm12Op {constraints: ,
//     safety: ['NotRnIsPc']}
class LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x111NotRnIsPc
    : public LoadStore2RegisterImm12OpTesterNotRnIsPc {
 public:
  LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x111NotRnIsPc(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTesterNotRnIsPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x111NotRnIsPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25)=~0 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00700000 /* op1_repeated(24:20)=0x111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTesterNotRnIsPc::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x111
//    = Load2RegisterImm12Op {constraints: }
class LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x111
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x111(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25)=~0 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00700000 /* op1_repeated(24:20)=0x111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=1 & op1(24:20)=xx1x1 & B(4)=0 & op1_repeated(24:20)=~0x111
//    = Load3RegisterImm5Op {constraints: }
class LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x1_B_4Is0_op1_repeated_24To20IsNot0x111
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x1_B_4Is0_op1_repeated_24To20IsNot0x111(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x1_B_4Is0_op1_repeated_24To20IsNot0x111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25)=~1 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4)=~0 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00700000 /* op1_repeated(24:20)=0x111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterImm5OpTester::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=0 & op1(24:20)=0x111
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x111
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x111(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25)=~0 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00700000 /* op1(24:20)=~0x111 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// A(25)=1 & op1(24:20)=0x111 & B(4)=0
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x111_B_4Is0
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x111_B_4Is0(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x111_B_4Is0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* A(25)=~1 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00700000 /* op1(24:20)=~0x111 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* B(4)=~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Flags(24:21)=1001 & Rn(19:16)=1101 & Imm12(11:0)=000000000100
//    = Store2RegisterImm12OpRnNotRtOnWriteback {constraints: }
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
  if ((inst.Bits() & 0x01E00000) != 0x01200000 /* Flags(24:21)=~1001 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000D0000 /* Rn(19:16)=~1101 */) return false;
  if ((inst.Bits() & 0x00000FFF) != 0x00000004 /* Imm12(11:0)=~000000000100 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// 
//    = Store2RegisterImm12Op {constraints: & ~cccc010100101101tttt000000000100 }
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
  if ((inst.Bits() & 0x0FFF0FFF) == 0x052D0004 /* constraint(31:0)=xxxx010100101101xxxx000000000100 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x01F00000) != 0x01800000 /* op1(24:20)=~11000 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterop1_24To20Is11000_op2_7To5Is000_Rd_15To12IsNot1111RegsNotPc
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterop1_24To20Is11000_op2_7To5Is000_Rd_15To12IsNot1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterop1_24To20Is11000_op2_7To5Is000_Rd_15To12IsNot1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01800000 /* op1(24:20)=~11000 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;
  if ((inst.Bits() & 0x0000F000) == 0x0000F000 /* Rd(15:12)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op2(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract {constraints: ,
//     safety: ['RegsNotPc']}
class Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterop1_24To20Is1101x_op2_7To5Isx10RegsNotPc
    : public Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc {
 public:
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterop1_24To20Is1101x_op2_7To5Isx10RegsNotPc(const NamedClassDecoder& decoder)
    : Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterop1_24To20Is1101x_op2_7To5Isx10RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(7:5)=~x10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=1111
//    = Unary1RegisterBitRangeMsbGeLsb {constraints: ,
//     safety: ['RegsNotPc']}
class Unary1RegisterBitRangeMsbGeLsbTesterop1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111RegsNotPc
    : public Unary1RegisterBitRangeMsbGeLsbTesterRegsNotPc {
 public:
  Unary1RegisterBitRangeMsbGeLsbTesterop1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111RegsNotPc(const NamedClassDecoder& decoder)
    : Unary1RegisterBitRangeMsbGeLsbTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterBitRangeMsbGeLsbTesterop1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20)=~1110x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op2(7:5)=~x00 */) return false;
  if ((inst.Bits() & 0x0000000F) != 0x0000000F /* Rn(3:0)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterBitRangeMsbGeLsbTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=~1111
//    = Binary2RegisterBitRangeMsbGeLsb {constraints: ,
//     safety: ['RegsNotPc']}
class Binary2RegisterBitRangeMsbGeLsbTesterop1_24To20Is1110x_op2_7To5Isx00_Rn_3To0IsNot1111RegsNotPc
    : public Binary2RegisterBitRangeMsbGeLsbTesterRegsNotPc {
 public:
  Binary2RegisterBitRangeMsbGeLsbTesterop1_24To20Is1110x_op2_7To5Isx00_Rn_3To0IsNot1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary2RegisterBitRangeMsbGeLsbTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterBitRangeMsbGeLsbTesterop1_24To20Is1110x_op2_7To5Isx00_Rn_3To0IsNot1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20)=~1110x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op2(7:5)=~x00 */) return false;
  if ((inst.Bits() & 0x0000000F) == 0x0000000F /* Rn(3:0)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterBitRangeMsbGeLsbTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1111x & op2(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract {constraints: ,
//     safety: ['RegsNotPc']}
class Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterop1_24To20Is1111x_op2_7To5Isx10RegsNotPc
    : public Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc {
 public:
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterop1_24To20Is1111x_op2_7To5Isx10RegsNotPc(const NamedClassDecoder& decoder)
    : Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterop1_24To20Is1111x_op2_7To5Isx10RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op1(24:20)=~1111x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(7:5)=~x10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=11111 & op2(7:5)=111
//    = Roadblock {constraints: }
class RoadblockTesterop1_24To20Is11111_op2_7To5Is111
    : public RoadblockTester {
 public:
  RoadblockTesterop1_24To20Is11111_op2_7To5Is111(const NamedClassDecoder& decoder)
    : RoadblockTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool RoadblockTesterop1_24To20Is11111_op2_7To5Is111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01F00000 /* op1(24:20)=~11111 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000E0 /* op2(7:5)=~111 */) return false;

  // Check other preconditions defined for the base decoder.
  return RoadblockTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=0010000 & op2(7:4)=xx0x & Rn(19:16)=xxx0
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_26To20Is0010000_op2_7To4Isxx0x_Rn_19To16Isxxx0
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_26To20Is0010000_op2_7To4Isxx0x_Rn_19To16Isxxx0(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_26To20Is0010000_op2_7To4Isxx0x_Rn_19To16Isxxx0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x01000000 /* op1(26:20)=~0010000 */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op2(7:4)=~xx0x */) return false;
  if ((inst.Bits() & 0x00010000) != 0x00000000 /* Rn(19:16)=~xxx0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=0010000 & op2(7:4)=0000 & Rn(19:16)=xxx1
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_26To20Is0010000_op2_7To4Is0000_Rn_19To16Isxxx1
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_26To20Is0010000_op2_7To4Is0000_Rn_19To16Isxxx1(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_26To20Is0010000_op2_7To4Is0000_Rn_19To16Isxxx1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x01000000 /* op1(26:20)=~0010000 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x00000000 /* op2(7:4)=~0000 */) return false;
  if ((inst.Bits() & 0x00010000) != 0x00010000 /* Rn(19:16)=~xxx1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=100x001
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_26To20Is100x001
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_26To20Is100x001(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_26To20Is100x001
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x04100000 /* op1(26:20)=~100x001 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=100x101
//    = PreloadRegisterImm12Op {constraints: }
class PreloadRegisterImm12OpTesterop1_26To20Is100x101
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterop1_26To20Is100x101(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterop1_26To20Is100x101
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x04500000 /* op1(26:20)=~100x101 */) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=100xx11
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_26To20Is100xx11
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_26To20Is100xx11(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_26To20Is100xx11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07300000) != 0x04300000 /* op1(26:20)=~100xx11 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=101x001 & Rn(19:16)=~1111
//    = PreloadRegisterImm12Op {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx }
class PreloadRegisterImm12OpTesterop1_26To20Is101x001_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterop1_26To20Is101x001_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterop1_26To20Is101x001_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x05100000 /* op1(26:20)=~101x001 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check pattern restrictions of row.
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* constraint(31:0)=xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=101x001 & Rn(19:16)=1111
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_26To20Is101x001_Rn_19To16Is1111
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_26To20Is101x001_Rn_19To16Is1111(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_26To20Is101x001_Rn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x05100000 /* op1(26:20)=~101x001 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=101x101 & Rn(19:16)=~1111
//    = PreloadRegisterImm12Op {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx }
class PreloadRegisterImm12OpTesterop1_26To20Is101x101_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterop1_26To20Is101x101_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterop1_26To20Is101x101_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x05500000 /* op1(26:20)=~101x101 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;

  // Check pattern restrictions of row.
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* constraint(31:0)=xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=101x101 & Rn(19:16)=1111
//    = PreloadRegisterImm12Op {constraints: }
class PreloadRegisterImm12OpTesterop1_26To20Is101x101_Rn_19To16Is1111
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterop1_26To20Is101x101_Rn_19To16Is1111(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterop1_26To20Is101x101_Rn_19To16Is1111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x05500000 /* op1(26:20)=~101x101 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=1010011
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_26To20Is1010011
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_26To20Is1010011(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_26To20Is1010011
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x05300000 /* op1(26:20)=~1010011 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=1010111 & op2(7:4)=0000
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is0000
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is0000(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is0000
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x05700000 /* op1(26:20)=~1010111 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x00000000 /* op2(7:4)=~0000 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=1010111 & op2(7:4)=0001
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is0001
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is0001(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is0001
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x05700000 /* op1(26:20)=~1010111 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x00000010 /* op2(7:4)=~0001 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=1010111 & op2(7:4)=001x
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is001x
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is001x(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is001x
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x05700000 /* op1(26:20)=~1010111 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:4)=~001x */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=1010111 & op2(7:4)=0100
//    = DataBarrier {constraints: }
class DataBarrierTesterop1_26To20Is1010111_op2_7To4Is0100
    : public DataBarrierTester {
 public:
  DataBarrierTesterop1_26To20Is1010111_op2_7To4Is0100(const NamedClassDecoder& decoder)
    : DataBarrierTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool DataBarrierTesterop1_26To20Is1010111_op2_7To4Is0100
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x05700000 /* op1(26:20)=~1010111 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x00000040 /* op2(7:4)=~0100 */) return false;

  // Check other preconditions defined for the base decoder.
  return DataBarrierTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=1010111 & op2(7:4)=0101
//    = DataBarrier {constraints: }
class DataBarrierTesterop1_26To20Is1010111_op2_7To4Is0101
    : public DataBarrierTester {
 public:
  DataBarrierTesterop1_26To20Is1010111_op2_7To4Is0101(const NamedClassDecoder& decoder)
    : DataBarrierTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool DataBarrierTesterop1_26To20Is1010111_op2_7To4Is0101
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x05700000 /* op1(26:20)=~1010111 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x00000050 /* op2(7:4)=~0101 */) return false;

  // Check other preconditions defined for the base decoder.
  return DataBarrierTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=1010111 & op2(7:4)=0110
//    = InstructionBarrier {constraints: }
class InstructionBarrierTesterop1_26To20Is1010111_op2_7To4Is0110
    : public InstructionBarrierTester {
 public:
  InstructionBarrierTesterop1_26To20Is1010111_op2_7To4Is0110(const NamedClassDecoder& decoder)
    : InstructionBarrierTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool InstructionBarrierTesterop1_26To20Is1010111_op2_7To4Is0110
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x05700000 /* op1(26:20)=~1010111 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x00000060 /* op2(7:4)=~0110 */) return false;

  // Check other preconditions defined for the base decoder.
  return InstructionBarrierTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=1010111 & op2(7:4)=0111
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is0111
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is0111(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is0111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x05700000 /* op1(26:20)=~1010111 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x00000070 /* op2(7:4)=~0111 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=1010111 & op2(7:4)=1xxx
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is1xxx
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is1xxx(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is1xxx
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x05700000 /* op1(26:20)=~1010111 */) return false;
  if ((inst.Bits() & 0x00000080) != 0x00000080 /* op2(7:4)=~1xxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=1011x11
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_26To20Is1011x11
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_26To20Is1011x11(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_26To20Is1011x11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07B00000) != 0x05B00000 /* op1(26:20)=~1011x11 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=110x001 & op2(7:4)=xxx0
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_26To20Is110x001_op2_7To4Isxxx0
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_26To20Is110x001_op2_7To4Isxxx0(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_26To20Is110x001_op2_7To4Isxxx0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x06100000 /* op1(26:20)=~110x001 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* op2(7:4)=~xxx0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=110x101 & op2(7:4)=xxx0
//    = PreloadRegisterPairOp {constraints: }
class PreloadRegisterPairOpTesterop1_26To20Is110x101_op2_7To4Isxxx0
    : public PreloadRegisterPairOpTester {
 public:
  PreloadRegisterPairOpTesterop1_26To20Is110x101_op2_7To4Isxxx0(const NamedClassDecoder& decoder)
    : PreloadRegisterPairOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterPairOpTesterop1_26To20Is110x101_op2_7To4Isxxx0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x06500000 /* op1(26:20)=~110x101 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* op2(7:4)=~xxx0 */) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterPairOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=111x001 & op2(7:4)=xxx0
//    = PreloadRegisterPairOpWAndRnNotPc {constraints: }
class PreloadRegisterPairOpWAndRnNotPcTesterop1_26To20Is111x001_op2_7To4Isxxx0
    : public PreloadRegisterPairOpWAndRnNotPcTester {
 public:
  PreloadRegisterPairOpWAndRnNotPcTesterop1_26To20Is111x001_op2_7To4Isxxx0(const NamedClassDecoder& decoder)
    : PreloadRegisterPairOpWAndRnNotPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterPairOpWAndRnNotPcTesterop1_26To20Is111x001_op2_7To4Isxxx0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x07100000 /* op1(26:20)=~111x001 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* op2(7:4)=~xxx0 */) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterPairOpWAndRnNotPcTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=111x101 & op2(7:4)=xxx0
//    = PreloadRegisterPairOpWAndRnNotPc {constraints: }
class PreloadRegisterPairOpWAndRnNotPcTesterop1_26To20Is111x101_op2_7To4Isxxx0
    : public PreloadRegisterPairOpWAndRnNotPcTester {
 public:
  PreloadRegisterPairOpWAndRnNotPcTesterop1_26To20Is111x101_op2_7To4Isxxx0(const NamedClassDecoder& decoder)
    : PreloadRegisterPairOpWAndRnNotPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterPairOpWAndRnNotPcTesterop1_26To20Is111x101_op2_7To4Isxxx0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x07500000 /* op1(26:20)=~111x101 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* op2(7:4)=~xxx0 */) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterPairOpWAndRnNotPcTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(26:20)=11xxx11 & op2(7:4)=xxx0
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_26To20Is11xxx11_op2_7To4Isxxx0
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_26To20Is11xxx11_op2_7To4Isxxx0(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_26To20Is11xxx11_op2_7To4Isxxx0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x06300000) != 0x06300000 /* op1(26:20)=~11xxx11 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* op2(7:4)=~xxx0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=000 & B(9)=1 & op(22:21)=x0
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop2_6To4Is000_B_9Is1_op_22To21Isx0
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop2_6To4Is000_B_9Is1_op_22To21Isx0(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop2_6To4Is000_B_9Is1_op_22To21Isx0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000200 /* B(9)=~1 */) return false;
  if ((inst.Bits() & 0x00200000) != 0x00000000 /* op(22:21)=~x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=000 & B(9)=1 & op(22:21)=x1
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop2_6To4Is000_B_9Is1_op_22To21Isx1
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop2_6To4Is000_B_9Is1_op_22To21Isx1(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop2_6To4Is000_B_9Is1_op_22To21Isx1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000200 /* B(9)=~1 */) return false;
  if ((inst.Bits() & 0x00200000) != 0x00200000 /* op(22:21)=~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=x0
//    = Unary1RegisterSet {constraints: }
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
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9)=~0 */) return false;
  if ((inst.Bits() & 0x00200000) != 0x00000000 /* op(22:21)=~x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterSetTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00
//    = Unary1RegisterUse {constraints: }
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
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9)=~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00030000) != 0x00000000 /* op1(19:16)=~xx00 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterUseTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx01
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9)=~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00030000) != 0x00010000 /* op1(19:16)=~xx01 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx1x
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9)=~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00020000) != 0x00020000 /* op1(19:16)=~xx1x */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=11
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop2_6To4Is000_B_9Is0_op_22To21Is11
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop2_6To4Is000_B_9Is0_op_22To21Is11(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop2_6To4Is000_B_9Is0_op_22To21Is11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9)=~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21)=~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=001 & op(22:21)=01
//    = BranchToRegister {constraints: }
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
  if ((inst.Bits() & 0x00000070) != 0x00000010 /* op2(6:4)=~001 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return BranchToRegisterTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=001 & op(22:21)=11
//    = Unary2RegisterOpNotRmIsPc {constraints: }
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
  if ((inst.Bits() & 0x00000070) != 0x00000010 /* op2(6:4)=~001 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21)=~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=010 & op(22:21)=01
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop2_6To4Is010_op_22To21Is01
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop2_6To4Is010_op_22To21Is01(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop2_6To4Is010_op_22To21Is01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000020 /* op2(6:4)=~010 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=011 & op(22:21)=01
//    = BranchToRegister {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00000070) != 0x00000030 /* op2(6:4)=~011 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return BranchToRegisterTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=110 & op(22:21)=11
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop2_6To4Is110_op_22To21Is11
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop2_6To4Is110_op_22To21Is11(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop2_6To4Is110_op_22To21Is11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000060 /* op2(6:4)=~110 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21)=~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=111 & op(22:21)=01
//    = BreakPointAndConstantPoolHead {constraints: }
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
  if ((inst.Bits() & 0x00000070) != 0x00000070 /* op2(6:4)=~111 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return Immediate16UseTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=111 & op(22:21)=10
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop2_6To4Is111_op_22To21Is10
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop2_6To4Is111_op_22To21Is10(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop2_6To4Is111_op_22To21Is10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000070 /* op2(6:4)=~111 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00400000 /* op(22:21)=~10 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=111 & op(22:21)=11
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop2_6To4Is111_op_22To21Is11
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop2_6To4Is111_op_22To21Is11(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop2_6To4Is111_op_22To21Is11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000070 /* op2(6:4)=~111 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21)=~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000000
//    = CondDecoder {constraints: }
class CondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000000
    : public CondDecoderTester {
 public:
  CondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000000(const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000000
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000000 /* op2(7:0)=~00000000 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000001
//    = CondDecoder {constraints: }
class CondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000001
    : public CondDecoderTester {
 public:
  CondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000001(const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000001
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000001 /* op2(7:0)=~00000001 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000010
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000010
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000010(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000010
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000002 /* op2(7:0)=~00000010 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000011
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000011
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000011(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000011
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000003 /* op2(7:0)=~00000011 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000100
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000100
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000100(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000100
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000004 /* op2(7:0)=~00000100 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=1111xxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x000000F0 /* op2(7:0)=~1111xxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(22)=0 & op1(19:16)=0100
//    = MoveImmediate12ToApsr {constraints: }
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
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00040000 /* op1(19:16)=~0100 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveImmediate12ToApsrTester::
      PassesParsePreconditions(inst, decoder);
}

// op(22)=0 & op1(19:16)=1x00
//    = MoveImmediate12ToApsr {constraints: }
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
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000B0000) != 0x00080000 /* op1(19:16)=~1x00 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveImmediate12ToApsrTester::
      PassesParsePreconditions(inst, decoder);
}

// op(22)=0 & op1(19:16)=xx01
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_22Is0_op1_19To16Isxx01
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_22Is0_op1_19To16Isxx01(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_22Is0_op1_19To16Isxx01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x00030000) != 0x00010000 /* op1(19:16)=~xx01 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(22)=0 & op1(19:16)=xx1x
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_22Is0_op1_19To16Isxx1x
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_22Is0_op1_19To16Isxx1x(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_22Is0_op1_19To16Isxx1x
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x00020000) != 0x00020000 /* op1(19:16)=~xx1x */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(22)=1
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_22Is1
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_22Is1(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_22Is1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00400000 /* op(22)=~1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=000x
//    = Binary3RegisterOpAltA {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00E00000) != 0x00000000 /* op(23:20)=~000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=001x
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00E00000) != 0x00200000 /* op(23:20)=~001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=0100
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00F00000) != 0x00400000 /* op(23:20)=~0100 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=0101
//    = UndefinedCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_23To20Is0101
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_23To20Is0101(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_23To20Is0101
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00500000 /* op(23:20)=~0101 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=0110
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00F00000) != 0x00600000 /* op(23:20)=~0110 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=0111
//    = UndefinedCondDecoder {constraints: }
class UnsafeCondDecoderTesterop_23To20Is0111
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_23To20Is0111(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_23To20Is0111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00F00000) != 0x00700000 /* op(23:20)=~0111 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=100x
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00E00000) != 0x00800000 /* op(23:20)=~100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=101x
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00E00000) != 0x00A00000 /* op(23:20)=~101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=110x
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00E00000) != 0x00C00000 /* op(23:20)=~110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=111x
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00E00000) != 0x00E00000 /* op(23:20)=~111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// opc3(7:6)=x0
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x00000040) != 0x00000000 /* opc3(7:6)=~x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc2(19:16)=0000 & opc3(7:6)=01
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* opc2(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000040 /* opc3(7:6)=~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc2(19:16)=0000 & opc3(7:6)=11
//    = CondVfpOp {constraints: }
class CondVfpOpTesteropc2_19To16Is0000_opc3_7To6Is11
    : public CondVfpOpTester {
 public:
  CondVfpOpTesteropc2_19To16Is0000_opc3_7To6Is11(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesteropc2_19To16Is0000_opc3_7To6Is11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* opc2(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x000000C0 /* opc3(7:6)=~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc2(19:16)=0001 & opc3(7:6)=01
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x000F0000) != 0x00010000 /* opc2(19:16)=~0001 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000040 /* opc3(7:6)=~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc2(19:16)=0001 & opc3(7:6)=11
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x000F0000) != 0x00010000 /* opc2(19:16)=~0001 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x000000C0 /* opc3(7:6)=~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc2(19:16)=001x & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x000E0000) != 0x00020000 /* opc2(19:16)=~001x */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6)=~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc2(19:16)=0100 & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x000F0000) != 0x00040000 /* opc2(19:16)=~0100 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6)=~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc2(19:16)=0101 & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x000F0000) != 0x00050000 /* opc2(19:16)=~0101 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6)=~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc2(19:16)=0111 & opc3(7:6)=11
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x000F0000) != 0x00070000 /* opc2(19:16)=~0111 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x000000C0 /* opc3(7:6)=~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc2(19:16)=1000 & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x000F0000) != 0x00080000 /* opc2(19:16)=~1000 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6)=~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc2(19:16)=101x & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x000E0000) != 0x000A0000 /* opc2(19:16)=~101x */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6)=~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc2(19:16)=110x & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x000E0000) != 0x000C0000 /* opc2(19:16)=~110x */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6)=~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// opc2(19:16)=111x & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
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
  if ((inst.Bits() & 0x000E0000) != 0x000E0000 /* opc2(19:16)=~111x */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6)=~x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=000 & op2(7:5)=xx0
//    = Binary3RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20)=~000 */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op2(7:5)=~xx0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Is011_A_19To16IsNot1111RegsNotPc
    : public Binary3RegisterImmedShiftedOpTesterRegsNotPc {
 public:
  Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Is011_A_19To16IsNot1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Is011_A_19To16IsNot1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20)=~000 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is000_op2_7To5Is011_A_19To16Is1111RegsNotPc
    : public Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is000_op2_7To5Is011_A_19To16Is1111RegsNotPc(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is000_op2_7To5Is011_A_19To16Is1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20)=~000 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=000 & op2(7:5)=101
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is000_op2_7To5Is101RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is000_op2_7To5Is101RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is000_op2_7To5Is101RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20)=~000 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000A0 /* op2(7:5)=~101 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=01x & op2(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is01x_op2_7To5Isxx0RegsNotPc
    : public Unary2RegisterSatImmedShiftedOpTesterRegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is01x_op2_7To5Isxx0RegsNotPc(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is01x_op2_7To5Isxx0RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op1(22:20)=~01x */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op2(7:5)=~xx0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=010 & op2(7:5)=001
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is010_op2_7To5Is001RegsNotPc
    : public Unary2RegisterSatImmedShiftedOpTesterRegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is010_op2_7To5Is001RegsNotPc(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is010_op2_7To5Is001RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00200000 /* op1(22:20)=~010 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is010_op2_7To5Is011_A_19To16IsNot1111RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is010_op2_7To5Is011_A_19To16IsNot1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is010_op2_7To5Is011_A_19To16IsNot1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00200000 /* op1(22:20)=~010 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is010_op2_7To5Is011_A_19To16Is1111RegsNotPc
    : public Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is010_op2_7To5Is011_A_19To16Is1111RegsNotPc(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is010_op2_7To5Is011_A_19To16Is1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00200000 /* op1(22:20)=~010 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=011 & op2(7:5)=001
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is001RegsNotPc
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is001RegsNotPc(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is001RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00300000 /* op1(22:20)=~011 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is011_op2_7To5Is011_A_19To16IsNot1111RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is011_op2_7To5Is011_A_19To16IsNot1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is011_op2_7To5Is011_A_19To16IsNot1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00300000 /* op1(22:20)=~011 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is011_A_19To16Is1111RegsNotPc
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is011_A_19To16Is1111RegsNotPc(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is011_A_19To16Is1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00300000 /* op1(22:20)=~011 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=011 & op2(7:5)=101
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is101RegsNotPc
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is101RegsNotPc(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is101RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00300000 /* op1(22:20)=~011 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000A0 /* op2(7:5)=~101 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is100_op2_7To5Is011_A_19To16IsNot1111RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is100_op2_7To5Is011_A_19To16IsNot1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is100_op2_7To5Is011_A_19To16IsNot1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00400000 /* op1(22:20)=~100 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is100_op2_7To5Is011_A_19To16Is1111RegsNotPc
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is100_op2_7To5Is011_A_19To16Is1111RegsNotPc(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is100_op2_7To5Is011_A_19To16Is1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00400000 /* op1(22:20)=~100 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=11x & op2(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is11x_op2_7To5Isxx0RegsNotPc
    : public Unary2RegisterSatImmedShiftedOpTesterRegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is11x_op2_7To5Isxx0RegsNotPc(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is11x_op2_7To5Isxx0RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op1(22:20)=~11x */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op2(7:5)=~xx0 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=110 & op2(7:5)=001
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is110_op2_7To5Is001RegsNotPc
    : public Unary2RegisterSatImmedShiftedOpTesterRegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is110_op2_7To5Is001RegsNotPc(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is110_op2_7To5Is001RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00600000 /* op1(22:20)=~110 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is110_op2_7To5Is011_A_19To16IsNot1111RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is110_op2_7To5Is011_A_19To16IsNot1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is110_op2_7To5Is011_A_19To16IsNot1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00600000 /* op1(22:20)=~110 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is110_op2_7To5Is011_A_19To16Is1111RegsNotPc
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is110_op2_7To5Is011_A_19To16Is1111RegsNotPc(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is110_op2_7To5Is011_A_19To16Is1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00600000 /* op1(22:20)=~110 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=111 & op2(7:5)=001
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is001RegsNotPc
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is001RegsNotPc(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is001RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00700000 /* op1(22:20)=~111 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is111_op2_7To5Is011_A_19To16IsNot1111RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is111_op2_7To5Is011_A_19To16IsNot1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is111_op2_7To5Is011_A_19To16IsNot1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00700000 /* op1(22:20)=~111 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is011_A_19To16Is1111RegsNotPc
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is011_A_19To16Is1111RegsNotPc(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is011_A_19To16Is1111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00700000 /* op1(22:20)=~111 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=111 & op2(7:5)=101
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is101RegsNotPc
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is101RegsNotPc(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is101RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00700000 /* op1(22:20)=~111 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000A0 /* op2(7:5)=~101 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=01 & op2(7:5)=000
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is000RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is000RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is000RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=01 & op2(7:5)=001
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is001RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is001RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is001RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=01 & op2(7:5)=010
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is010RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is010RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is010RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000040 /* op2(7:5)=~010 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=01 & op2(7:5)=011
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is011RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is011RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is011RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=01 & op2(7:5)=100
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is100RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is100RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is100RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000080 /* op2(7:5)=~100 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=01 & op2(7:5)=111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is111RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00100000 /* op1(21:20)=~01 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000E0 /* op2(7:5)=~111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=10 & op2(7:5)=000
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is000RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is000RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is000RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=10 & op2(7:5)=001
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is001RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is001RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is001RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=10 & op2(7:5)=010
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is010RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is010RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is010RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000040 /* op2(7:5)=~010 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=10 & op2(7:5)=011
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is011RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is011RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is011RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=10 & op2(7:5)=100
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is100RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is100RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is100RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000080 /* op2(7:5)=~100 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=10 & op2(7:5)=111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is111RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is111RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00300000) != 0x00200000 /* op1(21:20)=~10 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000E0 /* op2(7:5)=~111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=11 & op2(7:5)=000
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=11 & op2(7:5)=001
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=11 & op2(7:5)=010
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000040 /* op2(7:5)=~010 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=11 & op2(7:5)=011
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=11 & op2(7:5)=100
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000080 /* op2(7:5)=~100 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(21:20)=11 & op2(7:5)=111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00300000) != 0x00300000 /* op1(21:20)=~11 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000E0 /* op2(7:5)=~111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op(22:21)=00
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is00RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is00RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is00RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00000000 /* op(22:21)=~00 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op(22:21)=01
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is01RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is01RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is01RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op(22:21)=10
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is10RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is10RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is10RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00400000 /* op(22:21)=~10 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op(22:21)=11
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is11RegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is11RegsNotPc(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is11RegsNotPc
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21)=~11 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc
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

// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20)=~000 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000000 /* op2(7:5)=~00x */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is01x_A_15To12IsNot1111RegsNotPc
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is01x_A_15To12IsNot1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is01x_A_15To12IsNot1111RegsNotPc
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

// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20)=~000 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000040 /* op2(7:5)=~01x */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=001 & op2(7:5)=000
//    = Binary3RegisterOpAltA {constraints: }
class Binary3RegisterOpAltATesterop1_22To20Is001_op2_7To5Is000
    : public Binary3RegisterOpAltATester {
 public:
  Binary3RegisterOpAltATesterop1_22To20Is001_op2_7To5Is000(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterop1_22To20Is001_op2_7To5Is000
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00100000 /* op1(22:20)=~001 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATester::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=011 & op2(7:5)=000
//    = Binary3RegisterOpAltA {constraints: }
class Binary3RegisterOpAltATesterop1_22To20Is011_op2_7To5Is000
    : public Binary3RegisterOpAltATester {
 public:
  Binary3RegisterOpAltATesterop1_22To20Is011_op2_7To5Is000(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterop1_22To20Is011_op2_7To5Is000
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00300000 /* op1(22:20)=~011 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000000 /* op2(7:5)=~000 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATester::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=100 & op2(7:5)=00x
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00700000) != 0x00400000 /* op1(22:20)=~100 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000000 /* op2(7:5)=~00x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=100 & op2(7:5)=01x
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00700000) != 0x00400000 /* op1(22:20)=~100 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000040 /* op2(7:5)=~01x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc
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

// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00700000) != 0x00500000 /* op1(22:20)=~101 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x00000000 /* op2(7:5)=~00x */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* A(15:12)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op1(22:20)=101 & op2(7:5)=11x
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
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
  if ((inst.Bits() & 0x00700000) != 0x00500000 /* op1(22:20)=~101 */) return false;
  if ((inst.Bits() & 0x000000C0) != 0x000000C0 /* op2(7:5)=~11x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=0x00
//    = Deprecated {constraints: }
class UnsafeCondDecoderTesterop_23To20Is0x00
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterop_23To20Is0x00(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterop_23To20Is0x00
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00000000 /* op(23:20)=~0x00 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=1000
//    = StoreExclusive3RegisterOp {constraints: }
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
  if ((inst.Bits() & 0x00F00000) != 0x00800000 /* op(23:20)=~1000 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreExclusive3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=1001
//    = LoadExclusive2RegisterOp {constraints: }
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
  if ((inst.Bits() & 0x00F00000) != 0x00900000 /* op(23:20)=~1001 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadExclusive2RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=1010
//    = StoreExclusive3RegisterDoubleOp {constraints: }
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
  if ((inst.Bits() & 0x00F00000) != 0x00A00000 /* op(23:20)=~1010 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreExclusive3RegisterDoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=1011
//    = LoadExclusive2RegisterDoubleOp {constraints: }
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
  if ((inst.Bits() & 0x00F00000) != 0x00B00000 /* op(23:20)=~1011 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadExclusive2RegisterDoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=1100
//    = StoreExclusive3RegisterOp {constraints: }
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
  if ((inst.Bits() & 0x00F00000) != 0x00C00000 /* op(23:20)=~1100 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreExclusive3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=1101
//    = LoadExclusive2RegisterOp {constraints: }
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
  if ((inst.Bits() & 0x00F00000) != 0x00D00000 /* op(23:20)=~1101 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadExclusive2RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=1110
//    = StoreExclusive3RegisterOp {constraints: }
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
  if ((inst.Bits() & 0x00F00000) != 0x00E00000 /* op(23:20)=~1110 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreExclusive3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op(23:20)=1111
//    = LoadExclusive2RegisterOp {constraints: }
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
  if ((inst.Bits() & 0x00F00000) != 0x00F00000 /* op(23:20)=~1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadExclusive2RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// L(20)=0 & C(8)=0 & A(23:21)=000
//    = MoveVfpRegisterOp {constraints: }
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
  if ((inst.Bits() & 0x00100000) != 0x00000000 /* L(20)=~0 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000000 /* C(8)=~0 */) return false;
  if ((inst.Bits() & 0x00E00000) != 0x00000000 /* A(23:21)=~000 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// L(20)=0 & C(8)=0 & A(23:21)=111
//    = VfpUsesRegOp {constraints: }
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
  if ((inst.Bits() & 0x00100000) != 0x00000000 /* L(20)=~0 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000000 /* C(8)=~0 */) return false;
  if ((inst.Bits() & 0x00E00000) != 0x00E00000 /* A(23:21)=~111 */) return false;

  // Check other preconditions defined for the base decoder.
  return VfpUsesRegOpTester::
      PassesParsePreconditions(inst, decoder);
}

// L(20)=0 & C(8)=1 & A(23:21)=0xx
//    = MoveVfpRegisterOpWithTypeSel {constraints: }
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
  if ((inst.Bits() & 0x00100000) != 0x00000000 /* L(20)=~0 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000100 /* C(8)=~1 */) return false;
  if ((inst.Bits() & 0x00800000) != 0x00000000 /* A(23:21)=~0xx */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpWithTypeSelTester::
      PassesParsePreconditions(inst, decoder);
}

// L(20)=0 & C(8)=1 & A(23:21)=1xx & B(6:5)=0x
//    = DuplicateToAdvSIMDRegisters {constraints: }
class DuplicateToAdvSIMDRegistersTesterL_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x
    : public DuplicateToAdvSIMDRegistersTester {
 public:
  DuplicateToAdvSIMDRegistersTesterL_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x(const NamedClassDecoder& decoder)
    : DuplicateToAdvSIMDRegistersTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool DuplicateToAdvSIMDRegistersTesterL_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00100000) != 0x00000000 /* L(20)=~0 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000100 /* C(8)=~1 */) return false;
  if ((inst.Bits() & 0x00800000) != 0x00800000 /* A(23:21)=~1xx */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000000 /* B(6:5)=~0x */) return false;

  // Check other preconditions defined for the base decoder.
  return DuplicateToAdvSIMDRegistersTester::
      PassesParsePreconditions(inst, decoder);
}

// L(20)=1 & C(8)=0 & A(23:21)=000
//    = MoveVfpRegisterOp {constraints: }
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
  if ((inst.Bits() & 0x00100000) != 0x00100000 /* L(20)=~1 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000000 /* C(8)=~0 */) return false;
  if ((inst.Bits() & 0x00E00000) != 0x00000000 /* A(23:21)=~000 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// L(20)=1 & C(8)=0 & A(23:21)=111
//    = VfpMrsOp {constraints: }
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
  if ((inst.Bits() & 0x00100000) != 0x00100000 /* L(20)=~1 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000000 /* C(8)=~0 */) return false;
  if ((inst.Bits() & 0x00E00000) != 0x00E00000 /* A(23:21)=~111 */) return false;

  // Check other preconditions defined for the base decoder.
  return VfpMrsOpTester::
      PassesParsePreconditions(inst, decoder);
}

// L(20)=1 & C(8)=1
//    = MoveVfpRegisterOpWithTypeSel {constraints: }
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
  if ((inst.Bits() & 0x00100000) != 0x00100000 /* L(20)=~1 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000100 /* C(8)=~1 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveVfpRegisterOpWithTypeSelTester::
      PassesParsePreconditions(inst, decoder);
}

// C(8)=0 & op(7:4)=00x1
//    = MoveDoubleVfpRegisterOp {constraints: }
class MoveDoubleVfpRegisterOpTesterC_8Is0_op_7To4Is00x1
    : public MoveDoubleVfpRegisterOpTester {
 public:
  MoveDoubleVfpRegisterOpTesterC_8Is0_op_7To4Is00x1(const NamedClassDecoder& decoder)
    : MoveDoubleVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveDoubleVfpRegisterOpTesterC_8Is0_op_7To4Is00x1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000100) != 0x00000000 /* C(8)=~0 */) return false;
  if ((inst.Bits() & 0x000000D0) != 0x00000010 /* op(7:4)=~00x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveDoubleVfpRegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// C(8)=1 & op(7:4)=00x1
//    = MoveDoubleVfpRegisterOp {constraints: }
class MoveDoubleVfpRegisterOpTesterC_8Is1_op_7To4Is00x1
    : public MoveDoubleVfpRegisterOpTester {
 public:
  MoveDoubleVfpRegisterOpTesterC_8Is1_op_7To4Is00x1(const NamedClassDecoder& decoder)
    : MoveDoubleVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveDoubleVfpRegisterOpTesterC_8Is1_op_7To4Is00x1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000100) != 0x00000100 /* C(8)=~1 */) return false;
  if ((inst.Bits() & 0x000000D0) != 0x00000010 /* op(7:4)=~00x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveDoubleVfpRegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(27:20)=100xx1x0
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_27To20Is100xx1x0
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_27To20Is100xx1x0(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_27To20Is100xx1x0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E500000) != 0x08400000 /* op1(27:20)=~100xx1x0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(27:20)=100xx0x1
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_27To20Is100xx0x1
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_27To20Is100xx0x1(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_27To20Is100xx0x1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E500000) != 0x08100000 /* op1(27:20)=~100xx0x1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(27:20)=101xxxxx
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_27To20Is101xxxxx
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_27To20Is101xxxxx(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_27To20Is101xxxxx
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E000000) != 0x0A000000 /* op1(27:20)=~101xxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(27:20)=110xxxx0 & op1_repeated(27:20)=~11000x00
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_27To20Is110xxxx0_op1_repeated_27To20IsNot11000x00
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_27To20Is110xxxx0_op1_repeated_27To20IsNot11000x00(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_27To20Is110xxxx0_op1_repeated_27To20IsNot11000x00
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E100000) != 0x0C000000 /* op1(27:20)=~110xxxx0 */) return false;
  if ((inst.Bits() & 0x0FB00000) == 0x0C000000 /* op1_repeated(27:20)=11000x00 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(27:20)=110xxxx1 & Rn(19:16)=~1111 & op1_repeated(27:20)=~11000x01
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_27To20Is110xxxx1_Rn_19To16IsNot1111_op1_repeated_27To20IsNot11000x01
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_27To20Is110xxxx1_Rn_19To16IsNot1111_op1_repeated_27To20IsNot11000x01(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_27To20Is110xxxx1_Rn_19To16IsNot1111_op1_repeated_27To20IsNot11000x01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E100000) != 0x0C100000 /* op1(27:20)=~110xxxx1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;
  if ((inst.Bits() & 0x0FB00000) == 0x0C100000 /* op1_repeated(27:20)=11000x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(27:20)=110xxxx1 & Rn(19:16)=1111 & op1_repeated(27:20)=~11000x01
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_27To20Is110xxxx1_Rn_19To16Is1111_op1_repeated_27To20IsNot11000x01
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_27To20Is110xxxx1_Rn_19To16Is1111_op1_repeated_27To20IsNot11000x01(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_27To20Is110xxxx1_Rn_19To16Is1111_op1_repeated_27To20IsNot11000x01
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E100000) != 0x0C100000 /* op1(27:20)=~110xxxx1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x0FB00000) == 0x0C100000 /* op1_repeated(27:20)=11000x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(27:20)=11000100
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_27To20Is11000100
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_27To20Is11000100(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_27To20Is11000100
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0FF00000) != 0x0C400000 /* op1(27:20)=~11000100 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(27:20)=11000101
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_27To20Is11000101
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_27To20Is11000101(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_27To20Is11000101
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0FF00000) != 0x0C500000 /* op1(27:20)=~11000101 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(27:20)=1110xxxx & op(4)=0
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_27To20Is1110xxxx_op_4Is0
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_27To20Is1110xxxx_op_4Is0(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_27To20Is1110xxxx_op_4Is0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0F000000) != 0x0E000000 /* op1(27:20)=~1110xxxx */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* op(4)=~0 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(27:20)=1110xxx0 & op(4)=1
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_27To20Is1110xxx0_op_4Is1
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_27To20Is1110xxx0_op_4Is1(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_27To20Is1110xxx0_op_4Is1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0F100000) != 0x0E000000 /* op1(27:20)=~1110xxx0 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000010 /* op(4)=~1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(27:20)=1110xxx1 & op(4)=1
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterop1_27To20Is1110xxx1_op_4Is1
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterop1_27To20Is1110xxx1_op_4Is1(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterop1_27To20Is1110xxx1_op_4Is1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0F100000) != 0x0E100000 /* op1(27:20)=~1110xxx1 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000010 /* op(4)=~1 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// op(25:20)=0000x0
//    = StoreRegisterList {constraints: ,
//     rule: 'Stmda_Stmed_Rule_190_A1_P376'}
class StoreRegisterListTester_op_25To20Is0000x0_Stmda_Stmed_Rule_190_A1_P376
    : public LoadStoreRegisterListTesterop_25To20Is0000x0 {
 public:
  StoreRegisterListTester_op_25To20Is0000x0_Stmda_Stmed_Rule_190_A1_P376()
    : LoadStoreRegisterListTesterop_25To20Is0000x0(
      state_.StoreRegisterList_Stmda_Stmed_Rule_190_A1_P376_instance_)
  {}
};

// op(25:20)=0000x1
//    = LoadRegisterList {constraints: ,
//     rule: 'Ldmda_Ldmfa_Rule_54_A1_P112'}
class LoadRegisterListTester_op_25To20Is0000x1_Ldmda_Ldmfa_Rule_54_A1_P112
    : public LoadStoreRegisterListTesterop_25To20Is0000x1 {
 public:
  LoadRegisterListTester_op_25To20Is0000x1_Ldmda_Ldmfa_Rule_54_A1_P112()
    : LoadStoreRegisterListTesterop_25To20Is0000x1(
      state_.LoadRegisterList_Ldmda_Ldmfa_Rule_54_A1_P112_instance_)
  {}
};

// op(25:20)=0010x0
//    = StoreRegisterList {constraints: ,
//     rule: 'Stm_Stmia_Stmea_Rule_189_A1_P374'}
class StoreRegisterListTester_op_25To20Is0010x0_Stm_Stmia_Stmea_Rule_189_A1_P374
    : public LoadStoreRegisterListTesterop_25To20Is0010x0 {
 public:
  StoreRegisterListTester_op_25To20Is0010x0_Stm_Stmia_Stmea_Rule_189_A1_P374()
    : LoadStoreRegisterListTesterop_25To20Is0010x0(
      state_.StoreRegisterList_Stm_Stmia_Stmea_Rule_189_A1_P374_instance_)
  {}
};

// op(25:20)=001001
//    = LoadRegisterList {constraints: ,
//     rule: 'Ldm_Ldmia_Ldmfd_Rule_53_A1_P110'}
class LoadRegisterListTester_op_25To20Is001001_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110
    : public LoadStoreRegisterListTesterop_25To20Is001001 {
 public:
  LoadRegisterListTester_op_25To20Is001001_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110()
    : LoadStoreRegisterListTesterop_25To20Is001001(
      state_.LoadRegisterList_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110_instance_)
  {}
};

// op(25:20)=001011 & Rn(19:16)=~1101
//    = LoadRegisterList {constraints: ,
//     rule: 'Ldm_Ldmia_Ldmfd_Rule_53_A1_P110'}
class LoadRegisterListTester_op_25To20Is001011_Rn_19To16IsNot1101_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110
    : public LoadStoreRegisterListTesterop_25To20Is001011_Rn_19To16IsNot1101 {
 public:
  LoadRegisterListTester_op_25To20Is001011_Rn_19To16IsNot1101_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110()
    : LoadStoreRegisterListTesterop_25To20Is001011_Rn_19To16IsNot1101(
      state_.LoadRegisterList_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110_instance_)
  {}
};

// op(25:20)=001011 & Rn(19:16)=1101
//    = LoadRegisterList {constraints: ,
//     rule: 'Pop_Rule_A1'}
class LoadRegisterListTester_op_25To20Is001011_Rn_19To16Is1101_Pop_Rule_A1
    : public LoadStoreRegisterListTesterop_25To20Is001011_Rn_19To16Is1101 {
 public:
  LoadRegisterListTester_op_25To20Is001011_Rn_19To16Is1101_Pop_Rule_A1()
    : LoadStoreRegisterListTesterop_25To20Is001011_Rn_19To16Is1101(
      state_.LoadRegisterList_Pop_Rule_A1_instance_)
  {}
};

// op(25:20)=010000
//    = StoreRegisterList {constraints: ,
//     rule: 'Stmdb_Stmfd_Rule_191_A1_P378'}
class StoreRegisterListTester_op_25To20Is010000_Stmdb_Stmfd_Rule_191_A1_P378
    : public LoadStoreRegisterListTesterop_25To20Is010000 {
 public:
  StoreRegisterListTester_op_25To20Is010000_Stmdb_Stmfd_Rule_191_A1_P378()
    : LoadStoreRegisterListTesterop_25To20Is010000(
      state_.StoreRegisterList_Stmdb_Stmfd_Rule_191_A1_P378_instance_)
  {}
};

// op(25:20)=010010 & Rn(19:16)=~1101
//    = StoreRegisterList {constraints: ,
//     rule: 'Stmdb_Stmfd_Rule_191_A1_P378'}
class StoreRegisterListTester_op_25To20Is010010_Rn_19To16IsNot1101_Stmdb_Stmfd_Rule_191_A1_P378
    : public LoadStoreRegisterListTesterop_25To20Is010010_Rn_19To16IsNot1101 {
 public:
  StoreRegisterListTester_op_25To20Is010010_Rn_19To16IsNot1101_Stmdb_Stmfd_Rule_191_A1_P378()
    : LoadStoreRegisterListTesterop_25To20Is010010_Rn_19To16IsNot1101(
      state_.StoreRegisterList_Stmdb_Stmfd_Rule_191_A1_P378_instance_)
  {}
};

// op(25:20)=010010 & Rn(19:16)=1101
//    = StoreRegisterList {constraints: ,
//     rule: 'Push_Rule_A1'}
class StoreRegisterListTester_op_25To20Is010010_Rn_19To16Is1101_Push_Rule_A1
    : public LoadStoreRegisterListTesterop_25To20Is010010_Rn_19To16Is1101 {
 public:
  StoreRegisterListTester_op_25To20Is010010_Rn_19To16Is1101_Push_Rule_A1()
    : LoadStoreRegisterListTesterop_25To20Is010010_Rn_19To16Is1101(
      state_.StoreRegisterList_Push_Rule_A1_instance_)
  {}
};

// op(25:20)=0100x1
//    = LoadRegisterList {constraints: ,
//     rule: 'Ldmdb_Ldmea_Rule_55_A1_P114'}
class LoadRegisterListTester_op_25To20Is0100x1_Ldmdb_Ldmea_Rule_55_A1_P114
    : public LoadStoreRegisterListTesterop_25To20Is0100x1 {
 public:
  LoadRegisterListTester_op_25To20Is0100x1_Ldmdb_Ldmea_Rule_55_A1_P114()
    : LoadStoreRegisterListTesterop_25To20Is0100x1(
      state_.LoadRegisterList_Ldmdb_Ldmea_Rule_55_A1_P114_instance_)
  {}
};

// op(25:20)=0110x0
//    = StoreRegisterList {constraints: ,
//     rule: 'Stmib_Stmfa_Rule_192_A1_P380'}
class StoreRegisterListTester_op_25To20Is0110x0_Stmib_Stmfa_Rule_192_A1_P380
    : public LoadStoreRegisterListTesterop_25To20Is0110x0 {
 public:
  StoreRegisterListTester_op_25To20Is0110x0_Stmib_Stmfa_Rule_192_A1_P380()
    : LoadStoreRegisterListTesterop_25To20Is0110x0(
      state_.StoreRegisterList_Stmib_Stmfa_Rule_192_A1_P380_instance_)
  {}
};

// op(25:20)=0110x1
//    = LoadRegisterList {constraints: ,
//     rule: 'Ldmib_Ldmed_Rule_56_A1_P116'}
class LoadRegisterListTester_op_25To20Is0110x1_Ldmib_Ldmed_Rule_56_A1_P116
    : public LoadStoreRegisterListTesterop_25To20Is0110x1 {
 public:
  LoadRegisterListTester_op_25To20Is0110x1_Ldmib_Ldmed_Rule_56_A1_P116()
    : LoadStoreRegisterListTesterop_25To20Is0110x1(
      state_.LoadRegisterList_Ldmib_Ldmed_Rule_56_A1_P116_instance_)
  {}
};

// op(25:20)=0xx1x0
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Stm_Rule_11_B6_A1_P22'}
class ForbiddenCondDecoderTester_op_25To20Is0xx1x0_Stm_Rule_11_B6_A1_P22
    : public UnsafeCondDecoderTesterop_25To20Is0xx1x0 {
 public:
  ForbiddenCondDecoderTester_op_25To20Is0xx1x0_Stm_Rule_11_B6_A1_P22()
    : UnsafeCondDecoderTesterop_25To20Is0xx1x0(
      state_.ForbiddenCondDecoder_Stm_Rule_11_B6_A1_P22_instance_)
  {}
};

// op(25:20)=0xx1x1 & R(15)=0
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Ldm_Rule_3_B6_A1_P7'}
class ForbiddenCondDecoderTester_op_25To20Is0xx1x1_R_15Is0_Ldm_Rule_3_B6_A1_P7
    : public UnsafeCondDecoderTesterop_25To20Is0xx1x1_R_15Is0 {
 public:
  ForbiddenCondDecoderTester_op_25To20Is0xx1x1_R_15Is0_Ldm_Rule_3_B6_A1_P7()
    : UnsafeCondDecoderTesterop_25To20Is0xx1x1_R_15Is0(
      state_.ForbiddenCondDecoder_Ldm_Rule_3_B6_A1_P7_instance_)
  {}
};

// op(25:20)=0xx1x1 & R(15)=1
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Ldm_Rule_2_B6_A1_P5'}
class ForbiddenCondDecoderTester_op_25To20Is0xx1x1_R_15Is1_Ldm_Rule_2_B6_A1_P5
    : public UnsafeCondDecoderTesterop_25To20Is0xx1x1_R_15Is1 {
 public:
  ForbiddenCondDecoderTester_op_25To20Is0xx1x1_R_15Is1_Ldm_Rule_2_B6_A1_P5()
    : UnsafeCondDecoderTesterop_25To20Is0xx1x1_R_15Is1(
      state_.ForbiddenCondDecoder_Ldm_Rule_2_B6_A1_P5_instance_)
  {}
};

// op(25:20)=10xxxx
//    = BranchImmediate24 {constraints: ,
//     rule: 'B_Rule_16_A1_P44'}
class BranchImmediate24Tester_op_25To20Is10xxxx_B_Rule_16_A1_P44
    : public BranchImmediate24Testerop_25To20Is10xxxx {
 public:
  BranchImmediate24Tester_op_25To20Is10xxxx_B_Rule_16_A1_P44()
    : BranchImmediate24Testerop_25To20Is10xxxx(
      state_.BranchImmediate24_B_Rule_16_A1_P44_instance_)
  {}
};

// op(25:20)=11xxxx
//    = BranchImmediate24 {constraints: ,
//     rule: 'Bl_Blx_Rule_23_A1_P58'}
class BranchImmediate24Tester_op_25To20Is11xxxx_Bl_Blx_Rule_23_A1_P58
    : public BranchImmediate24Testerop_25To20Is11xxxx {
 public:
  BranchImmediate24Tester_op_25To20Is11xxxx_Bl_Blx_Rule_23_A1_P58()
    : BranchImmediate24Testerop_25To20Is11xxxx(
      state_.BranchImmediate24_Bl_Blx_Rule_23_A1_P58_instance_)
  {}
};

// op1(25:20)=00000x
//    = UndefinedCondDecoder {constraints: ,
//     rule: 'Undefined_A5_6'}
class UndefinedCondDecoderTester_op1_25To20Is00000x_Undefined_A5_6
    : public UnsafeCondDecoderTesterop1_25To20Is00000x {
 public:
  UndefinedCondDecoderTester_op1_25To20Is00000x_Undefined_A5_6()
    : UnsafeCondDecoderTesterop1_25To20Is00000x(
      state_.UndefinedCondDecoder_Undefined_A5_6_instance_)
  {}
};

// op1(25:20)=11xxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Svc_Rule_A1'}
class ForbiddenCondDecoderTester_op1_25To20Is11xxxx_Svc_Rule_A1
    : public UnsafeCondDecoderTesterop1_25To20Is11xxxx {
 public:
  ForbiddenCondDecoderTester_op1_25To20Is11xxxx_Svc_Rule_A1()
    : UnsafeCondDecoderTesterop1_25To20Is11xxxx(
      state_.ForbiddenCondDecoder_Svc_Rule_A1_instance_)
  {}
};

// coproc(11:8)=~101x & op1(25:20)=0xxxx0 & op1_repeated(25:20)=~000x00
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Stc_Rule_A2'}
class ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is0xxxx0_op1_repeated_25To20IsNot000x00_Stc_Rule_A2
    : public UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is0xxxx0_op1_repeated_25To20IsNot000x00 {
 public:
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is0xxxx0_op1_repeated_25To20IsNot000x00_Stc_Rule_A2()
    : UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is0xxxx0_op1_repeated_25To20IsNot000x00(
      state_.ForbiddenCondDecoder_Stc_Rule_A2_instance_)
  {}
};

// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=~1111 & op1_repeated(25:20)=~000x01
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Ldc_immediate_Rule_A1'}
class ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16IsNot1111_op1_repeated_25To20IsNot000x01_Ldc_immediate_Rule_A1
    : public UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16IsNot1111_op1_repeated_25To20IsNot000x01 {
 public:
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16IsNot1111_op1_repeated_25To20IsNot000x01_Ldc_immediate_Rule_A1()
    : UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16IsNot1111_op1_repeated_25To20IsNot000x01(
      state_.ForbiddenCondDecoder_Ldc_immediate_Rule_A1_instance_)
  {}
};

// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=1111 & op1_repeated(25:20)=~000x01
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Ldc_literal_Rule_A1'}
class ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16Is1111_op1_repeated_25To20IsNot000x01_Ldc_literal_Rule_A1
    : public UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16Is1111_op1_repeated_25To20IsNot000x01 {
 public:
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16Is1111_op1_repeated_25To20IsNot000x01_Ldc_literal_Rule_A1()
    : UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16Is1111_op1_repeated_25To20IsNot000x01(
      state_.ForbiddenCondDecoder_Ldc_literal_Rule_A1_instance_)
  {}
};

// coproc(11:8)=~101x & op1(25:20)=000100
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Mcrr_Rule_A1'}
class ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is000100_Mcrr_Rule_A1
    : public UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is000100 {
 public:
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is000100_Mcrr_Rule_A1()
    : UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is000100(
      state_.ForbiddenCondDecoder_Mcrr_Rule_A1_instance_)
  {}
};

// coproc(11:8)=~101x & op1(25:20)=000101
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Mrrc_Rule_A1'}
class ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is000101_Mrrc_Rule_A1
    : public UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is000101 {
 public:
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is000101_Mrrc_Rule_A1()
    : UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is000101(
      state_.ForbiddenCondDecoder_Mrrc_Rule_A1_instance_)
  {}
};

// coproc(11:8)=~101x & op1(25:20)=10xxxx & op(4)=0
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Cdp_Rule_A1'}
class ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is10xxxx_op_4Is0_Cdp_Rule_A1
    : public UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is10xxxx_op_4Is0 {
 public:
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is10xxxx_op_4Is0_Cdp_Rule_A1()
    : UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is10xxxx_op_4Is0(
      state_.ForbiddenCondDecoder_Cdp_Rule_A1_instance_)
  {}
};

// coproc(11:8)=~101x & op1(25:20)=10xxx0 & op(4)=1
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Mcr_Rule_A1'}
class ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is10xxx0_op_4Is1_Mcr_Rule_A1
    : public UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is10xxx0_op_4Is1 {
 public:
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is10xxx0_op_4Is1_Mcr_Rule_A1()
    : UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is10xxx0_op_4Is1(
      state_.ForbiddenCondDecoder_Mcr_Rule_A1_instance_)
  {}
};

// coproc(11:8)=~101x & op1(25:20)=10xxx1 & op(4)=1
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Mrc_Rule_A1'}
class ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is10xxx1_op_4Is1_Mrc_Rule_A1
    : public UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is10xxx1_op_4Is1 {
 public:
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is10xxx1_op_4Is1_Mrc_Rule_A1()
    : UnsafeCondDecoderTestercoproc_11To8IsNot101x_op1_25To20Is10xxx1_op_4Is1(
      state_.ForbiddenCondDecoder_Mrc_Rule_A1_instance_)
  {}
};

// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=1011
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'extra_load_store_instructions_unpriviledged'}
class ForbiddenCondDecoderTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011_extra_load_store_instructions_unpriviledged
    : public UnsafeCondDecoderTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011 {
 public:
  ForbiddenCondDecoderTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011_extra_load_store_instructions_unpriviledged()
    : UnsafeCondDecoderTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011(
      state_.ForbiddenCondDecoder_extra_load_store_instructions_unpriviledged_instance_)
  {}
};

// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=11x1
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'extra_load_store_instructions_unpriviledged'}
class ForbiddenCondDecoderTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1_extra_load_store_instructions_unpriviledged
    : public UnsafeCondDecoderTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1 {
 public:
  ForbiddenCondDecoderTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1_extra_load_store_instructions_unpriviledged()
    : UnsafeCondDecoderTesterop_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1(
      state_.ForbiddenCondDecoder_extra_load_store_instructions_unpriviledged_instance_)
  {}
};

// op(25)=1 & op1(24:20)=10000
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: 'Mov_Rule_96_A2_P194',
//     safety: ['RegsNotPc']}
class Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10000RegsNotPc_Mov_Rule_96_A2_P194
    : public Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10000RegsNotPc {
 public:
  Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10000RegsNotPc_Mov_Rule_96_A2_P194()
    : Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10000RegsNotPc(
      state_.Unary1RegisterImmediateOp_Mov_Rule_96_A2_P194_instance_)
  {}
};

// op(25)=1 & op1(24:20)=10100
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: 'Mov_Rule_99_A1_P200',
//     safety: [Rd(15:12)=~1111]}
class Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10100Safety_Rd_15To12IsNot1111_Mov_Rule_99_A1_P200
    : public Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10100Safety_Rd_15To12IsNot1111 {
 public:
  Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10100Safety_Rd_15To12IsNot1111_Mov_Rule_99_A1_P200()
    : Unary1RegisterImmediateOpTesterop_25Is1_op1_24To20Is10100Safety_Rd_15To12IsNot1111(
      state_.Unary1RegisterImmediateOp_Mov_Rule_99_A1_P200_instance_)
  {}
};

// op(24:20)=0000x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: 'And_Rule_11_A1_P34',
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_op_24To20Is0000xNotRdIsPcAndS_And_Rule_11_A1_P34
    : public Binary2RegisterImmediateOpTesterop_24To20Is0000xNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0000xNotRdIsPcAndS_And_Rule_11_A1_P34()
    : Binary2RegisterImmediateOpTesterop_24To20Is0000xNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_And_Rule_11_A1_P34_instance_)
  {}
};

// op(24:20)=0001x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: 'Eor_Rule_44_A1_P94',
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_op_24To20Is0001xNotRdIsPcAndS_Eor_Rule_44_A1_P94
    : public Binary2RegisterImmediateOpTesterop_24To20Is0001xNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0001xNotRdIsPcAndS_Eor_Rule_44_A1_P94()
    : Binary2RegisterImmediateOpTesterop_24To20Is0001xNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Eor_Rule_44_A1_P94_instance_)
  {}
};

// op(24:20)=0010x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: 'Sub_Rule_212_A1_P420',
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
class Binary2RegisterImmediateOpTester_op_24To20Is0010x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Sub_Rule_212_A1_P420
    : public Binary2RegisterImmediateOpTesterop_24To20Is0010x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0010x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Sub_Rule_212_A1_P420()
    : Binary2RegisterImmediateOpTesterop_24To20Is0010x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS(
      state_.Binary2RegisterImmediateOp_Sub_Rule_212_A1_P420_instance_)
  {}
};

// op(24:20)=00100 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: 'Adr_Rule_10_A2_P32'}
class Unary1RegisterImmediateOpTester_op_24To20Is00100_Rn_19To16Is1111_Adr_Rule_10_A2_P32
    : public Unary1RegisterImmediateOpTesterop_24To20Is00100_Rn_19To16Is1111 {
 public:
  Unary1RegisterImmediateOpTester_op_24To20Is00100_Rn_19To16Is1111_Adr_Rule_10_A2_P32()
    : Unary1RegisterImmediateOpTesterop_24To20Is00100_Rn_19To16Is1111(
      state_.Unary1RegisterImmediateOp_Adr_Rule_10_A2_P32_instance_)
  {}
};

// op(24:20)=00101 & Rn(19:16)=1111
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Subs_Pc_Lr_and_related_instructions_Rule_A1a'}
class ForbiddenCondDecoderTester_op_24To20Is00101_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1a
    : public UnsafeCondDecoderTesterop_24To20Is00101_Rn_19To16Is1111 {
 public:
  ForbiddenCondDecoderTester_op_24To20Is00101_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1a()
    : UnsafeCondDecoderTesterop_24To20Is00101_Rn_19To16Is1111(
      state_.ForbiddenCondDecoder_Subs_Pc_Lr_and_related_instructions_Rule_A1a_instance_)
  {}
};

// op(24:20)=0011x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: 'Rsb_Rule_142_A1_P284',
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_op_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_142_A1_P284
    : public Binary2RegisterImmediateOpTesterop_24To20Is0011xNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_142_A1_P284()
    : Binary2RegisterImmediateOpTesterop_24To20Is0011xNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Rsb_Rule_142_A1_P284_instance_)
  {}
};

// op(24:20)=0100x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: 'Add_Rule_5_A1_P22',
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
class Binary2RegisterImmediateOpTester_op_24To20Is0100x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Add_Rule_5_A1_P22
    : public Binary2RegisterImmediateOpTesterop_24To20Is0100x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0100x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Add_Rule_5_A1_P22()
    : Binary2RegisterImmediateOpTesterop_24To20Is0100x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS(
      state_.Binary2RegisterImmediateOp_Add_Rule_5_A1_P22_instance_)
  {}
};

// op(24:20)=01000 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: 'Adr_Rule_10_A1_P32'}
class Unary1RegisterImmediateOpTester_op_24To20Is01000_Rn_19To16Is1111_Adr_Rule_10_A1_P32
    : public Unary1RegisterImmediateOpTesterop_24To20Is01000_Rn_19To16Is1111 {
 public:
  Unary1RegisterImmediateOpTester_op_24To20Is01000_Rn_19To16Is1111_Adr_Rule_10_A1_P32()
    : Unary1RegisterImmediateOpTesterop_24To20Is01000_Rn_19To16Is1111(
      state_.Unary1RegisterImmediateOp_Adr_Rule_10_A1_P32_instance_)
  {}
};

// op(24:20)=01001 & Rn(19:16)=1111
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Subs_Pc_Lr_and_related_instructions_Rule_A1b'}
class ForbiddenCondDecoderTester_op_24To20Is01001_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1b
    : public UnsafeCondDecoderTesterop_24To20Is01001_Rn_19To16Is1111 {
 public:
  ForbiddenCondDecoderTester_op_24To20Is01001_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1b()
    : UnsafeCondDecoderTesterop_24To20Is01001_Rn_19To16Is1111(
      state_.ForbiddenCondDecoder_Subs_Pc_Lr_and_related_instructions_Rule_A1b_instance_)
  {}
};

// op(24:20)=0101x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: 'Adc_Rule_6_A1_P14',
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_op_24To20Is0101xNotRdIsPcAndS_Adc_Rule_6_A1_P14
    : public Binary2RegisterImmediateOpTesterop_24To20Is0101xNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0101xNotRdIsPcAndS_Adc_Rule_6_A1_P14()
    : Binary2RegisterImmediateOpTesterop_24To20Is0101xNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Adc_Rule_6_A1_P14_instance_)
  {}
};

// op(24:20)=0110x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: 'Sbc_Rule_151_A1_P302',
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_op_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_151_A1_P302
    : public Binary2RegisterImmediateOpTesterop_24To20Is0110xNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_151_A1_P302()
    : Binary2RegisterImmediateOpTesterop_24To20Is0110xNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Sbc_Rule_151_A1_P302_instance_)
  {}
};

// op(24:20)=0111x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: 'Rsc_Rule_145_A1_P290',
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_op_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_145_A1_P290
    : public Binary2RegisterImmediateOpTesterop_24To20Is0111xNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_145_A1_P290()
    : Binary2RegisterImmediateOpTesterop_24To20Is0111xNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Rsc_Rule_145_A1_P290_instance_)
  {}
};

// op(24:20)=10001
//    = MaskedBinaryRegisterImmediateTest {constraints: ,
//     rule: 'Tst_Rule_230_A1_P454'}
class MaskedBinaryRegisterImmediateTestTester_op_24To20Is10001_Tst_Rule_230_A1_P454
    : public BinaryRegisterImmediateTestTesterop_24To20Is10001 {
 public:
  MaskedBinaryRegisterImmediateTestTester_op_24To20Is10001_Tst_Rule_230_A1_P454()
    : BinaryRegisterImmediateTestTesterop_24To20Is10001(
      state_.MaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454_instance_)
  {}
};

// op(24:20)=10011
//    = BinaryRegisterImmediateTest {constraints: ,
//     rule: 'Teq_Rule_227_A1_P448'}
class BinaryRegisterImmediateTestTester_op_24To20Is10011_Teq_Rule_227_A1_P448
    : public BinaryRegisterImmediateTestTesterop_24To20Is10011 {
 public:
  BinaryRegisterImmediateTestTester_op_24To20Is10011_Teq_Rule_227_A1_P448()
    : BinaryRegisterImmediateTestTesterop_24To20Is10011(
      state_.BinaryRegisterImmediateTest_Teq_Rule_227_A1_P448_instance_)
  {}
};

// op(24:20)=10101
//    = BinaryRegisterImmediateTest {constraints: ,
//     rule: 'Cmp_Rule_35_A1_P80'}
class BinaryRegisterImmediateTestTester_op_24To20Is10101_Cmp_Rule_35_A1_P80
    : public BinaryRegisterImmediateTestTesterop_24To20Is10101 {
 public:
  BinaryRegisterImmediateTestTester_op_24To20Is10101_Cmp_Rule_35_A1_P80()
    : BinaryRegisterImmediateTestTesterop_24To20Is10101(
      state_.BinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80_instance_)
  {}
};

// op(24:20)=10111
//    = BinaryRegisterImmediateTest {constraints: ,
//     rule: 'Cmn_Rule_32_A1_P74'}
class BinaryRegisterImmediateTestTester_op_24To20Is10111_Cmn_Rule_32_A1_P74
    : public BinaryRegisterImmediateTestTesterop_24To20Is10111 {
 public:
  BinaryRegisterImmediateTestTester_op_24To20Is10111_Cmn_Rule_32_A1_P74()
    : BinaryRegisterImmediateTestTesterop_24To20Is10111(
      state_.BinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74_instance_)
  {}
};

// op(24:20)=1100x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: 'Orr_Rule_113_A1_P228',
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_op_24To20Is1100xNotRdIsPcAndS_Orr_Rule_113_A1_P228
    : public Binary2RegisterImmediateOpTesterop_24To20Is1100xNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTester_op_24To20Is1100xNotRdIsPcAndS_Orr_Rule_113_A1_P228()
    : Binary2RegisterImmediateOpTesterop_24To20Is1100xNotRdIsPcAndS(
      state_.Binary2RegisterImmediateOp_Orr_Rule_113_A1_P228_instance_)
  {}
};

// op(24:20)=1101x
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: 'Mov_Rule_96_A1_P194',
//     safety: ['NotRdIsPcAndS']}
class Unary1RegisterImmediateOpTester_op_24To20Is1101xNotRdIsPcAndS_Mov_Rule_96_A1_P194
    : public Unary1RegisterImmediateOpTesterop_24To20Is1101xNotRdIsPcAndS {
 public:
  Unary1RegisterImmediateOpTester_op_24To20Is1101xNotRdIsPcAndS_Mov_Rule_96_A1_P194()
    : Unary1RegisterImmediateOpTesterop_24To20Is1101xNotRdIsPcAndS(
      state_.Unary1RegisterImmediateOp_Mov_Rule_96_A1_P194_instance_)
  {}
};

// op(24:20)=1110x
//    = MaskedBinary2RegisterImmediateOp {constraints: ,
//     rule: 'Bic_Rule_19_A1_P50',
//     safety: ['NotRdIsPcAndS']}
class MaskedBinary2RegisterImmediateOpTester_op_24To20Is1110xNotRdIsPcAndS_Bic_Rule_19_A1_P50
    : public Binary2RegisterImmediateOpTesterop_24To20Is1110xNotRdIsPcAndS {
 public:
  MaskedBinary2RegisterImmediateOpTester_op_24To20Is1110xNotRdIsPcAndS_Bic_Rule_19_A1_P50()
    : Binary2RegisterImmediateOpTesterop_24To20Is1110xNotRdIsPcAndS(
      state_.MaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50_instance_)
  {}
};

// op(24:20)=1111x
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: 'Mvn_Rule_106_A1_P214',
//     safety: ['NotRdIsPcAndS']}
class Unary1RegisterImmediateOpTester_op_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_106_A1_P214
    : public Unary1RegisterImmediateOpTesterop_24To20Is1111xNotRdIsPcAndS {
 public:
  Unary1RegisterImmediateOpTester_op_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_106_A1_P214()
    : Unary1RegisterImmediateOpTesterop_24To20Is1111xNotRdIsPcAndS(
      state_.Unary1RegisterImmediateOp_Mvn_Rule_106_A1_P214_instance_)
  {}
};

// op1(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: 'And_Rule_7_A1_P36',
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0000xNotRdIsPcAndS_And_Rule_7_A1_P36
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0000xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0000xNotRdIsPcAndS_And_Rule_7_A1_P36()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0000xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_And_Rule_7_A1_P36_instance_)
  {}
};

// op1(24:20)=0001x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: 'Eor_Rule_45_A1_P96',
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0001xNotRdIsPcAndS_Eor_Rule_45_A1_P96
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0001xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0001xNotRdIsPcAndS_Eor_Rule_45_A1_P96()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0001xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96_instance_)
  {}
};

// op1(24:20)=0010x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: 'Sub_Rule_213_A1_P422',
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0010xNotRdIsPcAndS_Sub_Rule_213_A1_P422
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0010xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0010xNotRdIsPcAndS_Sub_Rule_213_A1_P422()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0010xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Sub_Rule_213_A1_P422_instance_)
  {}
};

// op1(24:20)=0011x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: 'Rsb_Rule_143_P286',
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_143_P286
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0011xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_143_P286()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0011xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Rsb_Rule_143_P286_instance_)
  {}
};

// op1(24:20)=0100x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: 'Add_Rule_6_A1_P24',
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0100xNotRdIsPcAndS_Add_Rule_6_A1_P24
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0100xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0100xNotRdIsPcAndS_Add_Rule_6_A1_P24()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0100xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24_instance_)
  {}
};

// op1(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: 'Adc_Rule_2_A1_P16',
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0101xNotRdIsPcAndS_Adc_Rule_2_A1_P16
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0101xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0101xNotRdIsPcAndS_Adc_Rule_2_A1_P16()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0101xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16_instance_)
  {}
};

// op1(24:20)=0110x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: 'Sbc_Rule_152_A1_P304',
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_152_A1_P304
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0110xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_152_A1_P304()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0110xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304_instance_)
  {}
};

// op1(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: 'Rsc_Rule_146_A1_P292',
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_op1_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_146_A1_P292
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is0111xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_146_A1_P292()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is0111xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292_instance_)
  {}
};

// op1(24:20)=10001
//    = Binary2RegisterImmedShiftedTest {constraints: ,
//     rule: 'Tst_Rule_231_A1_P456'}
class Binary2RegisterImmedShiftedTestTester_op1_24To20Is10001_Tst_Rule_231_A1_P456
    : public Binary2RegisterImmedShiftedTestTesterop1_24To20Is10001 {
 public:
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10001_Tst_Rule_231_A1_P456()
    : Binary2RegisterImmedShiftedTestTesterop1_24To20Is10001(
      state_.Binary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456_instance_)
  {}
};

// op1(24:20)=10011
//    = Binary2RegisterImmedShiftedTest {constraints: ,
//     rule: 'Teq_Rule_228_A1_P450'}
class Binary2RegisterImmedShiftedTestTester_op1_24To20Is10011_Teq_Rule_228_A1_P450
    : public Binary2RegisterImmedShiftedTestTesterop1_24To20Is10011 {
 public:
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10011_Teq_Rule_228_A1_P450()
    : Binary2RegisterImmedShiftedTestTesterop1_24To20Is10011(
      state_.Binary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450_instance_)
  {}
};

// op1(24:20)=10101
//    = Binary2RegisterImmedShiftedTest {constraints: ,
//     rule: 'Cmp_Rule_36_A1_P82'}
class Binary2RegisterImmedShiftedTestTester_op1_24To20Is10101_Cmp_Rule_36_A1_P82
    : public Binary2RegisterImmedShiftedTestTesterop1_24To20Is10101 {
 public:
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10101_Cmp_Rule_36_A1_P82()
    : Binary2RegisterImmedShiftedTestTesterop1_24To20Is10101(
      state_.Binary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82_instance_)
  {}
};

// op1(24:20)=10111
//    = Binary2RegisterImmedShiftedTest {constraints: ,
//     rule: 'Cmn_Rule_33_A1_P76'}
class Binary2RegisterImmedShiftedTestTester_op1_24To20Is10111_Cmn_Rule_33_A1_P76
    : public Binary2RegisterImmedShiftedTestTesterop1_24To20Is10111 {
 public:
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10111_Cmn_Rule_33_A1_P76()
    : Binary2RegisterImmedShiftedTestTesterop1_24To20Is10111(
      state_.Binary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76_instance_)
  {}
};

// op1(24:20)=1100x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: 'Orr_Rule_114_A1_P230',
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_op1_24To20Is1100xNotRdIsPcAndS_Orr_Rule_114_A1_P230
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is1100xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is1100xNotRdIsPcAndS_Orr_Rule_114_A1_P230()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is1100xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230_instance_)
  {}
};

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00
//    = Unary2RegisterOp {constraints: ,
//     rule: 'Mov_Rule_97_A1_P196',
//     safety: ['NotRdIsPcAndS']}
class Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is00NotRdIsPcAndS_Mov_Rule_97_A1_P196
    : public Unary2RegisterOpTesterop1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is00NotRdIsPcAndS {
 public:
  Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is00NotRdIsPcAndS_Mov_Rule_97_A1_P196()
    : Unary2RegisterOpTesterop1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is00NotRdIsPcAndS(
      state_.Unary2RegisterOp_Mov_Rule_97_A1_P196_instance_)
  {}
};

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     rule: 'Lsl_Rule_88_A1_P178',
//     safety: ['NeitherImm5NotZeroNorNotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS_Lsl_Rule_88_A1_P178
    : public Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS_Lsl_Rule_88_A1_P178()
    : Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS(
      state_.Unary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178_instance_)
  {}
};

// op1(24:20)=1101x & op3(6:5)=01
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     rule: 'Lsr_Rule_90_A1_P182',
//     safety: ['NotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is01NotRdIsPcAndS_Lsr_Rule_90_A1_P182
    : public Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op3_6To5Is01NotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is01NotRdIsPcAndS_Lsr_Rule_90_A1_P182()
    : Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op3_6To5Is01NotRdIsPcAndS(
      state_.Unary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182_instance_)
  {}
};

// op1(24:20)=1101x & op3(6:5)=10
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     rule: 'Asr_Rule_14_A1_P40',
//     safety: ['NotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is10NotRdIsPcAndS_Asr_Rule_14_A1_P40
    : public Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op3_6To5Is10NotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is10NotRdIsPcAndS_Asr_Rule_14_A1_P40()
    : Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op3_6To5Is10NotRdIsPcAndS(
      state_.Unary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40_instance_)
  {}
};

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11
//    = Unary2RegisterOp {constraints: ,
//     rule: 'Rrx_Rule_141_A1_P282',
//     safety: ['NotRdIsPcAndS']}
class Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is11NotRdIsPcAndS_Rrx_Rule_141_A1_P282
    : public Unary2RegisterOpTesterop1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is11NotRdIsPcAndS {
 public:
  Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is11NotRdIsPcAndS_Rrx_Rule_141_A1_P282()
    : Unary2RegisterOpTesterop1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is11NotRdIsPcAndS(
      state_.Unary2RegisterOp_Rrx_Rule_141_A1_P282_instance_)
  {}
};

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     rule: 'Ror_Rule_139_A1_P278',
//     safety: ['NeitherImm5NotZeroNorNotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS_Ror_Rule_139_A1_P278
    : public Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS_Ror_Rule_139_A1_P278()
    : Unary2RegisterImmedShiftedOpTesterop1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS(
      state_.Unary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278_instance_)
  {}
};

// op1(24:20)=1110x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: 'Bic_Rule_20_A1_P52',
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_op1_24To20Is1110xNotRdIsPcAndS_Bic_Rule_20_A1_P52
    : public Binary3RegisterImmedShiftedOpTesterop1_24To20Is1110xNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is1110xNotRdIsPcAndS_Bic_Rule_20_A1_P52()
    : Binary3RegisterImmedShiftedOpTesterop1_24To20Is1110xNotRdIsPcAndS(
      state_.Binary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52_instance_)
  {}
};

// op1(24:20)=1111x
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     rule: 'Mvn_Rule_107_A1_P216',
//     safety: ['NotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTester_op1_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_107_A1_P216
    : public Unary2RegisterImmedShiftedOpTesterop1_24To20Is1111xNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_107_A1_P216()
    : Unary2RegisterImmedShiftedOpTesterop1_24To20Is1111xNotRdIsPcAndS(
      state_.Unary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216_instance_)
  {}
};

// op1(24:20)=0000x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: 'And_Rule_13_A1_P38',
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_op1_24To20Is0000xRegsNotPc_And_Rule_13_A1_P38
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0000xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0000xRegsNotPc_And_Rule_13_A1_P38()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0000xRegsNotPc(
      state_.Binary4RegisterShiftedOp_And_Rule_13_A1_P38_instance_)
  {}
};

// op1(24:20)=0001x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: 'Eor_Rule_46_A1_P98',
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_op1_24To20Is0001xRegsNotPc_Eor_Rule_46_A1_P98
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0001xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0001xRegsNotPc_Eor_Rule_46_A1_P98()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0001xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Eor_Rule_46_A1_P98_instance_)
  {}
};

// op1(24:20)=0010x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: 'Sub_Rule_214_A1_P424',
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_op1_24To20Is0010xRegsNotPc_Sub_Rule_214_A1_P424
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0010xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0010xRegsNotPc_Sub_Rule_214_A1_P424()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0010xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Sub_Rule_214_A1_P424_instance_)
  {}
};

// op1(24:20)=0011x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: 'Rsb_Rule_144_A1_P288',
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_op1_24To20Is0011xRegsNotPc_Rsb_Rule_144_A1_P288
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0011xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0011xRegsNotPc_Rsb_Rule_144_A1_P288()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0011xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Rsb_Rule_144_A1_P288_instance_)
  {}
};

// op1(24:20)=0100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: 'Add_Rule_7_A1_P26',
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_op1_24To20Is0100xRegsNotPc_Add_Rule_7_A1_P26
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0100xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0100xRegsNotPc_Add_Rule_7_A1_P26()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0100xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Add_Rule_7_A1_P26_instance_)
  {}
};

// op1(24:20)=0101x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: 'Adc_Rule_3_A1_P18',
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_op1_24To20Is0101xRegsNotPc_Adc_Rule_3_A1_P18
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0101xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0101xRegsNotPc_Adc_Rule_3_A1_P18()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0101xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Adc_Rule_3_A1_P18_instance_)
  {}
};

// op1(24:20)=0110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: 'Sbc_Rule_153_A1_P306',
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_op1_24To20Is0110xRegsNotPc_Sbc_Rule_153_A1_P306
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0110xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0110xRegsNotPc_Sbc_Rule_153_A1_P306()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0110xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Sbc_Rule_153_A1_P306_instance_)
  {}
};

// op1(24:20)=0111x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: 'Rsc_Rule_147_A1_P294',
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_op1_24To20Is0111xRegsNotPc_Rsc_Rule_147_A1_P294
    : public Binary4RegisterShiftedOpTesterop1_24To20Is0111xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is0111xRegsNotPc_Rsc_Rule_147_A1_P294()
    : Binary4RegisterShiftedOpTesterop1_24To20Is0111xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Rsc_Rule_147_A1_P294_instance_)
  {}
};

// op1(24:20)=10001
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: 'Tst_Rule_232_A1_P458',
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTester_op1_24To20Is10001RegsNotPc_Tst_Rule_232_A1_P458
    : public Binary3RegisterShiftedTestTesterop1_24To20Is10001RegsNotPc {
 public:
  Binary3RegisterShiftedTestTester_op1_24To20Is10001RegsNotPc_Tst_Rule_232_A1_P458()
    : Binary3RegisterShiftedTestTesterop1_24To20Is10001RegsNotPc(
      state_.Binary3RegisterShiftedTest_Tst_Rule_232_A1_P458_instance_)
  {}
};

// op1(24:20)=10011
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: 'Teq_Rule_229_A1_P452',
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTester_op1_24To20Is10011RegsNotPc_Teq_Rule_229_A1_P452
    : public Binary3RegisterShiftedTestTesterop1_24To20Is10011RegsNotPc {
 public:
  Binary3RegisterShiftedTestTester_op1_24To20Is10011RegsNotPc_Teq_Rule_229_A1_P452()
    : Binary3RegisterShiftedTestTesterop1_24To20Is10011RegsNotPc(
      state_.Binary3RegisterShiftedTest_Teq_Rule_229_A1_P452_instance_)
  {}
};

// op1(24:20)=10101
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: 'Cmp_Rule_37_A1_P84',
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTester_op1_24To20Is10101RegsNotPc_Cmp_Rule_37_A1_P84
    : public Binary3RegisterShiftedTestTesterop1_24To20Is10101RegsNotPc {
 public:
  Binary3RegisterShiftedTestTester_op1_24To20Is10101RegsNotPc_Cmp_Rule_37_A1_P84()
    : Binary3RegisterShiftedTestTesterop1_24To20Is10101RegsNotPc(
      state_.Binary3RegisterShiftedTest_Cmp_Rule_37_A1_P84_instance_)
  {}
};

// op1(24:20)=10111
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: 'Cmn_Rule_34_A1_P78',
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTester_op1_24To20Is10111RegsNotPc_Cmn_Rule_34_A1_P78
    : public Binary3RegisterShiftedTestTesterop1_24To20Is10111RegsNotPc {
 public:
  Binary3RegisterShiftedTestTester_op1_24To20Is10111RegsNotPc_Cmn_Rule_34_A1_P78()
    : Binary3RegisterShiftedTestTesterop1_24To20Is10111RegsNotPc(
      state_.Binary3RegisterShiftedTest_Cmn_Rule_34_A1_P78_instance_)
  {}
};

// op1(24:20)=1100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: 'Orr_Rule_115_A1_P212',
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_op1_24To20Is1100xRegsNotPc_Orr_Rule_115_A1_P212
    : public Binary4RegisterShiftedOpTesterop1_24To20Is1100xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is1100xRegsNotPc_Orr_Rule_115_A1_P212()
    : Binary4RegisterShiftedOpTesterop1_24To20Is1100xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Orr_Rule_115_A1_P212_instance_)
  {}
};

// op1(24:20)=1101x & op2(6:5)=00
//    = Binary3RegisterOp {constraints: ,
//     rule: 'Lsl_Rule_89_A1_P180',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is00RegsNotPc_Lsl_Rule_89_A1_P180
    : public Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is00RegsNotPc {
 public:
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is00RegsNotPc_Lsl_Rule_89_A1_P180()
    : Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is00RegsNotPc(
      state_.Binary3RegisterOp_Lsl_Rule_89_A1_P180_instance_)
  {}
};

// op1(24:20)=1101x & op2(6:5)=01
//    = Binary3RegisterOp {constraints: ,
//     rule: 'Lsr_Rule_91_A1_P184',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is01RegsNotPc_Lsr_Rule_91_A1_P184
    : public Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is01RegsNotPc {
 public:
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is01RegsNotPc_Lsr_Rule_91_A1_P184()
    : Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is01RegsNotPc(
      state_.Binary3RegisterOp_Lsr_Rule_91_A1_P184_instance_)
  {}
};

// op1(24:20)=1101x & op2(6:5)=10
//    = Binary3RegisterOp {constraints: ,
//     rule: 'Asr_Rule_15_A1_P42',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is10RegsNotPc_Asr_Rule_15_A1_P42
    : public Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is10RegsNotPc {
 public:
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is10RegsNotPc_Asr_Rule_15_A1_P42()
    : Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is10RegsNotPc(
      state_.Binary3RegisterOp_Asr_Rule_15_A1_P42_instance_)
  {}
};

// op1(24:20)=1101x & op2(6:5)=11
//    = Binary3RegisterOp {constraints: ,
//     rule: 'Ror_Rule_140_A1_P280',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is11RegsNotPc_Ror_Rule_140_A1_P280
    : public Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is11RegsNotPc {
 public:
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is11RegsNotPc_Ror_Rule_140_A1_P280()
    : Binary3RegisterOpTesterop1_24To20Is1101x_op2_6To5Is11RegsNotPc(
      state_.Binary3RegisterOp_Ror_Rule_140_A1_P280_instance_)
  {}
};

// op1(24:20)=1110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: 'Bic_Rule_21_A1_P54',
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_op1_24To20Is1110xRegsNotPc_Bic_Rule_21_A1_P54
    : public Binary4RegisterShiftedOpTesterop1_24To20Is1110xRegsNotPc {
 public:
  Binary4RegisterShiftedOpTester_op1_24To20Is1110xRegsNotPc_Bic_Rule_21_A1_P54()
    : Binary4RegisterShiftedOpTesterop1_24To20Is1110xRegsNotPc(
      state_.Binary4RegisterShiftedOp_Bic_Rule_21_A1_P54_instance_)
  {}
};

// op1(24:20)=1111x
//    = Unary3RegisterShiftedOp {constraints: ,
//     rule: 'Mvn_Rule_108_A1_P218',
//     safety: ['RegsNotPc']}
class Unary3RegisterShiftedOpTester_op1_24To20Is1111xRegsNotPc_Mvn_Rule_108_A1_P218
    : public Unary3RegisterShiftedOpTesterop1_24To20Is1111xRegsNotPc {
 public:
  Unary3RegisterShiftedOpTester_op1_24To20Is1111xRegsNotPc_Mvn_Rule_108_A1_P218()
    : Unary3RegisterShiftedOpTesterop1_24To20Is1111xRegsNotPc(
      state_.Unary3RegisterShiftedOp_Mvn_Rule_108_A1_P218_instance_)
  {}
};

// opcode(24:20)=01x00
//    = StoreVectorRegisterList {constraints: ,
//     rule: 'Vstm_Rule_399_A1_A2_P784'}
class StoreVectorRegisterListTester_opcode_24To20Is01x00_Vstm_Rule_399_A1_A2_P784
    : public StoreVectorRegisterListTesteropcode_24To20Is01x00 {
 public:
  StoreVectorRegisterListTester_opcode_24To20Is01x00_Vstm_Rule_399_A1_A2_P784()
    : StoreVectorRegisterListTesteropcode_24To20Is01x00(
      state_.StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_)
  {}
};

// opcode(24:20)=01x10
//    = StoreVectorRegisterList {constraints: ,
//     rule: 'Vstm_Rule_399_A1_A2_P784'}
class StoreVectorRegisterListTester_opcode_24To20Is01x10_Vstm_Rule_399_A1_A2_P784
    : public StoreVectorRegisterListTesteropcode_24To20Is01x10 {
 public:
  StoreVectorRegisterListTester_opcode_24To20Is01x10_Vstm_Rule_399_A1_A2_P784()
    : StoreVectorRegisterListTesteropcode_24To20Is01x10(
      state_.StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_)
  {}
};

// opcode(24:20)=1xx00
//    = StoreVectorRegister {constraints: ,
//     rule: 'Vstr_Rule_400_A1_A2_P786'}
class StoreVectorRegisterTester_opcode_24To20Is1xx00_Vstr_Rule_400_A1_A2_P786
    : public StoreVectorRegisterTesteropcode_24To20Is1xx00 {
 public:
  StoreVectorRegisterTester_opcode_24To20Is1xx00_Vstr_Rule_400_A1_A2_P786()
    : StoreVectorRegisterTesteropcode_24To20Is1xx00(
      state_.StoreVectorRegister_Vstr_Rule_400_A1_A2_P786_instance_)
  {}
};

// opcode(24:20)=10x10 & Rn(19:16)=~1101
//    = StoreVectorRegisterList {constraints: ,
//     rule: 'Vstm_Rule_399_A1_A2_P784',
//     safety: ['NotRnIsSp']}
class StoreVectorRegisterListTester_opcode_24To20Is10x10_Rn_19To16IsNot1101NotRnIsSp_Vstm_Rule_399_A1_A2_P784
    : public StoreVectorRegisterListTesteropcode_24To20Is10x10_Rn_19To16IsNot1101NotRnIsSp {
 public:
  StoreVectorRegisterListTester_opcode_24To20Is10x10_Rn_19To16IsNot1101NotRnIsSp_Vstm_Rule_399_A1_A2_P784()
    : StoreVectorRegisterListTesteropcode_24To20Is10x10_Rn_19To16IsNot1101NotRnIsSp(
      state_.StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_)
  {}
};

// opcode(24:20)=10x10 & Rn(19:16)=1101
//    = StoreVectorRegisterList {constraints: ,
//     rule: 'Vpush_355_A1_A2_P696'}
class StoreVectorRegisterListTester_opcode_24To20Is10x10_Rn_19To16Is1101_Vpush_355_A1_A2_P696
    : public StoreVectorRegisterListTesteropcode_24To20Is10x10_Rn_19To16Is1101 {
 public:
  StoreVectorRegisterListTester_opcode_24To20Is10x10_Rn_19To16Is1101_Vpush_355_A1_A2_P696()
    : StoreVectorRegisterListTesteropcode_24To20Is10x10_Rn_19To16Is1101(
      state_.StoreVectorRegisterList_Vpush_355_A1_A2_P696_instance_)
  {}
};

// opcode(24:20)=01x01
//    = LoadVectorRegisterList {constraints: ,
//     rule: 'Vldm_Rule_319_A1_A2_P626'}
class LoadVectorRegisterListTester_opcode_24To20Is01x01_Vldm_Rule_319_A1_A2_P626
    : public LoadStoreVectorRegisterListTesteropcode_24To20Is01x01 {
 public:
  LoadVectorRegisterListTester_opcode_24To20Is01x01_Vldm_Rule_319_A1_A2_P626()
    : LoadStoreVectorRegisterListTesteropcode_24To20Is01x01(
      state_.LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626_instance_)
  {}
};

// opcode(24:20)=01x11 & Rn(19:16)=~1101
//    = LoadVectorRegisterList {constraints: ,
//     rule: 'Vldm_Rule_319_A1_A2_P626',
//     safety: ['NotRnIsSp']}
class LoadVectorRegisterListTester_opcode_24To20Is01x11_Rn_19To16IsNot1101NotRnIsSp_Vldm_Rule_319_A1_A2_P626
    : public LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_Rn_19To16IsNot1101NotRnIsSp {
 public:
  LoadVectorRegisterListTester_opcode_24To20Is01x11_Rn_19To16IsNot1101NotRnIsSp_Vldm_Rule_319_A1_A2_P626()
    : LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_Rn_19To16IsNot1101NotRnIsSp(
      state_.LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626_instance_)
  {}
};

// opcode(24:20)=01x11 & Rn(19:16)=1101
//    = LoadVectorRegisterList {constraints: ,
//     rule: 'Vpop_Rule_354_A1_A2_P694'}
class LoadVectorRegisterListTester_opcode_24To20Is01x11_Rn_19To16Is1101_Vpop_Rule_354_A1_A2_P694
    : public LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_Rn_19To16Is1101 {
 public:
  LoadVectorRegisterListTester_opcode_24To20Is01x11_Rn_19To16Is1101_Vpop_Rule_354_A1_A2_P694()
    : LoadStoreVectorRegisterListTesteropcode_24To20Is01x11_Rn_19To16Is1101(
      state_.LoadVectorRegisterList_Vpop_Rule_354_A1_A2_P694_instance_)
  {}
};

// opcode(24:20)=1xx01
//    = LoadVectorRegister {constraints: ,
//     rule: 'Vldr_Rule_320_A1_A2_P628'}
class LoadVectorRegisterTester_opcode_24To20Is1xx01_Vldr_Rule_320_A1_A2_P628
    : public LoadStoreVectorOpTesteropcode_24To20Is1xx01 {
 public:
  LoadVectorRegisterTester_opcode_24To20Is1xx01_Vldr_Rule_320_A1_A2_P628()
    : LoadStoreVectorOpTesteropcode_24To20Is1xx01(
      state_.LoadVectorRegister_Vldr_Rule_320_A1_A2_P628_instance_)
  {}
};

// opcode(24:20)=10x11
//    = LoadVectorRegisterList {constraints: ,
//     rule: 'Vldm_Rule_318_A1_A2_P626'}
class LoadVectorRegisterListTester_opcode_24To20Is10x11_Vldm_Rule_318_A1_A2_P626
    : public LoadStoreVectorRegisterListTesteropcode_24To20Is10x11 {
 public:
  LoadVectorRegisterListTester_opcode_24To20Is10x11_Vldm_Rule_318_A1_A2_P626()
    : LoadStoreVectorRegisterListTesteropcode_24To20Is10x11(
      state_.LoadVectorRegisterList_Vldm_Rule_318_A1_A2_P626_instance_)
  {}
};

// op2(6:5)=01 & op1(24:20)=xx0x0
//    = Store3RegisterOp {constraints: ,
//     rule: 'Strh_Rule_208_A1_P412'}
class Store3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x0_Strh_Rule_208_A1_P412
    : public LoadStore3RegisterOpTesterop2_6To5Is01_op1_24To20Isxx0x0 {
 public:
  Store3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x0_Strh_Rule_208_A1_P412()
    : LoadStore3RegisterOpTesterop2_6To5Is01_op1_24To20Isxx0x0(
      state_.Store3RegisterOp_Strh_Rule_208_A1_P412_instance_)
  {}
};

// op2(6:5)=01 & op1(24:20)=xx0x1
//    = Load3RegisterOp {constraints: ,
//     rule: 'Ldrh_Rule_76_A1_P156'}
class Load3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x1_Ldrh_Rule_76_A1_P156
    : public LoadStore3RegisterOpTesterop2_6To5Is01_op1_24To20Isxx0x1 {
 public:
  Load3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x1_Ldrh_Rule_76_A1_P156()
    : LoadStore3RegisterOpTesterop2_6To5Is01_op1_24To20Isxx0x1(
      state_.Load3RegisterOp_Ldrh_Rule_76_A1_P156_instance_)
  {}
};

// op2(6:5)=01 & op1(24:20)=xx1x0
//    = Store2RegisterImm8Op {constraints: ,
//     rule: 'Strh_Rule_207_A1_P410'}
class Store2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x0_Strh_Rule_207_A1_P410
    : public LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x0 {
 public:
  Store2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x0_Strh_Rule_207_A1_P410()
    : LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x0(
      state_.Store2RegisterImm8Op_Strh_Rule_207_A1_P410_instance_)
  {}
};

// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op {constraints: ,
//     rule: 'Ldrh_Rule_74_A1_P152'}
class Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16IsNot1111_Ldrh_Rule_74_A1_P152
    : public LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16IsNot1111 {
 public:
  Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16IsNot1111_Ldrh_Rule_74_A1_P152()
    : LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16IsNot1111(
      state_.Load2RegisterImm8Op_Ldrh_Rule_74_A1_P152_instance_)
  {}
};

// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op {constraints: ,
//     rule: 'Ldrh_Rule_75_A1_P154'}
class Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrh_Rule_75_A1_P154
    : public LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16Is1111 {
 public:
  Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrh_Rule_75_A1_P154()
    : LoadStore2RegisterImm8OpTesterop2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16Is1111(
      state_.Load2RegisterImm8Op_Ldrh_Rule_75_A1_P154_instance_)
  {}
};

// op2(6:5)=10 & op1(24:20)=xx0x0
//    = Load3RegisterDoubleOp {constraints: ,
//     rule: 'Ldrd_Rule_68_A1_P140'}
class Load3RegisterDoubleOpTester_op2_6To5Is10_op1_24To20Isxx0x0_Ldrd_Rule_68_A1_P140
    : public LoadStore3RegisterDoubleOpTesterop2_6To5Is10_op1_24To20Isxx0x0 {
 public:
  Load3RegisterDoubleOpTester_op2_6To5Is10_op1_24To20Isxx0x0_Ldrd_Rule_68_A1_P140()
    : LoadStore3RegisterDoubleOpTesterop2_6To5Is10_op1_24To20Isxx0x0(
      state_.Load3RegisterDoubleOp_Ldrd_Rule_68_A1_P140_instance_)
  {}
};

// op2(6:5)=10 & op1(24:20)=xx0x1
//    = Load3RegisterOp {constraints: ,
//     rule: 'Ldrsb_Rule_80_A1_P164'}
class Load3RegisterOpTester_op2_6To5Is10_op1_24To20Isxx0x1_Ldrsb_Rule_80_A1_P164
    : public LoadStore3RegisterOpTesterop2_6To5Is10_op1_24To20Isxx0x1 {
 public:
  Load3RegisterOpTester_op2_6To5Is10_op1_24To20Isxx0x1_Ldrsb_Rule_80_A1_P164()
    : LoadStore3RegisterOpTesterop2_6To5Is10_op1_24To20Isxx0x1(
      state_.Load3RegisterOp_Ldrsb_Rule_80_A1_P164_instance_)
  {}
};

// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=~1111
//    = Load2RegisterImm8DoubleOp {constraints: ,
//     rule: 'Ldrd_Rule_66_A1_P136'}
class Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16IsNot1111_Ldrd_Rule_66_A1_P136
    : public LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16IsNot1111 {
 public:
  Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16IsNot1111_Ldrd_Rule_66_A1_P136()
    : LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16IsNot1111(
      state_.Load2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136_instance_)
  {}
};

// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=1111
//    = Load2RegisterImm8DoubleOp {constraints: ,
//     rule: 'Ldrd_Rule_67_A1_P138'}
class Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16Is1111_Ldrd_Rule_67_A1_P138
    : public LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16Is1111 {
 public:
  Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16Is1111_Ldrd_Rule_67_A1_P138()
    : LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16Is1111(
      state_.Load2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138_instance_)
  {}
};

// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op {constraints: ,
//     rule: 'Ldrsb_Rule_78_A1_P160'}
class Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16IsNot1111_Ldrsb_Rule_78_A1_P160
    : public LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16IsNot1111 {
 public:
  Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16IsNot1111_Ldrsb_Rule_78_A1_P160()
    : LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16IsNot1111(
      state_.Load2RegisterImm8Op_Ldrsb_Rule_78_A1_P160_instance_)
  {}
};

// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op {constraints: ,
//     rule: 'ldrsb_Rule_79_A1_162'}
class Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16Is1111_ldrsb_Rule_79_A1_162
    : public LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16Is1111 {
 public:
  Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16Is1111_ldrsb_Rule_79_A1_162()
    : LoadStore2RegisterImm8OpTesterop2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16Is1111(
      state_.Load2RegisterImm8Op_ldrsb_Rule_79_A1_162_instance_)
  {}
};

// op2(6:5)=11 & op1(24:20)=xx0x0
//    = Store3RegisterDoubleOp {constraints: ,
//     rule: 'Strd_Rule_201_A1_P398'}
class Store3RegisterDoubleOpTester_op2_6To5Is11_op1_24To20Isxx0x0_Strd_Rule_201_A1_P398
    : public LoadStore3RegisterDoubleOpTesterop2_6To5Is11_op1_24To20Isxx0x0 {
 public:
  Store3RegisterDoubleOpTester_op2_6To5Is11_op1_24To20Isxx0x0_Strd_Rule_201_A1_P398()
    : LoadStore3RegisterDoubleOpTesterop2_6To5Is11_op1_24To20Isxx0x0(
      state_.Store3RegisterDoubleOp_Strd_Rule_201_A1_P398_instance_)
  {}
};

// op2(6:5)=11 & op1(24:20)=xx0x1
//    = Load3RegisterOp {constraints: ,
//     rule: 'Ldrsh_Rule_84_A1_P172'}
class Load3RegisterOpTester_op2_6To5Is11_op1_24To20Isxx0x1_Ldrsh_Rule_84_A1_P172
    : public LoadStore3RegisterOpTesterop2_6To5Is11_op1_24To20Isxx0x1 {
 public:
  Load3RegisterOpTester_op2_6To5Is11_op1_24To20Isxx0x1_Ldrsh_Rule_84_A1_P172()
    : LoadStore3RegisterOpTesterop2_6To5Is11_op1_24To20Isxx0x1(
      state_.Load3RegisterOp_Ldrsh_Rule_84_A1_P172_instance_)
  {}
};

// op2(6:5)=11 & op1(24:20)=xx1x0
//    = Store2RegisterImm8DoubleOp {constraints: ,
//     rule: 'Strd_Rule_200_A1_P396'}
class Store2RegisterImm8DoubleOpTester_op2_6To5Is11_op1_24To20Isxx1x0_Strd_Rule_200_A1_P396
    : public LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is11_op1_24To20Isxx1x0 {
 public:
  Store2RegisterImm8DoubleOpTester_op2_6To5Is11_op1_24To20Isxx1x0_Strd_Rule_200_A1_P396()
    : LoadStore2RegisterImm8DoubleOpTesterop2_6To5Is11_op1_24To20Isxx1x0(
      state_.Store2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396_instance_)
  {}
};

// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op {constraints: ,
//     rule: 'Ldrsh_Rule_82_A1_P168'}
class Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16IsNot1111_Ldrsh_Rule_82_A1_P168
    : public LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16IsNot1111 {
 public:
  Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16IsNot1111_Ldrsh_Rule_82_A1_P168()
    : LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16IsNot1111(
      state_.Load2RegisterImm8Op_Ldrsh_Rule_82_A1_P168_instance_)
  {}
};

// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op {constraints: ,
//     rule: 'Ldrsh_Rule_83_A1_P170'}
class Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrsh_Rule_83_A1_P170
    : public LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16Is1111 {
 public:
  Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrsh_Rule_83_A1_P170()
    : LoadStore2RegisterImm8OpTesterop2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16Is1111(
      state_.Load2RegisterImm8Op_Ldrsh_Rule_83_A1_P170_instance_)
  {}
};

// opc1(23:20)=0x00
//    = CondVfpOp {constraints: ,
//     rule: 'Vmla_vmls_Rule_423_A2_P636'}
class CondVfpOpTester_opc1_23To20Is0x00_Vmla_vmls_Rule_423_A2_P636
    : public CondVfpOpTesteropc1_23To20Is0x00 {
 public:
  CondVfpOpTester_opc1_23To20Is0x00_Vmla_vmls_Rule_423_A2_P636()
    : CondVfpOpTesteropc1_23To20Is0x00(
      state_.CondVfpOp_Vmla_vmls_Rule_423_A2_P636_instance_)
  {}
};

// opc1(23:20)=0x01
//    = CondVfpOp {constraints: ,
//     rule: 'Vnmla_vnmls_Rule_343_A1_P674'}
class CondVfpOpTester_opc1_23To20Is0x01_Vnmla_vnmls_Rule_343_A1_P674
    : public CondVfpOpTesteropc1_23To20Is0x01 {
 public:
  CondVfpOpTester_opc1_23To20Is0x01_Vnmla_vnmls_Rule_343_A1_P674()
    : CondVfpOpTesteropc1_23To20Is0x01(
      state_.CondVfpOp_Vnmla_vnmls_Rule_343_A1_P674_instance_)
  {}
};

// opc1(23:20)=0x10 & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: 'Vnmul_Rule_343_A2_P674'}
class CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx1_Vnmul_Rule_343_A2_P674
    : public CondVfpOpTesteropc1_23To20Is0x10_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx1_Vnmul_Rule_343_A2_P674()
    : CondVfpOpTesteropc1_23To20Is0x10_opc3_7To6Isx1(
      state_.CondVfpOp_Vnmul_Rule_343_A2_P674_instance_)
  {}
};

// opc1(23:20)=0x10 & opc3(7:6)=x0
//    = CondVfpOp {constraints: ,
//     rule: 'Vmul_Rule_338_A2_P664'}
class CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx0_Vmul_Rule_338_A2_P664
    : public CondVfpOpTesteropc1_23To20Is0x10_opc3_7To6Isx0 {
 public:
  CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx0_Vmul_Rule_338_A2_P664()
    : CondVfpOpTesteropc1_23To20Is0x10_opc3_7To6Isx0(
      state_.CondVfpOp_Vmul_Rule_338_A2_P664_instance_)
  {}
};

// opc1(23:20)=0x11 & opc3(7:6)=x0
//    = CondVfpOp {constraints: ,
//     rule: 'Vadd_Rule_271_A2_P536'}
class CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx0_Vadd_Rule_271_A2_P536
    : public CondVfpOpTesteropc1_23To20Is0x11_opc3_7To6Isx0 {
 public:
  CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx0_Vadd_Rule_271_A2_P536()
    : CondVfpOpTesteropc1_23To20Is0x11_opc3_7To6Isx0(
      state_.CondVfpOp_Vadd_Rule_271_A2_P536_instance_)
  {}
};

// opc1(23:20)=0x11 & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: 'Vsub_Rule_402_A2_P790'}
class CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx1_Vsub_Rule_402_A2_P790
    : public CondVfpOpTesteropc1_23To20Is0x11_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx1_Vsub_Rule_402_A2_P790()
    : CondVfpOpTesteropc1_23To20Is0x11_opc3_7To6Isx1(
      state_.CondVfpOp_Vsub_Rule_402_A2_P790_instance_)
  {}
};

// opc1(23:20)=1x00 & opc3(7:6)=x0
//    = CondVfpOp {constraints: ,
//     rule: 'Vdiv_Rule_301_A1_P590'}
class CondVfpOpTester_opc1_23To20Is1x00_opc3_7To6Isx0_Vdiv_Rule_301_A1_P590
    : public CondVfpOpTesteropc1_23To20Is1x00_opc3_7To6Isx0 {
 public:
  CondVfpOpTester_opc1_23To20Is1x00_opc3_7To6Isx0_Vdiv_Rule_301_A1_P590()
    : CondVfpOpTesteropc1_23To20Is1x00_opc3_7To6Isx0(
      state_.CondVfpOp_Vdiv_Rule_301_A1_P590_instance_)
  {}
};

// opc1(23:20)=1x01
//    = CondVfpOp {constraints: ,
//     rule: 'Vfnma_vfnms_Rule_A1'}
class CondVfpOpTester_opc1_23To20Is1x01_Vfnma_vfnms_Rule_A1
    : public CondVfpOpTesteropc1_23To20Is1x01 {
 public:
  CondVfpOpTester_opc1_23To20Is1x01_Vfnma_vfnms_Rule_A1()
    : CondVfpOpTesteropc1_23To20Is1x01(
      state_.CondVfpOp_Vfnma_vfnms_Rule_A1_instance_)
  {}
};

// opc1(23:20)=1x10
//    = CondVfpOp {constraints: ,
//     rule: 'Vfma_vfms_Rule_A1'}
class CondVfpOpTester_opc1_23To20Is1x10_Vfma_vfms_Rule_A1
    : public CondVfpOpTesteropc1_23To20Is1x10 {
 public:
  CondVfpOpTester_opc1_23To20Is1x10_Vfma_vfms_Rule_A1()
    : CondVfpOpTesteropc1_23To20Is1x10(
      state_.CondVfpOp_Vfma_vfms_Rule_A1_instance_)
  {}
};

// op1(22:21)=00
//    = Binary4RegisterDualOp {constraints: ,
//     rule: 'Smlaxx_Rule_166_A1_P330',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_op1_22To21Is00RegsNotPc_Smlaxx_Rule_166_A1_P330
    : public Binary4RegisterDualOpTesterop1_22To21Is00RegsNotPc {
 public:
  Binary4RegisterDualOpTester_op1_22To21Is00RegsNotPc_Smlaxx_Rule_166_A1_P330()
    : Binary4RegisterDualOpTesterop1_22To21Is00RegsNotPc(
      state_.Binary4RegisterDualOp_Smlaxx_Rule_166_A1_P330_instance_)
  {}
};

// op1(22:21)=01 & op(5)=0
//    = Binary4RegisterDualOp {constraints: ,
//     rule: 'Smlawx_Rule_171_A1_340',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_op1_22To21Is01_op_5Is0RegsNotPc_Smlawx_Rule_171_A1_340
    : public Binary4RegisterDualOpTesterop1_22To21Is01_op_5Is0RegsNotPc {
 public:
  Binary4RegisterDualOpTester_op1_22To21Is01_op_5Is0RegsNotPc_Smlawx_Rule_171_A1_340()
    : Binary4RegisterDualOpTesterop1_22To21Is01_op_5Is0RegsNotPc(
      state_.Binary4RegisterDualOp_Smlawx_Rule_171_A1_340_instance_)
  {}
};

// op1(22:21)=01 & op(5)=1
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: 'Smulwx_Rule_180_A1_P358',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_op1_22To21Is01_op_5Is1RegsNotPc_Smulwx_Rule_180_A1_P358
    : public Binary3RegisterOpAltATesterop1_22To21Is01_op_5Is1RegsNotPc {
 public:
  Binary3RegisterOpAltATester_op1_22To21Is01_op_5Is1RegsNotPc_Smulwx_Rule_180_A1_P358()
    : Binary3RegisterOpAltATesterop1_22To21Is01_op_5Is1RegsNotPc(
      state_.Binary3RegisterOpAltA_Smulwx_Rule_180_A1_P358_instance_)
  {}
};

// op1(22:21)=10
//    = Binary4RegisterDualResult {constraints: ,
//     rule: 'Smlalxx_Rule_169_A1_P336',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_op1_22To21Is10RegsNotPc_Smlalxx_Rule_169_A1_P336
    : public Binary4RegisterDualResultTesterop1_22To21Is10RegsNotPc {
 public:
  Binary4RegisterDualResultTester_op1_22To21Is10RegsNotPc_Smlalxx_Rule_169_A1_P336()
    : Binary4RegisterDualResultTesterop1_22To21Is10RegsNotPc(
      state_.Binary4RegisterDualResult_Smlalxx_Rule_169_A1_P336_instance_)
  {}
};

// op1(22:21)=11
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: 'Smulxx_Rule_178_P354',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_op1_22To21Is11RegsNotPc_Smulxx_Rule_178_P354
    : public Binary3RegisterOpAltATesterop1_22To21Is11RegsNotPc {
 public:
  Binary3RegisterOpAltATester_op1_22To21Is11RegsNotPc_Smulxx_Rule_178_P354()
    : Binary3RegisterOpAltATesterop1_22To21Is11RegsNotPc(
      state_.Binary3RegisterOpAltA_Smulxx_Rule_178_P354_instance_)
  {}
};

// A(25)=1 & op1(24:20)=xx0x0 & B(4)=0 & op1_repeated(24:20)=~0x010
//    = Store3RegisterImm5Op {constraints: ,
//     rule: 'Str_Rule_195_A1_P386'}
class Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x0_B_4Is0_op1_repeated_24To20IsNot0x010_Str_Rule_195_A1_P386
    : public LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x0_B_4Is0_op1_repeated_24To20IsNot0x010 {
 public:
  Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x0_B_4Is0_op1_repeated_24To20IsNot0x010_Str_Rule_195_A1_P386()
    : LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x0_B_4Is0_op1_repeated_24To20IsNot0x010(
      state_.Store3RegisterImm5Op_Str_Rule_195_A1_P386_instance_)
  {}
};

// A(25)=0 & op1(24:20)=0x010
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Strt_Rule_A1'}
class ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x010_Strt_Rule_A1
    : public UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x010 {
 public:
  ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x010_Strt_Rule_A1()
    : UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x010(
      state_.ForbiddenCondDecoder_Strt_Rule_A1_instance_)
  {}
};

// A(25)=1 & op1(24:20)=0x010 & B(4)=0
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Strt_Rule_A2'}
class ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x010_B_4Is0_Strt_Rule_A2
    : public UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x010_B_4Is0 {
 public:
  ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x010_B_4Is0_Strt_Rule_A2()
    : UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x010_B_4Is0(
      state_.ForbiddenCondDecoder_Strt_Rule_A2_instance_)
  {}
};

// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x011
//    = Load2RegisterImm12Op {constraints: ,
//     rule: 'Ldr_Rule_58_A1_P120',
//     safety: ['NotRnIsPc']}
class Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x011NotRnIsPc_Ldr_Rule_58_A1_P120
    : public LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x011NotRnIsPc {
 public:
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x011NotRnIsPc_Ldr_Rule_58_A1_P120()
    : LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x011NotRnIsPc(
      state_.Load2RegisterImm12Op_Ldr_Rule_58_A1_P120_instance_)
  {}
};

// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x011
//    = Load2RegisterImm12Op {constraints: ,
//     rule: 'Ldr_Rule_59_A1_P122'}
class Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x011_Ldr_Rule_59_A1_P122
    : public LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x011 {
 public:
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x011_Ldr_Rule_59_A1_P122()
    : LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x011(
      state_.Load2RegisterImm12Op_Ldr_Rule_59_A1_P122_instance_)
  {}
};

// A(25)=1 & op1(24:20)=xx0x1 & B(4)=0 & op1_repeated(24:20)=~0x011
//    = Load3RegisterImm5Op {constraints: ,
//     rule: 'Ldr_Rule_60_A1_P124'}
class Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x1_B_4Is0_op1_repeated_24To20IsNot0x011_Ldr_Rule_60_A1_P124
    : public LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x1_B_4Is0_op1_repeated_24To20IsNot0x011 {
 public:
  Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x1_B_4Is0_op1_repeated_24To20IsNot0x011_Ldr_Rule_60_A1_P124()
    : LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx0x1_B_4Is0_op1_repeated_24To20IsNot0x011(
      state_.Load3RegisterImm5Op_Ldr_Rule_60_A1_P124_instance_)
  {}
};

// A(25)=0 & op1(24:20)=0x011
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Ldrt_Rule_A1'}
class ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x011_Ldrt_Rule_A1
    : public UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x011 {
 public:
  ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x011_Ldrt_Rule_A1()
    : UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x011(
      state_.ForbiddenCondDecoder_Ldrt_Rule_A1_instance_)
  {}
};

// A(25)=1 & op1(24:20)=0x011 & B(4)=0
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Ldrt_Rule_A2'}
class ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x011_B_4Is0_Ldrt_Rule_A2
    : public UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x011_B_4Is0 {
 public:
  ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x011_B_4Is0_Ldrt_Rule_A2()
    : UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x011_B_4Is0(
      state_.ForbiddenCondDecoder_Ldrt_Rule_A2_instance_)
  {}
};

// A(25)=0 & op1(24:20)=xx1x0 & op1_repeated(24:20)=~0x110
//    = Store2RegisterImm12Op {constraints: ,
//     rule: 'Strb_Rule_197_A1_P390'}
class Store2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x0_op1_repeated_24To20IsNot0x110_Strb_Rule_197_A1_P390
    : public LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x0_op1_repeated_24To20IsNot0x110 {
 public:
  Store2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x0_op1_repeated_24To20IsNot0x110_Strb_Rule_197_A1_P390()
    : LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x0_op1_repeated_24To20IsNot0x110(
      state_.Store2RegisterImm12Op_Strb_Rule_197_A1_P390_instance_)
  {}
};

// A(25)=1 & op1(24:20)=xx1x0 & B(4)=0 & op1_repeated(24:20)=~0x110
//    = Store3RegisterImm5Op {constraints: ,
//     rule: 'Strb_Rule_198_A1_P392'}
class Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x0_B_4Is0_op1_repeated_24To20IsNot0x110_Strb_Rule_198_A1_P392
    : public LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x0_B_4Is0_op1_repeated_24To20IsNot0x110 {
 public:
  Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x0_B_4Is0_op1_repeated_24To20IsNot0x110_Strb_Rule_198_A1_P392()
    : LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x0_B_4Is0_op1_repeated_24To20IsNot0x110(
      state_.Store3RegisterImm5Op_Strb_Rule_198_A1_P392_instance_)
  {}
};

// A(25)=0 & op1(24:20)=0x110
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Strtb_Rule_A1'}
class ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x110_Strtb_Rule_A1
    : public UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x110 {
 public:
  ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x110_Strtb_Rule_A1()
    : UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x110(
      state_.ForbiddenCondDecoder_Strtb_Rule_A1_instance_)
  {}
};

// A(25)=1 & op1(24:20)=0x110 & B(4)=0
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Strtb_Rule_A2'}
class ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x110_B_4Is0_Strtb_Rule_A2
    : public UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x110_B_4Is0 {
 public:
  ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x110_B_4Is0_Strtb_Rule_A2()
    : UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x110_B_4Is0(
      state_.ForbiddenCondDecoder_Strtb_Rule_A2_instance_)
  {}
};

// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x111
//    = Load2RegisterImm12Op {constraints: ,
//     rule: 'Ldrb_Rule_62_A1_P128',
//     safety: ['NotRnIsPc']}
class Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x111NotRnIsPc_Ldrb_Rule_62_A1_P128
    : public LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x111NotRnIsPc {
 public:
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x111NotRnIsPc_Ldrb_Rule_62_A1_P128()
    : LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x111NotRnIsPc(
      state_.Load2RegisterImm12Op_Ldrb_Rule_62_A1_P128_instance_)
  {}
};

// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x111
//    = Load2RegisterImm12Op {constraints: ,
//     rule: 'Ldrb_Rule_63_A1_P130'}
class Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x111_Ldrb_Rule_63_A1_P130
    : public LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x111 {
 public:
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x111_Ldrb_Rule_63_A1_P130()
    : LoadStore2RegisterImm12OpTesterA_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x111(
      state_.Load2RegisterImm12Op_Ldrb_Rule_63_A1_P130_instance_)
  {}
};

// A(25)=1 & op1(24:20)=xx1x1 & B(4)=0 & op1_repeated(24:20)=~0x111
//    = Load3RegisterImm5Op {constraints: ,
//     rule: 'Ldrb_Rule_64_A1_P132'}
class Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x1_B_4Is0_op1_repeated_24To20IsNot0x111_Ldrb_Rule_64_A1_P132
    : public LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x1_B_4Is0_op1_repeated_24To20IsNot0x111 {
 public:
  Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x1_B_4Is0_op1_repeated_24To20IsNot0x111_Ldrb_Rule_64_A1_P132()
    : LoadStore3RegisterImm5OpTesterA_25Is1_op1_24To20Isxx1x1_B_4Is0_op1_repeated_24To20IsNot0x111(
      state_.Load3RegisterImm5Op_Ldrb_Rule_64_A1_P132_instance_)
  {}
};

// A(25)=0 & op1(24:20)=0x111
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Ldrtb_Rule_A1'}
class ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x111_Ldrtb_Rule_A1
    : public UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x111 {
 public:
  ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x111_Ldrtb_Rule_A1()
    : UnsafeCondDecoderTesterA_25Is0_op1_24To20Is0x111(
      state_.ForbiddenCondDecoder_Ldrtb_Rule_A1_instance_)
  {}
};

// A(25)=1 & op1(24:20)=0x111 & B(4)=0
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Ldrtb_Rule_A2'}
class ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x111_B_4Is0_Ldrtb_Rule_A2
    : public UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x111_B_4Is0 {
 public:
  ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x111_B_4Is0_Ldrtb_Rule_A2()
    : UnsafeCondDecoderTesterA_25Is1_op1_24To20Is0x111_B_4Is0(
      state_.ForbiddenCondDecoder_Ldrtb_Rule_A2_instance_)
  {}
};

// Flags(24:21)=1001 & Rn(19:16)=1101 & Imm12(11:0)=000000000100
//    = Store2RegisterImm12OpRnNotRtOnWriteback {constraints: ,
//     rule: 'Push_Rule_123_A2_P248'}
class Store2RegisterImm12OpRnNotRtOnWritebackTester_Flags_24To21Is1001_Rn_19To16Is1101_Imm12_11To0Is000000000100_Push_Rule_123_A2_P248
    : public LoadStore2RegisterImm12OpTesterFlags_24To21Is1001_Rn_19To16Is1101_Imm12_11To0Is000000000100 {
 public:
  Store2RegisterImm12OpRnNotRtOnWritebackTester_Flags_24To21Is1001_Rn_19To16Is1101_Imm12_11To0Is000000000100_Push_Rule_123_A2_P248()
    : LoadStore2RegisterImm12OpTesterFlags_24To21Is1001_Rn_19To16Is1101_Imm12_11To0Is000000000100(
      state_.Store2RegisterImm12OpRnNotRtOnWriteback_Push_Rule_123_A2_P248_instance_)
  {}
};

// 
//    = Store2RegisterImm12Op {constraints: & ~cccc010100101101tttt000000000100 ,
//     rule: 'Str_Rule_194_A1_P384'}
class Store2RegisterImm12OpTester_Notcccc010100101101tttt000000000100_Str_Rule_194_A1_P384
    : public LoadStore2RegisterImm12OpTesterNotcccc010100101101tttt000000000100 {
 public:
  Store2RegisterImm12OpTester_Notcccc010100101101tttt000000000100_Str_Rule_194_A1_P384()
    : LoadStore2RegisterImm12OpTesterNotcccc010100101101tttt000000000100(
      state_.Store2RegisterImm12Op_Str_Rule_194_A1_P384_instance_)
  {}
};

// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: 'Usad8_Rule_253_A1_P500',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_op1_24To20Is11000_op2_7To5Is000_Rd_15To12Is1111RegsNotPc_Usad8_Rule_253_A1_P500
    : public Binary3RegisterOpAltATesterop1_24To20Is11000_op2_7To5Is000_Rd_15To12Is1111RegsNotPc {
 public:
  Binary3RegisterOpAltATester_op1_24To20Is11000_op2_7To5Is000_Rd_15To12Is1111RegsNotPc_Usad8_Rule_253_A1_P500()
    : Binary3RegisterOpAltATesterop1_24To20Is11000_op2_7To5Is000_Rd_15To12Is1111RegsNotPc(
      state_.Binary3RegisterOpAltA_Usad8_Rule_253_A1_P500_instance_)
  {}
};

// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     rule: 'Usada8_Rule_254_A1_P502',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_op1_24To20Is11000_op2_7To5Is000_Rd_15To12IsNot1111RegsNotPc_Usada8_Rule_254_A1_P502
    : public Binary4RegisterDualOpTesterop1_24To20Is11000_op2_7To5Is000_Rd_15To12IsNot1111RegsNotPc {
 public:
  Binary4RegisterDualOpTester_op1_24To20Is11000_op2_7To5Is000_Rd_15To12IsNot1111RegsNotPc_Usada8_Rule_254_A1_P502()
    : Binary4RegisterDualOpTesterop1_24To20Is11000_op2_7To5Is000_Rd_15To12IsNot1111RegsNotPc(
      state_.Binary4RegisterDualOp_Usada8_Rule_254_A1_P502_instance_)
  {}
};

// op1(24:20)=1101x & op2(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract {constraints: ,
//     rule: 'Sbfx_Rule_154_A1_P308',
//     safety: ['RegsNotPc']}
class Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_op1_24To20Is1101x_op2_7To5Isx10RegsNotPc_Sbfx_Rule_154_A1_P308
    : public Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterop1_24To20Is1101x_op2_7To5Isx10RegsNotPc {
 public:
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_op1_24To20Is1101x_op2_7To5Isx10RegsNotPc_Sbfx_Rule_154_A1_P308()
    : Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterop1_24To20Is1101x_op2_7To5Isx10RegsNotPc(
      state_.Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_Sbfx_Rule_154_A1_P308_instance_)
  {}
};

// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=1111
//    = Unary1RegisterBitRangeMsbGeLsb {constraints: ,
//     rule: 'Bfc_17_A1_P46',
//     safety: ['RegsNotPc']}
class Unary1RegisterBitRangeMsbGeLsbTester_op1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111RegsNotPc_Bfc_17_A1_P46
    : public Unary1RegisterBitRangeMsbGeLsbTesterop1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111RegsNotPc {
 public:
  Unary1RegisterBitRangeMsbGeLsbTester_op1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111RegsNotPc_Bfc_17_A1_P46()
    : Unary1RegisterBitRangeMsbGeLsbTesterop1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111RegsNotPc(
      state_.Unary1RegisterBitRangeMsbGeLsb_Bfc_17_A1_P46_instance_)
  {}
};

// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=~1111
//    = Binary2RegisterBitRangeMsbGeLsb {constraints: ,
//     rule: 'Bfi_Rule_18_A1_P48',
//     safety: ['RegsNotPc']}
class Binary2RegisterBitRangeMsbGeLsbTester_op1_24To20Is1110x_op2_7To5Isx00_Rn_3To0IsNot1111RegsNotPc_Bfi_Rule_18_A1_P48
    : public Binary2RegisterBitRangeMsbGeLsbTesterop1_24To20Is1110x_op2_7To5Isx00_Rn_3To0IsNot1111RegsNotPc {
 public:
  Binary2RegisterBitRangeMsbGeLsbTester_op1_24To20Is1110x_op2_7To5Isx00_Rn_3To0IsNot1111RegsNotPc_Bfi_Rule_18_A1_P48()
    : Binary2RegisterBitRangeMsbGeLsbTesterop1_24To20Is1110x_op2_7To5Isx00_Rn_3To0IsNot1111RegsNotPc(
      state_.Binary2RegisterBitRangeMsbGeLsb_Bfi_Rule_18_A1_P48_instance_)
  {}
};

// op1(24:20)=1111x & op2(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract {constraints: ,
//     rule: 'Ubfx_Rule_236_A1_P466',
//     safety: ['RegsNotPc']}
class Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_op1_24To20Is1111x_op2_7To5Isx10RegsNotPc_Ubfx_Rule_236_A1_P466
    : public Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterop1_24To20Is1111x_op2_7To5Isx10RegsNotPc {
 public:
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_op1_24To20Is1111x_op2_7To5Isx10RegsNotPc_Ubfx_Rule_236_A1_P466()
    : Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterop1_24To20Is1111x_op2_7To5Isx10RegsNotPc(
      state_.Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_Ubfx_Rule_236_A1_P466_instance_)
  {}
};

// op1(24:20)=11111 & op2(7:5)=111
//    = Roadblock {constraints: ,
//     rule: 'Udf_Rule_A1'}
class RoadblockTester_op1_24To20Is11111_op2_7To5Is111_Udf_Rule_A1
    : public RoadblockTesterop1_24To20Is11111_op2_7To5Is111 {
 public:
  RoadblockTester_op1_24To20Is11111_op2_7To5Is111_Udf_Rule_A1()
    : RoadblockTesterop1_24To20Is11111_op2_7To5Is111(
      state_.Roadblock_Udf_Rule_A1_instance_)
  {}
};

// op1(26:20)=0010000 & op2(7:4)=xx0x & Rn(19:16)=xxx0
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Cps_Rule_b6_1_1_A1_B6_3'}
class ForbiddenUncondDecoderTester_op1_26To20Is0010000_op2_7To4Isxx0x_Rn_19To16Isxxx0_Cps_Rule_b6_1_1_A1_B6_3
    : public UnsafeUncondDecoderTesterop1_26To20Is0010000_op2_7To4Isxx0x_Rn_19To16Isxxx0 {
 public:
  ForbiddenUncondDecoderTester_op1_26To20Is0010000_op2_7To4Isxx0x_Rn_19To16Isxxx0_Cps_Rule_b6_1_1_A1_B6_3()
    : UnsafeUncondDecoderTesterop1_26To20Is0010000_op2_7To4Isxx0x_Rn_19To16Isxxx0(
      state_.ForbiddenUncondDecoder_Cps_Rule_b6_1_1_A1_B6_3_instance_)
  {}
};

// op1(26:20)=0010000 & op2(7:4)=0000 & Rn(19:16)=xxx1
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Setend_Rule_157_P314'}
class ForbiddenUncondDecoderTester_op1_26To20Is0010000_op2_7To4Is0000_Rn_19To16Isxxx1_Setend_Rule_157_P314
    : public UnsafeUncondDecoderTesterop1_26To20Is0010000_op2_7To4Is0000_Rn_19To16Isxxx1 {
 public:
  ForbiddenUncondDecoderTester_op1_26To20Is0010000_op2_7To4Is0000_Rn_19To16Isxxx1_Setend_Rule_157_P314()
    : UnsafeUncondDecoderTesterop1_26To20Is0010000_op2_7To4Is0000_Rn_19To16Isxxx1(
      state_.ForbiddenUncondDecoder_Setend_Rule_157_P314_instance_)
  {}
};

// op1(26:20)=100x001
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Unallocated_hints'}
class ForbiddenUncondDecoderTester_op1_26To20Is100x001_Unallocated_hints
    : public UnsafeUncondDecoderTesterop1_26To20Is100x001 {
 public:
  ForbiddenUncondDecoderTester_op1_26To20Is100x001_Unallocated_hints()
    : UnsafeUncondDecoderTesterop1_26To20Is100x001(
      state_.ForbiddenUncondDecoder_Unallocated_hints_instance_)
  {}
};

// op1(26:20)=100x101
//    = PreloadRegisterImm12Op {constraints: ,
//     rule: 'Pli_Rule_120_A1_P242'}
class PreloadRegisterImm12OpTester_op1_26To20Is100x101_Pli_Rule_120_A1_P242
    : public PreloadRegisterImm12OpTesterop1_26To20Is100x101 {
 public:
  PreloadRegisterImm12OpTester_op1_26To20Is100x101_Pli_Rule_120_A1_P242()
    : PreloadRegisterImm12OpTesterop1_26To20Is100x101(
      state_.PreloadRegisterImm12Op_Pli_Rule_120_A1_P242_instance_)
  {}
};

// op1(26:20)=100xx11
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: 'Unpredictable'}
class UnpredictableUncondDecoderTester_op1_26To20Is100xx11_Unpredictable
    : public UnsafeUncondDecoderTesterop1_26To20Is100xx11 {
 public:
  UnpredictableUncondDecoderTester_op1_26To20Is100xx11_Unpredictable()
    : UnsafeUncondDecoderTesterop1_26To20Is100xx11(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// op1(26:20)=101x001 & Rn(19:16)=~1111
//    = PreloadRegisterImm12Op {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     rule: 'Pldw_Rule_117_A1_P236'}
class PreloadRegisterImm12OpTester_op1_26To20Is101x001_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx_Pldw_Rule_117_A1_P236
    : public PreloadRegisterImm12OpTesterop1_26To20Is101x001_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx {
 public:
  PreloadRegisterImm12OpTester_op1_26To20Is101x001_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx_Pldw_Rule_117_A1_P236()
    : PreloadRegisterImm12OpTesterop1_26To20Is101x001_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx(
      state_.PreloadRegisterImm12Op_Pldw_Rule_117_A1_P236_instance_)
  {}
};

// op1(26:20)=101x001 & Rn(19:16)=1111
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: 'Unpredictable'}
class UnpredictableUncondDecoderTester_op1_26To20Is101x001_Rn_19To16Is1111_Unpredictable
    : public UnsafeUncondDecoderTesterop1_26To20Is101x001_Rn_19To16Is1111 {
 public:
  UnpredictableUncondDecoderTester_op1_26To20Is101x001_Rn_19To16Is1111_Unpredictable()
    : UnsafeUncondDecoderTesterop1_26To20Is101x001_Rn_19To16Is1111(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// op1(26:20)=101x101 & Rn(19:16)=~1111
//    = PreloadRegisterImm12Op {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     rule: 'Pld_Rule_117_A1_P236'}
class PreloadRegisterImm12OpTester_op1_26To20Is101x101_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx_Pld_Rule_117_A1_P236
    : public PreloadRegisterImm12OpTesterop1_26To20Is101x101_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx {
 public:
  PreloadRegisterImm12OpTester_op1_26To20Is101x101_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx_Pld_Rule_117_A1_P236()
    : PreloadRegisterImm12OpTesterop1_26To20Is101x101_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx(
      state_.PreloadRegisterImm12Op_Pld_Rule_117_A1_P236_instance_)
  {}
};

// op1(26:20)=101x101 & Rn(19:16)=1111
//    = PreloadRegisterImm12Op {constraints: ,
//     rule: 'Pld_Rule_118_A1_P238'}
class PreloadRegisterImm12OpTester_op1_26To20Is101x101_Rn_19To16Is1111_Pld_Rule_118_A1_P238
    : public PreloadRegisterImm12OpTesterop1_26To20Is101x101_Rn_19To16Is1111 {
 public:
  PreloadRegisterImm12OpTester_op1_26To20Is101x101_Rn_19To16Is1111_Pld_Rule_118_A1_P238()
    : PreloadRegisterImm12OpTesterop1_26To20Is101x101_Rn_19To16Is1111(
      state_.PreloadRegisterImm12Op_Pld_Rule_118_A1_P238_instance_)
  {}
};

// op1(26:20)=1010011
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: 'Unpredictable'}
class UnpredictableUncondDecoderTester_op1_26To20Is1010011_Unpredictable
    : public UnsafeUncondDecoderTesterop1_26To20Is1010011 {
 public:
  UnpredictableUncondDecoderTester_op1_26To20Is1010011_Unpredictable()
    : UnsafeUncondDecoderTesterop1_26To20Is1010011(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=0000
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: 'Unpredictable'}
class UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is0000_Unpredictable
    : public UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is0000 {
 public:
  UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is0000_Unpredictable()
    : UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is0000(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=0001
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Clrex_Rule_30_A1_P70'}
class ForbiddenUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is0001_Clrex_Rule_30_A1_P70
    : public UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is0001 {
 public:
  ForbiddenUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is0001_Clrex_Rule_30_A1_P70()
    : UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is0001(
      state_.ForbiddenUncondDecoder_Clrex_Rule_30_A1_P70_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=001x
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: 'Unpredictable'}
class UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is001x_Unpredictable
    : public UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is001x {
 public:
  UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is001x_Unpredictable()
    : UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is001x(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=0100
//    = DataBarrier {constraints: ,
//     rule: 'Dsb_Rule_42_A1_P92'}
class DataBarrierTester_op1_26To20Is1010111_op2_7To4Is0100_Dsb_Rule_42_A1_P92
    : public DataBarrierTesterop1_26To20Is1010111_op2_7To4Is0100 {
 public:
  DataBarrierTester_op1_26To20Is1010111_op2_7To4Is0100_Dsb_Rule_42_A1_P92()
    : DataBarrierTesterop1_26To20Is1010111_op2_7To4Is0100(
      state_.DataBarrier_Dsb_Rule_42_A1_P92_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=0101
//    = DataBarrier {constraints: ,
//     rule: 'Dmb_Rule_41_A1_P90'}
class DataBarrierTester_op1_26To20Is1010111_op2_7To4Is0101_Dmb_Rule_41_A1_P90
    : public DataBarrierTesterop1_26To20Is1010111_op2_7To4Is0101 {
 public:
  DataBarrierTester_op1_26To20Is1010111_op2_7To4Is0101_Dmb_Rule_41_A1_P90()
    : DataBarrierTesterop1_26To20Is1010111_op2_7To4Is0101(
      state_.DataBarrier_Dmb_Rule_41_A1_P90_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=0110
//    = InstructionBarrier {constraints: ,
//     rule: 'Isb_Rule_49_A1_P102'}
class InstructionBarrierTester_op1_26To20Is1010111_op2_7To4Is0110_Isb_Rule_49_A1_P102
    : public InstructionBarrierTesterop1_26To20Is1010111_op2_7To4Is0110 {
 public:
  InstructionBarrierTester_op1_26To20Is1010111_op2_7To4Is0110_Isb_Rule_49_A1_P102()
    : InstructionBarrierTesterop1_26To20Is1010111_op2_7To4Is0110(
      state_.InstructionBarrier_Isb_Rule_49_A1_P102_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=0111
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: 'Unpredictable'}
class UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is0111_Unpredictable
    : public UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is0111 {
 public:
  UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is0111_Unpredictable()
    : UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is0111(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=1xxx
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: 'Unpredictable'}
class UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is1xxx_Unpredictable
    : public UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is1xxx {
 public:
  UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is1xxx_Unpredictable()
    : UnsafeUncondDecoderTesterop1_26To20Is1010111_op2_7To4Is1xxx(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// op1(26:20)=1011x11
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: 'Unpredictable'}
class UnpredictableUncondDecoderTester_op1_26To20Is1011x11_Unpredictable
    : public UnsafeUncondDecoderTesterop1_26To20Is1011x11 {
 public:
  UnpredictableUncondDecoderTester_op1_26To20Is1011x11_Unpredictable()
    : UnsafeUncondDecoderTesterop1_26To20Is1011x11(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// op1(26:20)=110x001 & op2(7:4)=xxx0
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Unallocated_hints'}
class ForbiddenUncondDecoderTester_op1_26To20Is110x001_op2_7To4Isxxx0_Unallocated_hints
    : public UnsafeUncondDecoderTesterop1_26To20Is110x001_op2_7To4Isxxx0 {
 public:
  ForbiddenUncondDecoderTester_op1_26To20Is110x001_op2_7To4Isxxx0_Unallocated_hints()
    : UnsafeUncondDecoderTesterop1_26To20Is110x001_op2_7To4Isxxx0(
      state_.ForbiddenUncondDecoder_Unallocated_hints_instance_)
  {}
};

// op1(26:20)=110x101 & op2(7:4)=xxx0
//    = PreloadRegisterPairOp {constraints: ,
//     rule: 'Pli_Rule_121_A1_P244'}
class PreloadRegisterPairOpTester_op1_26To20Is110x101_op2_7To4Isxxx0_Pli_Rule_121_A1_P244
    : public PreloadRegisterPairOpTesterop1_26To20Is110x101_op2_7To4Isxxx0 {
 public:
  PreloadRegisterPairOpTester_op1_26To20Is110x101_op2_7To4Isxxx0_Pli_Rule_121_A1_P244()
    : PreloadRegisterPairOpTesterop1_26To20Is110x101_op2_7To4Isxxx0(
      state_.PreloadRegisterPairOp_Pli_Rule_121_A1_P244_instance_)
  {}
};

// op1(26:20)=111x001 & op2(7:4)=xxx0
//    = PreloadRegisterPairOpWAndRnNotPc {constraints: ,
//     rule: 'Pldw_Rule_119_A1_P240'}
class PreloadRegisterPairOpWAndRnNotPcTester_op1_26To20Is111x001_op2_7To4Isxxx0_Pldw_Rule_119_A1_P240
    : public PreloadRegisterPairOpWAndRnNotPcTesterop1_26To20Is111x001_op2_7To4Isxxx0 {
 public:
  PreloadRegisterPairOpWAndRnNotPcTester_op1_26To20Is111x001_op2_7To4Isxxx0_Pldw_Rule_119_A1_P240()
    : PreloadRegisterPairOpWAndRnNotPcTesterop1_26To20Is111x001_op2_7To4Isxxx0(
      state_.PreloadRegisterPairOpWAndRnNotPc_Pldw_Rule_119_A1_P240_instance_)
  {}
};

// op1(26:20)=111x101 & op2(7:4)=xxx0
//    = PreloadRegisterPairOpWAndRnNotPc {constraints: ,
//     rule: 'Pld_Rule_119_A1_P240'}
class PreloadRegisterPairOpWAndRnNotPcTester_op1_26To20Is111x101_op2_7To4Isxxx0_Pld_Rule_119_A1_P240
    : public PreloadRegisterPairOpWAndRnNotPcTesterop1_26To20Is111x101_op2_7To4Isxxx0 {
 public:
  PreloadRegisterPairOpWAndRnNotPcTester_op1_26To20Is111x101_op2_7To4Isxxx0_Pld_Rule_119_A1_P240()
    : PreloadRegisterPairOpWAndRnNotPcTesterop1_26To20Is111x101_op2_7To4Isxxx0(
      state_.PreloadRegisterPairOpWAndRnNotPc_Pld_Rule_119_A1_P240_instance_)
  {}
};

// op1(26:20)=11xxx11 & op2(7:4)=xxx0
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: 'Unpredictable'}
class UnpredictableUncondDecoderTester_op1_26To20Is11xxx11_op2_7To4Isxxx0_Unpredictable
    : public UnsafeUncondDecoderTesterop1_26To20Is11xxx11_op2_7To4Isxxx0 {
 public:
  UnpredictableUncondDecoderTester_op1_26To20Is11xxx11_op2_7To4Isxxx0_Unpredictable()
    : UnsafeUncondDecoderTesterop1_26To20Is11xxx11_op2_7To4Isxxx0(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// op2(6:4)=000 & B(9)=1 & op(22:21)=x0
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Msr_Rule_Banked_register_A1_B9_1990'}
class ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is1_op_22To21Isx0_Msr_Rule_Banked_register_A1_B9_1990
    : public UnsafeCondDecoderTesterop2_6To4Is000_B_9Is1_op_22To21Isx0 {
 public:
  ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is1_op_22To21Isx0_Msr_Rule_Banked_register_A1_B9_1990()
    : UnsafeCondDecoderTesterop2_6To4Is000_B_9Is1_op_22To21Isx0(
      state_.ForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1990_instance_)
  {}
};

// op2(6:4)=000 & B(9)=1 & op(22:21)=x1
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Msr_Rule_Banked_register_A1_B9_1992'}
class ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is1_op_22To21Isx1_Msr_Rule_Banked_register_A1_B9_1992
    : public UnsafeCondDecoderTesterop2_6To4Is000_B_9Is1_op_22To21Isx1 {
 public:
  ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is1_op_22To21Isx1_Msr_Rule_Banked_register_A1_B9_1992()
    : UnsafeCondDecoderTesterop2_6To4Is000_B_9Is1_op_22To21Isx1(
      state_.ForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1992_instance_)
  {}
};

// op2(6:4)=000 & B(9)=0 & op(22:21)=x0
//    = Unary1RegisterSet {constraints: ,
//     rule: 'Mrs_Rule_102_A1_P206_Or_B6_10'}
class Unary1RegisterSetTester_op2_6To4Is000_B_9Is0_op_22To21Isx0_Mrs_Rule_102_A1_P206_Or_B6_10
    : public Unary1RegisterSetTesterop2_6To4Is000_B_9Is0_op_22To21Isx0 {
 public:
  Unary1RegisterSetTester_op2_6To4Is000_B_9Is0_op_22To21Isx0_Mrs_Rule_102_A1_P206_Or_B6_10()
    : Unary1RegisterSetTesterop2_6To4Is000_B_9Is0_op_22To21Isx0(
      state_.Unary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10_instance_)
  {}
};

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00
//    = Unary1RegisterUse {constraints: ,
//     rule: 'Msr_Rule_104_A1_P210'}
class Unary1RegisterUseTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx00_Msr_Rule_104_A1_P210
    : public Unary1RegisterUseTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx00 {
 public:
  Unary1RegisterUseTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx00_Msr_Rule_104_A1_P210()
    : Unary1RegisterUseTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx00(
      state_.Unary1RegisterUse_Msr_Rule_104_A1_P210_instance_)
  {}
};

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx01
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Msr_Rule_B6_1_7_P14'}
class ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01_Msr_Rule_B6_1_7_P14
    : public UnsafeCondDecoderTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01 {
 public:
  ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01_Msr_Rule_B6_1_7_P14()
    : UnsafeCondDecoderTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_7_P14_instance_)
  {}
};

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx1x
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Msr_Rule_B6_1_7_P14'}
class ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x_Msr_Rule_B6_1_7_P14
    : public UnsafeCondDecoderTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x {
 public:
  ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x_Msr_Rule_B6_1_7_P14()
    : UnsafeCondDecoderTesterop2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_7_P14_instance_)
  {}
};

// op2(6:4)=000 & B(9)=0 & op(22:21)=11
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Msr_Rule_B6_1_7_P14'}
class ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is0_op_22To21Is11_Msr_Rule_B6_1_7_P14
    : public UnsafeCondDecoderTesterop2_6To4Is000_B_9Is0_op_22To21Is11 {
 public:
  ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is0_op_22To21Is11_Msr_Rule_B6_1_7_P14()
    : UnsafeCondDecoderTesterop2_6To4Is000_B_9Is0_op_22To21Is11(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_7_P14_instance_)
  {}
};

// op2(6:4)=001 & op(22:21)=01
//    = BranchToRegister {constraints: ,
//     rule: 'Bx_Rule_25_A1_P62'}
class BranchToRegisterTester_op2_6To4Is001_op_22To21Is01_Bx_Rule_25_A1_P62
    : public BranchToRegisterTesterop2_6To4Is001_op_22To21Is01 {
 public:
  BranchToRegisterTester_op2_6To4Is001_op_22To21Is01_Bx_Rule_25_A1_P62()
    : BranchToRegisterTesterop2_6To4Is001_op_22To21Is01(
      state_.BranchToRegister_Bx_Rule_25_A1_P62_instance_)
  {}
};

// op2(6:4)=001 & op(22:21)=11
//    = Unary2RegisterOpNotRmIsPc {constraints: ,
//     rule: 'Clz_Rule_31_A1_P72'}
class Unary2RegisterOpNotRmIsPcTester_op2_6To4Is001_op_22To21Is11_Clz_Rule_31_A1_P72
    : public Unary2RegisterOpNotRmIsPcTesterop2_6To4Is001_op_22To21Is11 {
 public:
  Unary2RegisterOpNotRmIsPcTester_op2_6To4Is001_op_22To21Is11_Clz_Rule_31_A1_P72()
    : Unary2RegisterOpNotRmIsPcTesterop2_6To4Is001_op_22To21Is11(
      state_.Unary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72_instance_)
  {}
};

// op2(6:4)=010 & op(22:21)=01
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Bxj_Rule_26_A1_P64'}
class ForbiddenCondDecoderTester_op2_6To4Is010_op_22To21Is01_Bxj_Rule_26_A1_P64
    : public UnsafeCondDecoderTesterop2_6To4Is010_op_22To21Is01 {
 public:
  ForbiddenCondDecoderTester_op2_6To4Is010_op_22To21Is01_Bxj_Rule_26_A1_P64()
    : UnsafeCondDecoderTesterop2_6To4Is010_op_22To21Is01(
      state_.ForbiddenCondDecoder_Bxj_Rule_26_A1_P64_instance_)
  {}
};

// op2(6:4)=011 & op(22:21)=01
//    = BranchToRegister {constraints: ,
//     rule: 'Blx_Rule_24_A1_P60',
//     safety: ['RegsNotPc']}
class BranchToRegisterTester_op2_6To4Is011_op_22To21Is01RegsNotPc_Blx_Rule_24_A1_P60
    : public BranchToRegisterTesterop2_6To4Is011_op_22To21Is01RegsNotPc {
 public:
  BranchToRegisterTester_op2_6To4Is011_op_22To21Is01RegsNotPc_Blx_Rule_24_A1_P60()
    : BranchToRegisterTesterop2_6To4Is011_op_22To21Is01RegsNotPc(
      state_.BranchToRegister_Blx_Rule_24_A1_P60_instance_)
  {}
};

// op2(6:4)=110 & op(22:21)=11
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Eret_Rule_A1'}
class ForbiddenCondDecoderTester_op2_6To4Is110_op_22To21Is11_Eret_Rule_A1
    : public UnsafeCondDecoderTesterop2_6To4Is110_op_22To21Is11 {
 public:
  ForbiddenCondDecoderTester_op2_6To4Is110_op_22To21Is11_Eret_Rule_A1()
    : UnsafeCondDecoderTesterop2_6To4Is110_op_22To21Is11(
      state_.ForbiddenCondDecoder_Eret_Rule_A1_instance_)
  {}
};

// op2(6:4)=111 & op(22:21)=01
//    = BreakPointAndConstantPoolHead {constraints: ,
//     rule: 'Bkpt_Rule_22_A1_P56'}
class BreakPointAndConstantPoolHeadTester_op2_6To4Is111_op_22To21Is01_Bkpt_Rule_22_A1_P56
    : public Immediate16UseTesterop2_6To4Is111_op_22To21Is01 {
 public:
  BreakPointAndConstantPoolHeadTester_op2_6To4Is111_op_22To21Is01_Bkpt_Rule_22_A1_P56()
    : Immediate16UseTesterop2_6To4Is111_op_22To21Is01(
      state_.BreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56_instance_)
  {}
};

// op2(6:4)=111 & op(22:21)=10
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Hvc_Rule_A1'}
class ForbiddenCondDecoderTester_op2_6To4Is111_op_22To21Is10_Hvc_Rule_A1
    : public UnsafeCondDecoderTesterop2_6To4Is111_op_22To21Is10 {
 public:
  ForbiddenCondDecoderTester_op2_6To4Is111_op_22To21Is10_Hvc_Rule_A1()
    : UnsafeCondDecoderTesterop2_6To4Is111_op_22To21Is10(
      state_.ForbiddenCondDecoder_Hvc_Rule_A1_instance_)
  {}
};

// op2(6:4)=111 & op(22:21)=11
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Smc_Rule_B6_1_9_P18'}
class ForbiddenCondDecoderTester_op2_6To4Is111_op_22To21Is11_Smc_Rule_B6_1_9_P18
    : public UnsafeCondDecoderTesterop2_6To4Is111_op_22To21Is11 {
 public:
  ForbiddenCondDecoderTester_op2_6To4Is111_op_22To21Is11_Smc_Rule_B6_1_9_P18()
    : UnsafeCondDecoderTesterop2_6To4Is111_op_22To21Is11(
      state_.ForbiddenCondDecoder_Smc_Rule_B6_1_9_P18_instance_)
  {}
};

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000000
//    = CondDecoder {constraints: ,
//     rule: 'Nop_Rule_110_A1_P222'}
class CondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000000_Nop_Rule_110_A1_P222
    : public CondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000000 {
 public:
  CondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000000_Nop_Rule_110_A1_P222()
    : CondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000000(
      state_.CondDecoder_Nop_Rule_110_A1_P222_instance_)
  {}
};

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000001
//    = CondDecoder {constraints: ,
//     rule: 'Yield_Rule_413_A1_P812'}
class CondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000001_Yield_Rule_413_A1_P812
    : public CondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000001 {
 public:
  CondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000001_Yield_Rule_413_A1_P812()
    : CondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000001(
      state_.CondDecoder_Yield_Rule_413_A1_P812_instance_)
  {}
};

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000010
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Wfe_Rule_411_A1_P808'}
class ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000010_Wfe_Rule_411_A1_P808
    : public UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000010 {
 public:
  ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000010_Wfe_Rule_411_A1_P808()
    : UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000010(
      state_.ForbiddenCondDecoder_Wfe_Rule_411_A1_P808_instance_)
  {}
};

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000011
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Wfi_Rule_412_A1_P810'}
class ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000011_Wfi_Rule_412_A1_P810
    : public UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000011 {
 public:
  ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000011_Wfi_Rule_412_A1_P810()
    : UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000011(
      state_.ForbiddenCondDecoder_Wfi_Rule_412_A1_P810_instance_)
  {}
};

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000100
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Sev_Rule_158_A1_P316'}
class ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000100_Sev_Rule_158_A1_P316
    : public UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000100 {
 public:
  ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000100_Sev_Rule_158_A1_P316()
    : UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is00000100(
      state_.ForbiddenCondDecoder_Sev_Rule_158_A1_P316_instance_)
  {}
};

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=1111xxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Dbg_Rule_40_A1_P88'}
class ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx_Dbg_Rule_40_A1_P88
    : public UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx {
 public:
  ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx_Dbg_Rule_40_A1_P88()
    : UnsafeCondDecoderTesterop_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx(
      state_.ForbiddenCondDecoder_Dbg_Rule_40_A1_P88_instance_)
  {}
};

// op(22)=0 & op1(19:16)=0100
//    = MoveImmediate12ToApsr {constraints: ,
//     rule: 'Msr_Rule_103_A1_P208'}
class MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is0100_Msr_Rule_103_A1_P208
    : public MoveImmediate12ToApsrTesterop_22Is0_op1_19To16Is0100 {
 public:
  MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is0100_Msr_Rule_103_A1_P208()
    : MoveImmediate12ToApsrTesterop_22Is0_op1_19To16Is0100(
      state_.MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_)
  {}
};

// op(22)=0 & op1(19:16)=1x00
//    = MoveImmediate12ToApsr {constraints: ,
//     rule: 'Msr_Rule_103_A1_P208'}
class MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is1x00_Msr_Rule_103_A1_P208
    : public MoveImmediate12ToApsrTesterop_22Is0_op1_19To16Is1x00 {
 public:
  MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is1x00_Msr_Rule_103_A1_P208()
    : MoveImmediate12ToApsrTesterop_22Is0_op1_19To16Is1x00(
      state_.MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_)
  {}
};

// op(22)=0 & op1(19:16)=xx01
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Msr_Rule_B6_1_6_A1_PB6_12'}
class ForbiddenCondDecoderTester_op_22Is0_op1_19To16Isxx01_Msr_Rule_B6_1_6_A1_PB6_12
    : public UnsafeCondDecoderTesterop_22Is0_op1_19To16Isxx01 {
 public:
  ForbiddenCondDecoderTester_op_22Is0_op1_19To16Isxx01_Msr_Rule_B6_1_6_A1_PB6_12()
    : UnsafeCondDecoderTesterop_22Is0_op1_19To16Isxx01(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12_instance_)
  {}
};

// op(22)=0 & op1(19:16)=xx1x
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Msr_Rule_B6_1_6_A1_PB6_12'}
class ForbiddenCondDecoderTester_op_22Is0_op1_19To16Isxx1x_Msr_Rule_B6_1_6_A1_PB6_12
    : public UnsafeCondDecoderTesterop_22Is0_op1_19To16Isxx1x {
 public:
  ForbiddenCondDecoderTester_op_22Is0_op1_19To16Isxx1x_Msr_Rule_B6_1_6_A1_PB6_12()
    : UnsafeCondDecoderTesterop_22Is0_op1_19To16Isxx1x(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12_instance_)
  {}
};

// op(22)=1
//    = ForbiddenCondDecoder {constraints: ,
//     rule: 'Msr_Rule_B6_1_6_A1_PB6_12'}
class ForbiddenCondDecoderTester_op_22Is1_Msr_Rule_B6_1_6_A1_PB6_12
    : public UnsafeCondDecoderTesterop_22Is1 {
 public:
  ForbiddenCondDecoderTester_op_22Is1_Msr_Rule_B6_1_6_A1_PB6_12()
    : UnsafeCondDecoderTesterop_22Is1(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12_instance_)
  {}
};

// op(23:20)=000x
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: 'Mul_Rule_105_A1_P212',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_op_23To20Is000xRegsNotPc_Mul_Rule_105_A1_P212
    : public Binary3RegisterOpAltATesterop_23To20Is000xRegsNotPc {
 public:
  Binary3RegisterOpAltATester_op_23To20Is000xRegsNotPc_Mul_Rule_105_A1_P212()
    : Binary3RegisterOpAltATesterop_23To20Is000xRegsNotPc(
      state_.Binary3RegisterOpAltA_Mul_Rule_105_A1_P212_instance_)
  {}
};

// op(23:20)=001x
//    = Binary4RegisterDualOp {constraints: ,
//     rule: 'Mla_Rule_94_A1_P190',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_op_23To20Is001xRegsNotPc_Mla_Rule_94_A1_P190
    : public Binary4RegisterDualOpTesterop_23To20Is001xRegsNotPc {
 public:
  Binary4RegisterDualOpTester_op_23To20Is001xRegsNotPc_Mla_Rule_94_A1_P190()
    : Binary4RegisterDualOpTesterop_23To20Is001xRegsNotPc(
      state_.Binary4RegisterDualOp_Mla_Rule_94_A1_P190_instance_)
  {}
};

// op(23:20)=0100
//    = Binary4RegisterDualResult {constraints: ,
//     rule: 'Umaal_Rule_244_A1_P482',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_op_23To20Is0100RegsNotPc_Umaal_Rule_244_A1_P482
    : public Binary4RegisterDualResultTesterop_23To20Is0100RegsNotPc {
 public:
  Binary4RegisterDualResultTester_op_23To20Is0100RegsNotPc_Umaal_Rule_244_A1_P482()
    : Binary4RegisterDualResultTesterop_23To20Is0100RegsNotPc(
      state_.Binary4RegisterDualResult_Umaal_Rule_244_A1_P482_instance_)
  {}
};

// op(23:20)=0101
//    = UndefinedCondDecoder {constraints: ,
//     rule: 'Undefined_A5_2_5_0101'}
class UndefinedCondDecoderTester_op_23To20Is0101_Undefined_A5_2_5_0101
    : public UnsafeCondDecoderTesterop_23To20Is0101 {
 public:
  UndefinedCondDecoderTester_op_23To20Is0101_Undefined_A5_2_5_0101()
    : UnsafeCondDecoderTesterop_23To20Is0101(
      state_.UndefinedCondDecoder_Undefined_A5_2_5_0101_instance_)
  {}
};

// op(23:20)=0110
//    = Binary4RegisterDualOp {constraints: ,
//     rule: 'Mls_Rule_95_A1_P192',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_op_23To20Is0110RegsNotPc_Mls_Rule_95_A1_P192
    : public Binary4RegisterDualOpTesterop_23To20Is0110RegsNotPc {
 public:
  Binary4RegisterDualOpTester_op_23To20Is0110RegsNotPc_Mls_Rule_95_A1_P192()
    : Binary4RegisterDualOpTesterop_23To20Is0110RegsNotPc(
      state_.Binary4RegisterDualOp_Mls_Rule_95_A1_P192_instance_)
  {}
};

// op(23:20)=0111
//    = UndefinedCondDecoder {constraints: ,
//     rule: 'Undefined_A5_2_5_0111'}
class UndefinedCondDecoderTester_op_23To20Is0111_Undefined_A5_2_5_0111
    : public UnsafeCondDecoderTesterop_23To20Is0111 {
 public:
  UndefinedCondDecoderTester_op_23To20Is0111_Undefined_A5_2_5_0111()
    : UnsafeCondDecoderTesterop_23To20Is0111(
      state_.UndefinedCondDecoder_Undefined_A5_2_5_0111_instance_)
  {}
};

// op(23:20)=100x
//    = Binary4RegisterDualResult {constraints: ,
//     rule: 'Umull_Rule_246_A1_P486',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_op_23To20Is100xRegsNotPc_Umull_Rule_246_A1_P486
    : public Binary4RegisterDualResultTesterop_23To20Is100xRegsNotPc {
 public:
  Binary4RegisterDualResultTester_op_23To20Is100xRegsNotPc_Umull_Rule_246_A1_P486()
    : Binary4RegisterDualResultTesterop_23To20Is100xRegsNotPc(
      state_.Binary4RegisterDualResult_Umull_Rule_246_A1_P486_instance_)
  {}
};

// op(23:20)=101x
//    = Binary4RegisterDualResult {constraints: ,
//     rule: 'Umlal_Rule_245_A1_P484',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_op_23To20Is101xRegsNotPc_Umlal_Rule_245_A1_P484
    : public Binary4RegisterDualResultTesterop_23To20Is101xRegsNotPc {
 public:
  Binary4RegisterDualResultTester_op_23To20Is101xRegsNotPc_Umlal_Rule_245_A1_P484()
    : Binary4RegisterDualResultTesterop_23To20Is101xRegsNotPc(
      state_.Binary4RegisterDualResult_Umlal_Rule_245_A1_P484_instance_)
  {}
};

// op(23:20)=110x
//    = Binary4RegisterDualResult {constraints: ,
//     rule: 'Smull_Rule_179_A1_P356',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_op_23To20Is110xRegsNotPc_Smull_Rule_179_A1_P356
    : public Binary4RegisterDualResultTesterop_23To20Is110xRegsNotPc {
 public:
  Binary4RegisterDualResultTester_op_23To20Is110xRegsNotPc_Smull_Rule_179_A1_P356()
    : Binary4RegisterDualResultTesterop_23To20Is110xRegsNotPc(
      state_.Binary4RegisterDualResult_Smull_Rule_179_A1_P356_instance_)
  {}
};

// op(23:20)=111x
//    = Binary4RegisterDualResult {constraints: ,
//     rule: 'Smlal_Rule_168_A1_P334',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_op_23To20Is111xRegsNotPc_Smlal_Rule_168_A1_P334
    : public Binary4RegisterDualResultTesterop_23To20Is111xRegsNotPc {
 public:
  Binary4RegisterDualResultTester_op_23To20Is111xRegsNotPc_Smlal_Rule_168_A1_P334()
    : Binary4RegisterDualResultTesterop_23To20Is111xRegsNotPc(
      state_.Binary4RegisterDualResult_Smlal_Rule_168_A1_P334_instance_)
  {}
};

// opc3(7:6)=x0
//    = CondVfpOp {constraints: ,
//     rule: 'Vmov_Rule_326_A2_P640'}
class CondVfpOpTester_opc3_7To6Isx0_Vmov_Rule_326_A2_P640
    : public CondVfpOpTesteropc3_7To6Isx0 {
 public:
  CondVfpOpTester_opc3_7To6Isx0_Vmov_Rule_326_A2_P640()
    : CondVfpOpTesteropc3_7To6Isx0(
      state_.CondVfpOp_Vmov_Rule_326_A2_P640_instance_)
  {}
};

// opc2(19:16)=0000 & opc3(7:6)=01
//    = CondVfpOp {constraints: ,
//     rule: 'Vmov_Rule_327_A2_P642'}
class CondVfpOpTester_opc2_19To16Is0000_opc3_7To6Is01_Vmov_Rule_327_A2_P642
    : public CondVfpOpTesteropc2_19To16Is0000_opc3_7To6Is01 {
 public:
  CondVfpOpTester_opc2_19To16Is0000_opc3_7To6Is01_Vmov_Rule_327_A2_P642()
    : CondVfpOpTesteropc2_19To16Is0000_opc3_7To6Is01(
      state_.CondVfpOp_Vmov_Rule_327_A2_P642_instance_)
  {}
};

// opc2(19:16)=0000 & opc3(7:6)=11
//    = CondVfpOp {constraints: ,
//     rule: 'Vabs_Rule_269_A2_P532'}
class CondVfpOpTester_opc2_19To16Is0000_opc3_7To6Is11_Vabs_Rule_269_A2_P532
    : public CondVfpOpTesteropc2_19To16Is0000_opc3_7To6Is11 {
 public:
  CondVfpOpTester_opc2_19To16Is0000_opc3_7To6Is11_Vabs_Rule_269_A2_P532()
    : CondVfpOpTesteropc2_19To16Is0000_opc3_7To6Is11(
      state_.CondVfpOp_Vabs_Rule_269_A2_P532_instance_)
  {}
};

// opc2(19:16)=0001 & opc3(7:6)=01
//    = CondVfpOp {constraints: ,
//     rule: 'Vneg_Rule_342_A2_P672'}
class CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is01_Vneg_Rule_342_A2_P672
    : public CondVfpOpTesteropc2_19To16Is0001_opc3_7To6Is01 {
 public:
  CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is01_Vneg_Rule_342_A2_P672()
    : CondVfpOpTesteropc2_19To16Is0001_opc3_7To6Is01(
      state_.CondVfpOp_Vneg_Rule_342_A2_P672_instance_)
  {}
};

// opc2(19:16)=0001 & opc3(7:6)=11
//    = CondVfpOp {constraints: ,
//     rule: 'Vsqrt_Rule_388_A1_P762'}
class CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is11_Vsqrt_Rule_388_A1_P762
    : public CondVfpOpTesteropc2_19To16Is0001_opc3_7To6Is11 {
 public:
  CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is11_Vsqrt_Rule_388_A1_P762()
    : CondVfpOpTesteropc2_19To16Is0001_opc3_7To6Is11(
      state_.CondVfpOp_Vsqrt_Rule_388_A1_P762_instance_)
  {}
};

// opc2(19:16)=001x & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: 'Vcvtb_Vcvtt_Rule_300_A1_P588'}
class CondVfpOpTester_opc2_19To16Is001x_opc3_7To6Isx1_Vcvtb_Vcvtt_Rule_300_A1_P588
    : public CondVfpOpTesteropc2_19To16Is001x_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc2_19To16Is001x_opc3_7To6Isx1_Vcvtb_Vcvtt_Rule_300_A1_P588()
    : CondVfpOpTesteropc2_19To16Is001x_opc3_7To6Isx1(
      state_.CondVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588_instance_)
  {}
};

// opc2(19:16)=0100 & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: 'Vcmp_Vcmpe_Rule_A1'}
class CondVfpOpTester_opc2_19To16Is0100_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_A1
    : public CondVfpOpTesteropc2_19To16Is0100_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc2_19To16Is0100_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_A1()
    : CondVfpOpTesteropc2_19To16Is0100_opc3_7To6Isx1(
      state_.CondVfpOp_Vcmp_Vcmpe_Rule_A1_instance_)
  {}
};

// opc2(19:16)=0101 & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: 'Vcmp_Vcmpe_Rule_A2'}
class CondVfpOpTester_opc2_19To16Is0101_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_A2
    : public CondVfpOpTesteropc2_19To16Is0101_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc2_19To16Is0101_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_A2()
    : CondVfpOpTesteropc2_19To16Is0101_opc3_7To6Isx1(
      state_.CondVfpOp_Vcmp_Vcmpe_Rule_A2_instance_)
  {}
};

// opc2(19:16)=0111 & opc3(7:6)=11
//    = CondVfpOp {constraints: ,
//     rule: 'Vcvt_Rule_298_A1_P584'}
class CondVfpOpTester_opc2_19To16Is0111_opc3_7To6Is11_Vcvt_Rule_298_A1_P584
    : public CondVfpOpTesteropc2_19To16Is0111_opc3_7To6Is11 {
 public:
  CondVfpOpTester_opc2_19To16Is0111_opc3_7To6Is11_Vcvt_Rule_298_A1_P584()
    : CondVfpOpTesteropc2_19To16Is0111_opc3_7To6Is11(
      state_.CondVfpOp_Vcvt_Rule_298_A1_P584_instance_)
  {}
};

// opc2(19:16)=1000 & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: 'Vcvt_Vcvtr_Rule_295_A1_P578'}
class CondVfpOpTester_opc2_19To16Is1000_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578
    : public CondVfpOpTesteropc2_19To16Is1000_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc2_19To16Is1000_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578()
    : CondVfpOpTesteropc2_19To16Is1000_opc3_7To6Isx1(
      state_.CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578_instance_)
  {}
};

// opc2(19:16)=101x & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: 'Vcvt_Rule_297_A1_P582'}
class CondVfpOpTester_opc2_19To16Is101x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582
    : public CondVfpOpTesteropc2_19To16Is101x_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc2_19To16Is101x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582()
    : CondVfpOpTesteropc2_19To16Is101x_opc3_7To6Isx1(
      state_.CondVfpOp_Vcvt_Rule_297_A1_P582_instance_)
  {}
};

// opc2(19:16)=110x & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: 'Vcvt_Vcvtr_Rule_295_A1_P578'}
class CondVfpOpTester_opc2_19To16Is110x_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578
    : public CondVfpOpTesteropc2_19To16Is110x_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc2_19To16Is110x_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578()
    : CondVfpOpTesteropc2_19To16Is110x_opc3_7To6Isx1(
      state_.CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578_instance_)
  {}
};

// opc2(19:16)=111x & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: 'Vcvt_Rule_297_A1_P582'}
class CondVfpOpTester_opc2_19To16Is111x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582
    : public CondVfpOpTesteropc2_19To16Is111x_opc3_7To6Isx1 {
 public:
  CondVfpOpTester_opc2_19To16Is111x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582()
    : CondVfpOpTesteropc2_19To16Is111x_opc3_7To6Isx1(
      state_.CondVfpOp_Vcvt_Rule_297_A1_P582_instance_)
  {}
};

// op1(22:20)=000 & op2(7:5)=xx0
//    = Binary3RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     rule: 'Pkh_Rule_116_A1_P234',
//     safety: ['RegsNotPc']}
class Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Isxx0RegsNotPc_Pkh_Rule_116_A1_P234
    : public Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Isxx0RegsNotPc {
 public:
  Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Isxx0RegsNotPc_Pkh_Rule_116_A1_P234()
    : Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Isxx0RegsNotPc(
      state_.Binary3RegisterImmedShiftedOpRegsNotPc_Pkh_Rule_116_A1_P234_instance_)
  {}
};

// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     rule: 'Sxtab16_Rule_221_A1_P436',
//     safety: ['RegsNotPc']}
class Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Sxtab16_Rule_221_A1_P436
    : public Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Is011_A_19To16IsNot1111RegsNotPc {
 public:
  Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Sxtab16_Rule_221_A1_P436()
    : Binary3RegisterImmedShiftedOpTesterop1_22To20Is000_op2_7To5Is011_A_19To16IsNot1111RegsNotPc(
      state_.Binary3RegisterImmedShiftedOpRegsNotPc_Sxtab16_Rule_221_A1_P436_instance_)
  {}
};

// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     rule: 'Sxtb16_Rule_224_A1_P442',
//     safety: ['RegsNotPc']}
class Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_A_19To16Is1111RegsNotPc_Sxtb16_Rule_224_A1_P442
    : public Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is000_op2_7To5Is011_A_19To16Is1111RegsNotPc {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_A_19To16Is1111RegsNotPc_Sxtb16_Rule_224_A1_P442()
    : Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is000_op2_7To5Is011_A_19To16Is1111RegsNotPc(
      state_.Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb16_Rule_224_A1_P442_instance_)
  {}
};

// op1(22:20)=000 & op2(7:5)=101
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Sel_Rule_156_A1_P312',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is000_op2_7To5Is101RegsNotPc_Sel_Rule_156_A1_P312
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is000_op2_7To5Is101RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is000_op2_7To5Is101RegsNotPc_Sel_Rule_156_A1_P312()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is000_op2_7To5Is101RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sel_Rule_156_A1_P312_instance_)
  {}
};

// op1(22:20)=01x & op2(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     rule: 'Ssat_Rule_183_A1_P362',
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is01x_op2_7To5Isxx0RegsNotPc_Ssat_Rule_183_A1_P362
    : public Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is01x_op2_7To5Isxx0RegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is01x_op2_7To5Isxx0RegsNotPc_Ssat_Rule_183_A1_P362()
    : Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is01x_op2_7To5Isxx0RegsNotPc(
      state_.Unary2RegisterSatImmedShiftedOp_Ssat_Rule_183_A1_P362_instance_)
  {}
};

// op1(22:20)=010 & op2(7:5)=001
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     rule: 'Ssat16_Rule_184_A1_P364',
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is010_op2_7To5Is001RegsNotPc_Ssat16_Rule_184_A1_P364
    : public Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is010_op2_7To5Is001RegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is010_op2_7To5Is001RegsNotPc_Ssat16_Rule_184_A1_P364()
    : Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is010_op2_7To5Is001RegsNotPc(
      state_.Unary2RegisterSatImmedShiftedOp_Ssat16_Rule_184_A1_P364_instance_)
  {}
};

// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Sxtab_Rule_220_A1_P434',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is010_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Sxtab_Rule_220_A1_P434
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is010_op2_7To5Is011_A_19To16IsNot1111RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is010_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Sxtab_Rule_220_A1_P434()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is010_op2_7To5Is011_A_19To16IsNot1111RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sxtab_Rule_220_A1_P434_instance_)
  {}
};

// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     rule: 'Sxtb_Rule_223_A1_P440',
//     safety: ['RegsNotPc']}
class Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is010_op2_7To5Is011_A_19To16Is1111RegsNotPc_Sxtb_Rule_223_A1_P440
    : public Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is010_op2_7To5Is011_A_19To16Is1111RegsNotPc {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is010_op2_7To5Is011_A_19To16Is1111RegsNotPc_Sxtb_Rule_223_A1_P440()
    : Unary2RegisterImmedShiftedOpRegsNotPcTesterop1_22To20Is010_op2_7To5Is011_A_19To16Is1111RegsNotPc(
      state_.Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb_Rule_223_A1_P440_instance_)
  {}
};

// op1(22:20)=011 & op2(7:5)=001
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: 'Rev_Rule_135_A1_P272',
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is001RegsNotPc_Rev_Rule_135_A1_P272
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is001RegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is001RegsNotPc_Rev_Rule_135_A1_P272()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is001RegsNotPc(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev_Rule_135_A1_P272_instance_)
  {}
};

// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Sxtah_Rule_222_A1_P438',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Sxtah_Rule_222_A1_P438
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is011_op2_7To5Is011_A_19To16IsNot1111RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Sxtah_Rule_222_A1_P438()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is011_op2_7To5Is011_A_19To16IsNot1111RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sxtah_Rule_222_A1_P438_instance_)
  {}
};

// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: 'Sxth_Rule_225_A1_P444',
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_A_19To16Is1111RegsNotPc_Sxth_Rule_225_A1_P444
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is011_A_19To16Is1111RegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_A_19To16Is1111RegsNotPc_Sxth_Rule_225_A1_P444()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is011_A_19To16Is1111RegsNotPc(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Sxth_Rule_225_A1_P444_instance_)
  {}
};

// op1(22:20)=011 & op2(7:5)=101
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: 'Rev16_Rule_136_A1_P274',
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is101RegsNotPc_Rev16_Rule_136_A1_P274
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is101RegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is101RegsNotPc_Rev16_Rule_136_A1_P274()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is011_op2_7To5Is101RegsNotPc(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev16_Rule_136_A1_P274_instance_)
  {}
};

// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Uxtab16_Rule_262_A1_P516',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is100_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Uxtab16_Rule_262_A1_P516
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is100_op2_7To5Is011_A_19To16IsNot1111RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is100_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Uxtab16_Rule_262_A1_P516()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is100_op2_7To5Is011_A_19To16IsNot1111RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uxtab16_Rule_262_A1_P516_instance_)
  {}
};

// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: 'Uxtb16_Rule_264_A1_P522',
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is100_op2_7To5Is011_A_19To16Is1111RegsNotPc_Uxtb16_Rule_264_A1_P522
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is100_op2_7To5Is011_A_19To16Is1111RegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is100_op2_7To5Is011_A_19To16Is1111RegsNotPc_Uxtb16_Rule_264_A1_P522()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is100_op2_7To5Is011_A_19To16Is1111RegsNotPc(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb16_Rule_264_A1_P522_instance_)
  {}
};

// op1(22:20)=11x & op2(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     rule: 'Usat_Rule_255_A1_P504',
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is11x_op2_7To5Isxx0RegsNotPc_Usat_Rule_255_A1_P504
    : public Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is11x_op2_7To5Isxx0RegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is11x_op2_7To5Isxx0RegsNotPc_Usat_Rule_255_A1_P504()
    : Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is11x_op2_7To5Isxx0RegsNotPc(
      state_.Unary2RegisterSatImmedShiftedOp_Usat_Rule_255_A1_P504_instance_)
  {}
};

// op1(22:20)=110 & op2(7:5)=001
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     rule: 'Usat16_Rule_256_A1_P506',
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is110_op2_7To5Is001RegsNotPc_Usat16_Rule_256_A1_P506
    : public Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is110_op2_7To5Is001RegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is110_op2_7To5Is001RegsNotPc_Usat16_Rule_256_A1_P506()
    : Unary2RegisterSatImmedShiftedOpTesterop1_22To20Is110_op2_7To5Is001RegsNotPc(
      state_.Unary2RegisterSatImmedShiftedOp_Usat16_Rule_256_A1_P506_instance_)
  {}
};

// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Uxtab_Rule_260_A1_P514',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is110_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Uxtab_Rule_260_A1_P514
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is110_op2_7To5Is011_A_19To16IsNot1111RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is110_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Uxtab_Rule_260_A1_P514()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is110_op2_7To5Is011_A_19To16IsNot1111RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uxtab_Rule_260_A1_P514_instance_)
  {}
};

// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: 'Uxtb_Rule_263_A1_P520',
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is110_op2_7To5Is011_A_19To16Is1111RegsNotPc_Uxtb_Rule_263_A1_P520
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is110_op2_7To5Is011_A_19To16Is1111RegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is110_op2_7To5Is011_A_19To16Is1111RegsNotPc_Uxtb_Rule_263_A1_P520()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is110_op2_7To5Is011_A_19To16Is1111RegsNotPc(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb_Rule_263_A1_P520_instance_)
  {}
};

// op1(22:20)=111 & op2(7:5)=001
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: 'Rbit_Rule_134_A1_P270',
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is001RegsNotPc_Rbit_Rule_134_A1_P270
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is001RegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is001RegsNotPc_Rbit_Rule_134_A1_P270()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is001RegsNotPc(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Rbit_Rule_134_A1_P270_instance_)
  {}
};

// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Uxtah_Rule_262_A1_P518',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Uxtah_Rule_262_A1_P518
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is111_op2_7To5Is011_A_19To16IsNot1111RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Uxtah_Rule_262_A1_P518()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_22To20Is111_op2_7To5Is011_A_19To16IsNot1111RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uxtah_Rule_262_A1_P518_instance_)
  {}
};

// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: 'Uxth_Rule_265_A1_P524',
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is011_A_19To16Is1111RegsNotPc_Uxth_Rule_265_A1_P524
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is011_A_19To16Is1111RegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is011_A_19To16Is1111RegsNotPc_Uxth_Rule_265_A1_P524()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is011_A_19To16Is1111RegsNotPc(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxth_Rule_265_A1_P524_instance_)
  {}
};

// op1(22:20)=111 & op2(7:5)=101
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: 'Revsh_Rule_137_A1_P276',
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is101RegsNotPc_Revsh_Rule_137_A1_P276
    : public Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is101RegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is101RegsNotPc_Revsh_Rule_137_A1_P276()
    : Unary2RegisterOpNotRmIsPcTesterop1_22To20Is111_op2_7To5Is101RegsNotPc(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Revsh_Rule_137_A1_P276_instance_)
  {}
};

// op1(21:20)=01 & op2(7:5)=000
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Sadd16_Rule_148_A1_P296',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is000RegsNotPc_Sadd16_Rule_148_A1_P296
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is000RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is000RegsNotPc_Sadd16_Rule_148_A1_P296()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is000RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sadd16_Rule_148_A1_P296_instance_)
  {}
};

// op1(21:20)=01 & op2(7:5)=001
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Sasx_Rule_150_A1_P300',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is001RegsNotPc_Sasx_Rule_150_A1_P300
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is001RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is001RegsNotPc_Sasx_Rule_150_A1_P300()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is001RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sasx_Rule_150_A1_P300_instance_)
  {}
};

// op1(21:20)=01 & op2(7:5)=010
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Ssax_Rule_185_A1_P366',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is010RegsNotPc_Ssax_Rule_185_A1_P366
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is010RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is010RegsNotPc_Ssax_Rule_185_A1_P366()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is010RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Ssax_Rule_185_A1_P366_instance_)
  {}
};

// op1(21:20)=01 & op2(7:5)=011
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Ssub16_Rule_186_A1_P368',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is011RegsNotPc_Ssub16_Rule_186_A1_P368
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is011RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is011RegsNotPc_Ssub16_Rule_186_A1_P368()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is011RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Ssub16_Rule_186_A1_P368_instance_)
  {}
};

// op1(21:20)=01 & op2(7:5)=100
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Sadd8_Rule_149_A1_P298',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is100RegsNotPc_Sadd8_Rule_149_A1_P298
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is100RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is100RegsNotPc_Sadd8_Rule_149_A1_P298()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is100RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sadd8_Rule_149_A1_P298_instance_)
  {}
};

// op1(21:20)=01 & op2(7:5)=111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Ssub8_Rule_187_A1_P370',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is111RegsNotPc_Ssub8_Rule_187_A1_P370
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is111RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is111RegsNotPc_Ssub8_Rule_187_A1_P370()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is01_op2_7To5Is111RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Ssub8_Rule_187_A1_P370_instance_)
  {}
};

// op1(21:20)=10 & op2(7:5)=000
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Qadd16_Rule_125_A1_P252',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is000RegsNotPc_Qadd16_Rule_125_A1_P252
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is000RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is000RegsNotPc_Qadd16_Rule_125_A1_P252()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is000RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qadd16_Rule_125_A1_P252_instance_)
  {}
};

// op1(21:20)=10 & op2(7:5)=001
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Qasx_Rule_127_A1_P256',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is001RegsNotPc_Qasx_Rule_127_A1_P256
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is001RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is001RegsNotPc_Qasx_Rule_127_A1_P256()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is001RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qasx_Rule_127_A1_P256_instance_)
  {}
};

// op1(21:20)=10 & op2(7:5)=010
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Qsax_Rule_130_A1_P262',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is010RegsNotPc_Qsax_Rule_130_A1_P262
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is010RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is010RegsNotPc_Qsax_Rule_130_A1_P262()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is010RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qsax_Rule_130_A1_P262_instance_)
  {}
};

// op1(21:20)=10 & op2(7:5)=011
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Qsub16_Rule_132_A1_P266',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is011RegsNotPc_Qsub16_Rule_132_A1_P266
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is011RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is011RegsNotPc_Qsub16_Rule_132_A1_P266()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is011RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qsub16_Rule_132_A1_P266_instance_)
  {}
};

// op1(21:20)=10 & op2(7:5)=100
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Qadd8_Rule_126_A1_P254',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is100RegsNotPc_Qadd8_Rule_126_A1_P254
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is100RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is100RegsNotPc_Qadd8_Rule_126_A1_P254()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is100RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qadd8_Rule_126_A1_P254_instance_)
  {}
};

// op1(21:20)=10 & op2(7:5)=111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Qsub8_Rule_133_A1_P268',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is111RegsNotPc_Qsub8_Rule_133_A1_P268
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is111RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is111RegsNotPc_Qsub8_Rule_133_A1_P268()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is10_op2_7To5Is111RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qsub8_Rule_133_A1_P268_instance_)
  {}
};

// op1(21:20)=11 & op2(7:5)=000
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Shadd16_Rule_159_A1_P318',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is000RegsNotPc_Shadd16_Rule_159_A1_P318
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is000RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is000RegsNotPc_Shadd16_Rule_159_A1_P318()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is000RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318_instance_)
  {}
};

// op1(21:20)=11 & op2(7:5)=001
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Shasx_Rule_161_A1_P322',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is001RegsNotPc_Shasx_Rule_161_A1_P322
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is001RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is001RegsNotPc_Shasx_Rule_161_A1_P322()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is001RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322_instance_)
  {}
};

// op1(21:20)=11 & op2(7:5)=010
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Shsax_Rule_162_A1_P324',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is010RegsNotPc_Shsax_Rule_162_A1_P324
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is010RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is010RegsNotPc_Shsax_Rule_162_A1_P324()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is010RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324_instance_)
  {}
};

// op1(21:20)=11 & op2(7:5)=011
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Shsub16_Rule_163_A1_P326',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is011RegsNotPc_Shsub16_Rule_163_A1_P326
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is011RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is011RegsNotPc_Shsub16_Rule_163_A1_P326()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is011RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326_instance_)
  {}
};

// op1(21:20)=11 & op2(7:5)=100
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Shadd8_Rule_160_A1_P320',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is100RegsNotPc_Shadd8_Rule_160_A1_P320
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is100RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is100RegsNotPc_Shadd8_Rule_160_A1_P320()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is100RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320_instance_)
  {}
};

// op1(21:20)=11 & op2(7:5)=111
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Shsub8_Rule_164_A1_P328',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is111RegsNotPc_Shsub8_Rule_164_A1_P328
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is111RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is111RegsNotPc_Shsub8_Rule_164_A1_P328()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop1_21To20Is11_op2_7To5Is111RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328_instance_)
  {}
};

// op(22:21)=00
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Qadd_Rule_124_A1_P250',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is00RegsNotPc_Qadd_Rule_124_A1_P250
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is00RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is00RegsNotPc_Qadd_Rule_124_A1_P250()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is00RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qadd_Rule_124_A1_P250_instance_)
  {}
};

// op(22:21)=01
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Qsub_Rule_131_A1_P264',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is01RegsNotPc_Qsub_Rule_131_A1_P264
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is01RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is01RegsNotPc_Qsub_Rule_131_A1_P264()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is01RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qsub_Rule_131_A1_P264_instance_)
  {}
};

// op(22:21)=10
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Qdadd_Rule_128_A1_P258',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is10RegsNotPc_Qdadd_Rule_128_A1_P258
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is10RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is10RegsNotPc_Qdadd_Rule_128_A1_P258()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is10RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qdadd_Rule_128_A1_P258_instance_)
  {}
};

// op(22:21)=11
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: 'Qdsub_Rule_129_A1_P260',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is11RegsNotPc_Qdsub_Rule_129_A1_P260
    : public Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is11RegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is11RegsNotPc_Qdsub_Rule_129_A1_P260()
    : Binary3RegisterOpAltBNoCondUpdatesTesterop_22To21Is11RegsNotPc(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qdsub_Rule_129_A1_P260_instance_)
  {}
};

// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     rule: 'Smlad_Rule_167_P332',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc_Smlad_Rule_167_P332
    : public Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc {
 public:
  Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc_Smlad_Rule_167_P332()
    : Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc(
      state_.Binary4RegisterDualOp_Smlad_Rule_167_P332_instance_)
  {}
};

// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: 'Smuad_Rule_177_P352',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smuad_Rule_177_P352
    : public Binary3RegisterOpAltATesterop1_22To20Is000_op2_7To5Is00x_A_15To12Is1111RegsNotPc {
 public:
  Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smuad_Rule_177_P352()
    : Binary3RegisterOpAltATesterop1_22To20Is000_op2_7To5Is00x_A_15To12Is1111RegsNotPc(
      state_.Binary3RegisterOpAltA_Smuad_Rule_177_P352_instance_)
  {}
};

// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     rule: 'Smlsd_Rule_172_P342',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is01x_A_15To12IsNot1111RegsNotPc_Smlsd_Rule_172_P342
    : public Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is01x_A_15To12IsNot1111RegsNotPc {
 public:
  Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is01x_A_15To12IsNot1111RegsNotPc_Smlsd_Rule_172_P342()
    : Binary4RegisterDualOpTesterop1_22To20Is000_op2_7To5Is01x_A_15To12IsNot1111RegsNotPc(
      state_.Binary4RegisterDualOp_Smlsd_Rule_172_P342_instance_)
  {}
};

// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: 'Smusd_Rule_181_P360',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is01x_A_15To12Is1111RegsNotPc_Smusd_Rule_181_P360
    : public Binary3RegisterOpAltATesterop1_22To20Is000_op2_7To5Is01x_A_15To12Is1111RegsNotPc {
 public:
  Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is01x_A_15To12Is1111RegsNotPc_Smusd_Rule_181_P360()
    : Binary3RegisterOpAltATesterop1_22To20Is000_op2_7To5Is01x_A_15To12Is1111RegsNotPc(
      state_.Binary3RegisterOpAltA_Smusd_Rule_181_P360_instance_)
  {}
};

// op1(22:20)=001 & op2(7:5)=000
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: 'Sdiv_Rule_A1'}
class Binary3RegisterOpAltATester_op1_22To20Is001_op2_7To5Is000_Sdiv_Rule_A1
    : public Binary3RegisterOpAltATesterop1_22To20Is001_op2_7To5Is000 {
 public:
  Binary3RegisterOpAltATester_op1_22To20Is001_op2_7To5Is000_Sdiv_Rule_A1()
    : Binary3RegisterOpAltATesterop1_22To20Is001_op2_7To5Is000(
      state_.Binary3RegisterOpAltA_Sdiv_Rule_A1_instance_)
  {}
};

// op1(22:20)=011 & op2(7:5)=000
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: 'Udiv_Rule_A1'}
class Binary3RegisterOpAltATester_op1_22To20Is011_op2_7To5Is000_Udiv_Rule_A1
    : public Binary3RegisterOpAltATesterop1_22To20Is011_op2_7To5Is000 {
 public:
  Binary3RegisterOpAltATester_op1_22To20Is011_op2_7To5Is000_Udiv_Rule_A1()
    : Binary3RegisterOpAltATesterop1_22To20Is011_op2_7To5Is000(
      state_.Binary3RegisterOpAltA_Udiv_Rule_A1_instance_)
  {}
};

// op1(22:20)=100 & op2(7:5)=00x
//    = Binary4RegisterDualResult {constraints: ,
//     rule: 'Smlald_Rule_170_P336',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is00xRegsNotPc_Smlald_Rule_170_P336
    : public Binary4RegisterDualResultTesterop1_22To20Is100_op2_7To5Is00xRegsNotPc {
 public:
  Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is00xRegsNotPc_Smlald_Rule_170_P336()
    : Binary4RegisterDualResultTesterop1_22To20Is100_op2_7To5Is00xRegsNotPc(
      state_.Binary4RegisterDualResult_Smlald_Rule_170_P336_instance_)
  {}
};

// op1(22:20)=100 & op2(7:5)=01x
//    = Binary4RegisterDualResult {constraints: ,
//     rule: 'Smlsld_Rule_173_P344',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is01xRegsNotPc_Smlsld_Rule_173_P344
    : public Binary4RegisterDualResultTesterop1_22To20Is100_op2_7To5Is01xRegsNotPc {
 public:
  Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is01xRegsNotPc_Smlsld_Rule_173_P344()
    : Binary4RegisterDualResultTesterop1_22To20Is100_op2_7To5Is01xRegsNotPc(
      state_.Binary4RegisterDualResult_Smlsld_Rule_173_P344_instance_)
  {}
};

// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     rule: 'Smmla_Rule_174_P346',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc_Smmla_Rule_174_P346
    : public Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc {
 public:
  Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc_Smmla_Rule_174_P346()
    : Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc(
      state_.Binary4RegisterDualOp_Smmla_Rule_174_P346_instance_)
  {}
};

// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: 'Smmul_Rule_176_P350',
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_op1_22To20Is101_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smmul_Rule_176_P350
    : public Binary3RegisterOpAltATesterop1_22To20Is101_op2_7To5Is00x_A_15To12Is1111RegsNotPc {
 public:
  Binary3RegisterOpAltATester_op1_22To20Is101_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smmul_Rule_176_P350()
    : Binary3RegisterOpAltATesterop1_22To20Is101_op2_7To5Is00x_A_15To12Is1111RegsNotPc(
      state_.Binary3RegisterOpAltA_Smmul_Rule_176_P350_instance_)
  {}
};

// op1(22:20)=101 & op2(7:5)=11x
//    = Binary4RegisterDualOp {constraints: ,
//     rule: 'Smmls_Rule_175_P348',
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is11xRegsNotPc_Smmls_Rule_175_P348
    : public Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is11xRegsNotPc {
 public:
  Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is11xRegsNotPc_Smmls_Rule_175_P348()
    : Binary4RegisterDualOpTesterop1_22To20Is101_op2_7To5Is11xRegsNotPc(
      state_.Binary4RegisterDualOp_Smmls_Rule_175_P348_instance_)
  {}
};

// op(23:20)=0x00
//    = Deprecated {constraints: ,
//     rule: 'Swp_Swpb_Rule_A1'}
class DeprecatedTester_op_23To20Is0x00_Swp_Swpb_Rule_A1
    : public UnsafeCondDecoderTesterop_23To20Is0x00 {
 public:
  DeprecatedTester_op_23To20Is0x00_Swp_Swpb_Rule_A1()
    : UnsafeCondDecoderTesterop_23To20Is0x00(
      state_.Deprecated_Swp_Swpb_Rule_A1_instance_)
  {}
};

// op(23:20)=1000
//    = StoreExclusive3RegisterOp {constraints: ,
//     rule: 'Strex_Rule_202_A1_P400'}
class StoreExclusive3RegisterOpTester_op_23To20Is1000_Strex_Rule_202_A1_P400
    : public StoreExclusive3RegisterOpTesterop_23To20Is1000 {
 public:
  StoreExclusive3RegisterOpTester_op_23To20Is1000_Strex_Rule_202_A1_P400()
    : StoreExclusive3RegisterOpTesterop_23To20Is1000(
      state_.StoreExclusive3RegisterOp_Strex_Rule_202_A1_P400_instance_)
  {}
};

// op(23:20)=1001
//    = LoadExclusive2RegisterOp {constraints: ,
//     rule: 'Ldrex_Rule_69_A1_P142'}
class LoadExclusive2RegisterOpTester_op_23To20Is1001_Ldrex_Rule_69_A1_P142
    : public LoadExclusive2RegisterOpTesterop_23To20Is1001 {
 public:
  LoadExclusive2RegisterOpTester_op_23To20Is1001_Ldrex_Rule_69_A1_P142()
    : LoadExclusive2RegisterOpTesterop_23To20Is1001(
      state_.LoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142_instance_)
  {}
};

// op(23:20)=1010
//    = StoreExclusive3RegisterDoubleOp {constraints: ,
//     rule: 'Strexd_Rule_204_A1_P404'}
class StoreExclusive3RegisterDoubleOpTester_op_23To20Is1010_Strexd_Rule_204_A1_P404
    : public StoreExclusive3RegisterDoubleOpTesterop_23To20Is1010 {
 public:
  StoreExclusive3RegisterDoubleOpTester_op_23To20Is1010_Strexd_Rule_204_A1_P404()
    : StoreExclusive3RegisterDoubleOpTesterop_23To20Is1010(
      state_.StoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404_instance_)
  {}
};

// op(23:20)=1011
//    = LoadExclusive2RegisterDoubleOp {constraints: ,
//     rule: 'Ldrexd_Rule_71_A1_P146'}
class LoadExclusive2RegisterDoubleOpTester_op_23To20Is1011_Ldrexd_Rule_71_A1_P146
    : public LoadExclusive2RegisterDoubleOpTesterop_23To20Is1011 {
 public:
  LoadExclusive2RegisterDoubleOpTester_op_23To20Is1011_Ldrexd_Rule_71_A1_P146()
    : LoadExclusive2RegisterDoubleOpTesterop_23To20Is1011(
      state_.LoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146_instance_)
  {}
};

// op(23:20)=1100
//    = StoreExclusive3RegisterOp {constraints: ,
//     rule: 'Strexb_Rule_203_A1_P402'}
class StoreExclusive3RegisterOpTester_op_23To20Is1100_Strexb_Rule_203_A1_P402
    : public StoreExclusive3RegisterOpTesterop_23To20Is1100 {
 public:
  StoreExclusive3RegisterOpTester_op_23To20Is1100_Strexb_Rule_203_A1_P402()
    : StoreExclusive3RegisterOpTesterop_23To20Is1100(
      state_.StoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402_instance_)
  {}
};

// op(23:20)=1101
//    = LoadExclusive2RegisterOp {constraints: ,
//     rule: 'Ldrexb_Rule_70_A1_P144'}
class LoadExclusive2RegisterOpTester_op_23To20Is1101_Ldrexb_Rule_70_A1_P144
    : public LoadExclusive2RegisterOpTesterop_23To20Is1101 {
 public:
  LoadExclusive2RegisterOpTester_op_23To20Is1101_Ldrexb_Rule_70_A1_P144()
    : LoadExclusive2RegisterOpTesterop_23To20Is1101(
      state_.LoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144_instance_)
  {}
};

// op(23:20)=1110
//    = StoreExclusive3RegisterOp {constraints: ,
//     rule: 'Strexh_Rule_205_A1_P406'}
class StoreExclusive3RegisterOpTester_op_23To20Is1110_Strexh_Rule_205_A1_P406
    : public StoreExclusive3RegisterOpTesterop_23To20Is1110 {
 public:
  StoreExclusive3RegisterOpTester_op_23To20Is1110_Strexh_Rule_205_A1_P406()
    : StoreExclusive3RegisterOpTesterop_23To20Is1110(
      state_.StoreExclusive3RegisterOp_Strexh_Rule_205_A1_P406_instance_)
  {}
};

// op(23:20)=1111
//    = LoadExclusive2RegisterOp {constraints: ,
//     rule: 'Ldrexh_Rule_72_A1_P148'}
class LoadExclusive2RegisterOpTester_op_23To20Is1111_Ldrexh_Rule_72_A1_P148
    : public LoadExclusive2RegisterOpTesterop_23To20Is1111 {
 public:
  LoadExclusive2RegisterOpTester_op_23To20Is1111_Ldrexh_Rule_72_A1_P148()
    : LoadExclusive2RegisterOpTesterop_23To20Is1111(
      state_.LoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148_instance_)
  {}
};

// L(20)=0 & C(8)=0 & A(23:21)=000
//    = MoveVfpRegisterOp {constraints: ,
//     rule: 'Vmov_Rule_330_A1_P648'}
class MoveVfpRegisterOpTester_L_20Is0_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648
    : public MoveVfpRegisterOpTesterL_20Is0_C_8Is0_A_23To21Is000 {
 public:
  MoveVfpRegisterOpTester_L_20Is0_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648()
    : MoveVfpRegisterOpTesterL_20Is0_C_8Is0_A_23To21Is000(
      state_.MoveVfpRegisterOp_Vmov_Rule_330_A1_P648_instance_)
  {}
};

// L(20)=0 & C(8)=0 & A(23:21)=111
//    = VfpUsesRegOp {constraints: ,
//     rule: 'Vmsr_Rule_336_A1_P660'}
class VfpUsesRegOpTester_L_20Is0_C_8Is0_A_23To21Is111_Vmsr_Rule_336_A1_P660
    : public VfpUsesRegOpTesterL_20Is0_C_8Is0_A_23To21Is111 {
 public:
  VfpUsesRegOpTester_L_20Is0_C_8Is0_A_23To21Is111_Vmsr_Rule_336_A1_P660()
    : VfpUsesRegOpTesterL_20Is0_C_8Is0_A_23To21Is111(
      state_.VfpUsesRegOp_Vmsr_Rule_336_A1_P660_instance_)
  {}
};

// L(20)=0 & C(8)=1 & A(23:21)=0xx
//    = MoveVfpRegisterOpWithTypeSel {constraints: ,
//     rule: 'Vmov_Rule_328_A1_P644'}
class MoveVfpRegisterOpWithTypeSelTester_L_20Is0_C_8Is1_A_23To21Is0xx_Vmov_Rule_328_A1_P644
    : public MoveVfpRegisterOpWithTypeSelTesterL_20Is0_C_8Is1_A_23To21Is0xx {
 public:
  MoveVfpRegisterOpWithTypeSelTester_L_20Is0_C_8Is1_A_23To21Is0xx_Vmov_Rule_328_A1_P644()
    : MoveVfpRegisterOpWithTypeSelTesterL_20Is0_C_8Is1_A_23To21Is0xx(
      state_.MoveVfpRegisterOpWithTypeSel_Vmov_Rule_328_A1_P644_instance_)
  {}
};

// L(20)=0 & C(8)=1 & A(23:21)=1xx & B(6:5)=0x
//    = DuplicateToAdvSIMDRegisters {constraints: ,
//     rule: 'Vdup_Rule_303_A1_P594'}
class DuplicateToAdvSIMDRegistersTester_L_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x_Vdup_Rule_303_A1_P594
    : public DuplicateToAdvSIMDRegistersTesterL_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x {
 public:
  DuplicateToAdvSIMDRegistersTester_L_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x_Vdup_Rule_303_A1_P594()
    : DuplicateToAdvSIMDRegistersTesterL_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x(
      state_.DuplicateToAdvSIMDRegisters_Vdup_Rule_303_A1_P594_instance_)
  {}
};

// L(20)=1 & C(8)=0 & A(23:21)=000
//    = MoveVfpRegisterOp {constraints: ,
//     rule: 'Vmov_Rule_330_A1_P648'}
class MoveVfpRegisterOpTester_L_20Is1_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648
    : public MoveVfpRegisterOpTesterL_20Is1_C_8Is0_A_23To21Is000 {
 public:
  MoveVfpRegisterOpTester_L_20Is1_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648()
    : MoveVfpRegisterOpTesterL_20Is1_C_8Is0_A_23To21Is000(
      state_.MoveVfpRegisterOp_Vmov_Rule_330_A1_P648_instance_)
  {}
};

// L(20)=1 & C(8)=0 & A(23:21)=111
//    = VfpMrsOp {constraints: ,
//     rule: 'Vmrs_Rule_335_A1_P658'}
class VfpMrsOpTester_L_20Is1_C_8Is0_A_23To21Is111_Vmrs_Rule_335_A1_P658
    : public VfpMrsOpTesterL_20Is1_C_8Is0_A_23To21Is111 {
 public:
  VfpMrsOpTester_L_20Is1_C_8Is0_A_23To21Is111_Vmrs_Rule_335_A1_P658()
    : VfpMrsOpTesterL_20Is1_C_8Is0_A_23To21Is111(
      state_.VfpMrsOp_Vmrs_Rule_335_A1_P658_instance_)
  {}
};

// L(20)=1 & C(8)=1
//    = MoveVfpRegisterOpWithTypeSel {constraints: ,
//     rule: 'Vmov_Rule_329_A1_P646'}
class MoveVfpRegisterOpWithTypeSelTester_L_20Is1_C_8Is1_Vmov_Rule_329_A1_P646
    : public MoveVfpRegisterOpWithTypeSelTesterL_20Is1_C_8Is1 {
 public:
  MoveVfpRegisterOpWithTypeSelTester_L_20Is1_C_8Is1_Vmov_Rule_329_A1_P646()
    : MoveVfpRegisterOpWithTypeSelTesterL_20Is1_C_8Is1(
      state_.MoveVfpRegisterOpWithTypeSel_Vmov_Rule_329_A1_P646_instance_)
  {}
};

// C(8)=0 & op(7:4)=00x1
//    = MoveDoubleVfpRegisterOp {constraints: ,
//     rule: 'Vmov_two_S_Rule_A1'}
class MoveDoubleVfpRegisterOpTester_C_8Is0_op_7To4Is00x1_Vmov_two_S_Rule_A1
    : public MoveDoubleVfpRegisterOpTesterC_8Is0_op_7To4Is00x1 {
 public:
  MoveDoubleVfpRegisterOpTester_C_8Is0_op_7To4Is00x1_Vmov_two_S_Rule_A1()
    : MoveDoubleVfpRegisterOpTesterC_8Is0_op_7To4Is00x1(
      state_.MoveDoubleVfpRegisterOp_Vmov_two_S_Rule_A1_instance_)
  {}
};

// C(8)=1 & op(7:4)=00x1
//    = MoveDoubleVfpRegisterOp {constraints: ,
//     rule: 'Vmov_one_D_Rule_A1'}
class MoveDoubleVfpRegisterOpTester_C_8Is1_op_7To4Is00x1_Vmov_one_D_Rule_A1
    : public MoveDoubleVfpRegisterOpTesterC_8Is1_op_7To4Is00x1 {
 public:
  MoveDoubleVfpRegisterOpTester_C_8Is1_op_7To4Is00x1_Vmov_one_D_Rule_A1()
    : MoveDoubleVfpRegisterOpTesterC_8Is1_op_7To4Is00x1(
      state_.MoveDoubleVfpRegisterOp_Vmov_one_D_Rule_A1_instance_)
  {}
};

// op1(27:20)=100xx1x0
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Srs_Rule_B6_1_10_A1_B6_20'}
class ForbiddenUncondDecoderTester_op1_27To20Is100xx1x0_Srs_Rule_B6_1_10_A1_B6_20
    : public UnsafeUncondDecoderTesterop1_27To20Is100xx1x0 {
 public:
  ForbiddenUncondDecoderTester_op1_27To20Is100xx1x0_Srs_Rule_B6_1_10_A1_B6_20()
    : UnsafeUncondDecoderTesterop1_27To20Is100xx1x0(
      state_.ForbiddenUncondDecoder_Srs_Rule_B6_1_10_A1_B6_20_instance_)
  {}
};

// op1(27:20)=100xx0x1
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Rfe_Rule_B6_1_10_A1_B6_16'}
class ForbiddenUncondDecoderTester_op1_27To20Is100xx0x1_Rfe_Rule_B6_1_10_A1_B6_16
    : public UnsafeUncondDecoderTesterop1_27To20Is100xx0x1 {
 public:
  ForbiddenUncondDecoderTester_op1_27To20Is100xx0x1_Rfe_Rule_B6_1_10_A1_B6_16()
    : UnsafeUncondDecoderTesterop1_27To20Is100xx0x1(
      state_.ForbiddenUncondDecoder_Rfe_Rule_B6_1_10_A1_B6_16_instance_)
  {}
};

// op1(27:20)=101xxxxx
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Blx_Rule_23_A2_P58'}
class ForbiddenUncondDecoderTester_op1_27To20Is101xxxxx_Blx_Rule_23_A2_P58
    : public UnsafeUncondDecoderTesterop1_27To20Is101xxxxx {
 public:
  ForbiddenUncondDecoderTester_op1_27To20Is101xxxxx_Blx_Rule_23_A2_P58()
    : UnsafeUncondDecoderTesterop1_27To20Is101xxxxx(
      state_.ForbiddenUncondDecoder_Blx_Rule_23_A2_P58_instance_)
  {}
};

// op1(27:20)=110xxxx0 & op1_repeated(27:20)=~11000x00
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Stc2_Rule_188_A2_P372'}
class ForbiddenUncondDecoderTester_op1_27To20Is110xxxx0_op1_repeated_27To20IsNot11000x00_Stc2_Rule_188_A2_P372
    : public UnsafeUncondDecoderTesterop1_27To20Is110xxxx0_op1_repeated_27To20IsNot11000x00 {
 public:
  ForbiddenUncondDecoderTester_op1_27To20Is110xxxx0_op1_repeated_27To20IsNot11000x00_Stc2_Rule_188_A2_P372()
    : UnsafeUncondDecoderTesterop1_27To20Is110xxxx0_op1_repeated_27To20IsNot11000x00(
      state_.ForbiddenUncondDecoder_Stc2_Rule_188_A2_P372_instance_)
  {}
};

// op1(27:20)=110xxxx1 & Rn(19:16)=~1111 & op1_repeated(27:20)=~11000x01
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Ldc2_Rule_51_A2_P106'}
class ForbiddenUncondDecoderTester_op1_27To20Is110xxxx1_Rn_19To16IsNot1111_op1_repeated_27To20IsNot11000x01_Ldc2_Rule_51_A2_P106
    : public UnsafeUncondDecoderTesterop1_27To20Is110xxxx1_Rn_19To16IsNot1111_op1_repeated_27To20IsNot11000x01 {
 public:
  ForbiddenUncondDecoderTester_op1_27To20Is110xxxx1_Rn_19To16IsNot1111_op1_repeated_27To20IsNot11000x01_Ldc2_Rule_51_A2_P106()
    : UnsafeUncondDecoderTesterop1_27To20Is110xxxx1_Rn_19To16IsNot1111_op1_repeated_27To20IsNot11000x01(
      state_.ForbiddenUncondDecoder_Ldc2_Rule_51_A2_P106_instance_)
  {}
};

// op1(27:20)=110xxxx1 & Rn(19:16)=1111 & op1_repeated(27:20)=~11000x01
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Ldc2_Rule_52_A2_P108'}
class ForbiddenUncondDecoderTester_op1_27To20Is110xxxx1_Rn_19To16Is1111_op1_repeated_27To20IsNot11000x01_Ldc2_Rule_52_A2_P108
    : public UnsafeUncondDecoderTesterop1_27To20Is110xxxx1_Rn_19To16Is1111_op1_repeated_27To20IsNot11000x01 {
 public:
  ForbiddenUncondDecoderTester_op1_27To20Is110xxxx1_Rn_19To16Is1111_op1_repeated_27To20IsNot11000x01_Ldc2_Rule_52_A2_P108()
    : UnsafeUncondDecoderTesterop1_27To20Is110xxxx1_Rn_19To16Is1111_op1_repeated_27To20IsNot11000x01(
      state_.ForbiddenUncondDecoder_Ldc2_Rule_52_A2_P108_instance_)
  {}
};

// op1(27:20)=11000100
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Mcrr2_Rule_93_A2_P188'}
class ForbiddenUncondDecoderTester_op1_27To20Is11000100_Mcrr2_Rule_93_A2_P188
    : public UnsafeUncondDecoderTesterop1_27To20Is11000100 {
 public:
  ForbiddenUncondDecoderTester_op1_27To20Is11000100_Mcrr2_Rule_93_A2_P188()
    : UnsafeUncondDecoderTesterop1_27To20Is11000100(
      state_.ForbiddenUncondDecoder_Mcrr2_Rule_93_A2_P188_instance_)
  {}
};

// op1(27:20)=11000101
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Mrrc2_Rule_101_A2_P204'}
class ForbiddenUncondDecoderTester_op1_27To20Is11000101_Mrrc2_Rule_101_A2_P204
    : public UnsafeUncondDecoderTesterop1_27To20Is11000101 {
 public:
  ForbiddenUncondDecoderTester_op1_27To20Is11000101_Mrrc2_Rule_101_A2_P204()
    : UnsafeUncondDecoderTesterop1_27To20Is11000101(
      state_.ForbiddenUncondDecoder_Mrrc2_Rule_101_A2_P204_instance_)
  {}
};

// op1(27:20)=1110xxxx & op(4)=0
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Cdp2_Rule_28_A2_P68'}
class ForbiddenUncondDecoderTester_op1_27To20Is1110xxxx_op_4Is0_Cdp2_Rule_28_A2_P68
    : public UnsafeUncondDecoderTesterop1_27To20Is1110xxxx_op_4Is0 {
 public:
  ForbiddenUncondDecoderTester_op1_27To20Is1110xxxx_op_4Is0_Cdp2_Rule_28_A2_P68()
    : UnsafeUncondDecoderTesterop1_27To20Is1110xxxx_op_4Is0(
      state_.ForbiddenUncondDecoder_Cdp2_Rule_28_A2_P68_instance_)
  {}
};

// op1(27:20)=1110xxx0 & op(4)=1
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Mcr2_Rule_92_A2_P186'}
class ForbiddenUncondDecoderTester_op1_27To20Is1110xxx0_op_4Is1_Mcr2_Rule_92_A2_P186
    : public UnsafeUncondDecoderTesterop1_27To20Is1110xxx0_op_4Is1 {
 public:
  ForbiddenUncondDecoderTester_op1_27To20Is1110xxx0_op_4Is1_Mcr2_Rule_92_A2_P186()
    : UnsafeUncondDecoderTesterop1_27To20Is1110xxx0_op_4Is1(
      state_.ForbiddenUncondDecoder_Mcr2_Rule_92_A2_P186_instance_)
  {}
};

// op1(27:20)=1110xxx1 & op(4)=1
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: 'Mrc2_Rule_100_A2_P202'}
class ForbiddenUncondDecoderTester_op1_27To20Is1110xxx1_op_4Is1_Mrc2_Rule_100_A2_P202
    : public UnsafeUncondDecoderTesterop1_27To20Is1110xxx1_op_4Is1 {
 public:
  ForbiddenUncondDecoderTester_op1_27To20Is1110xxx1_op_4Is1_Mrc2_Rule_100_A2_P202()
    : UnsafeUncondDecoderTesterop1_27To20Is1110xxx1_op_4Is1(
      state_.ForbiddenUncondDecoder_Mrc2_Rule_100_A2_P202_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// op(25:20)=0000x0
//    = StoreRegisterList => StoreRegisterList {constraints: ,
//     pattern: 'cccc100000w0nnnnrrrrrrrrrrrrrrrr',
//     rule: 'Stmda_Stmed_Rule_190_A1_P376'}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_op_25To20Is0000x0_Stmda_Stmed_Rule_190_A1_P376_cccc100000w0nnnnrrrrrrrrrrrrrrrr_Test) {
  StoreRegisterListTester_op_25To20Is0000x0_Stmda_Stmed_Rule_190_A1_P376 tester;
  tester.Test("cccc100000w0nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0000x1
//    = LoadRegisterList => LoadMultiple {constraints: ,
//     pattern: 'cccc100000w1nnnnrrrrrrrrrrrrrrrr',
//     rule: 'Ldmda_Ldmfa_Rule_54_A1_P112'}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_op_25To20Is0000x1_Ldmda_Ldmfa_Rule_54_A1_P112_cccc100000w1nnnnrrrrrrrrrrrrrrrr_Test) {
  LoadRegisterListTester_op_25To20Is0000x1_Ldmda_Ldmfa_Rule_54_A1_P112 baseline_tester;
  NamedLoadMultiple_Ldmda_Ldmfa_Rule_54_A1_P112 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100000w1nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0010x0
//    = StoreRegisterList => StoreRegisterList {constraints: ,
//     pattern: 'cccc100010w0nnnnrrrrrrrrrrrrrrrr',
//     rule: 'Stm_Stmia_Stmea_Rule_189_A1_P374'}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_op_25To20Is0010x0_Stm_Stmia_Stmea_Rule_189_A1_P374_cccc100010w0nnnnrrrrrrrrrrrrrrrr_Test) {
  StoreRegisterListTester_op_25To20Is0010x0_Stm_Stmia_Stmea_Rule_189_A1_P374 tester;
  tester.Test("cccc100010w0nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=001001
//    = LoadRegisterList => LoadMultiple {constraints: ,
//     pattern: 'cccc10001001nnnnrrrrrrrrrrrrrrrr',
//     rule: 'Ldm_Ldmia_Ldmfd_Rule_53_A1_P110'}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_op_25To20Is001001_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110_cccc10001001nnnnrrrrrrrrrrrrrrrr_Test) {
  LoadRegisterListTester_op_25To20Is001001_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110 baseline_tester;
  NamedLoadMultiple_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc10001001nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=001011 & Rn(19:16)=~1101
//    = LoadRegisterList => LoadMultiple {constraints: ,
//     pattern: 'cccc10001011nnnnrrrrrrrrrrrrrrrr',
//     rule: 'Ldm_Ldmia_Ldmfd_Rule_53_A1_P110'}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_op_25To20Is001011_Rn_19To16IsNot1101_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110_cccc10001011nnnnrrrrrrrrrrrrrrrr_Test) {
  LoadRegisterListTester_op_25To20Is001011_Rn_19To16IsNot1101_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110 baseline_tester;
  NamedLoadMultiple_Ldm_Ldmia_Ldmfd_Rule_53_A1_P110 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc10001011nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=001011 & Rn(19:16)=1101
//    = LoadRegisterList => LoadMultiple {constraints: ,
//     pattern: 'cccc10001011nnnnrrrrrrrrrrrrrrrr',
//     rule: 'Pop_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_op_25To20Is001011_Rn_19To16Is1101_Pop_Rule_A1_cccc10001011nnnnrrrrrrrrrrrrrrrr_Test) {
  LoadRegisterListTester_op_25To20Is001011_Rn_19To16Is1101_Pop_Rule_A1 baseline_tester;
  NamedLoadMultiple_Pop_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc10001011nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=010000
//    = StoreRegisterList => StoreRegisterList {constraints: ,
//     pattern: 'cccc10010000nnnnrrrrrrrrrrrrrrrr',
//     rule: 'Stmdb_Stmfd_Rule_191_A1_P378'}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_op_25To20Is010000_Stmdb_Stmfd_Rule_191_A1_P378_cccc10010000nnnnrrrrrrrrrrrrrrrr_Test) {
  StoreRegisterListTester_op_25To20Is010000_Stmdb_Stmfd_Rule_191_A1_P378 tester;
  tester.Test("cccc10010000nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=010010 & Rn(19:16)=~1101
//    = StoreRegisterList => StoreRegisterList {constraints: ,
//     pattern: 'cccc10010010nnnnrrrrrrrrrrrrrrrr',
//     rule: 'Stmdb_Stmfd_Rule_191_A1_P378'}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_op_25To20Is010010_Rn_19To16IsNot1101_Stmdb_Stmfd_Rule_191_A1_P378_cccc10010010nnnnrrrrrrrrrrrrrrrr_Test) {
  StoreRegisterListTester_op_25To20Is010010_Rn_19To16IsNot1101_Stmdb_Stmfd_Rule_191_A1_P378 tester;
  tester.Test("cccc10010010nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=010010 & Rn(19:16)=1101
//    = StoreRegisterList => StoreRegisterList {constraints: ,
//     pattern: 'cccc10010010nnnnrrrrrrrrrrrrrrrr',
//     rule: 'Push_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_op_25To20Is010010_Rn_19To16Is1101_Push_Rule_A1_cccc10010010nnnnrrrrrrrrrrrrrrrr_Test) {
  StoreRegisterListTester_op_25To20Is010010_Rn_19To16Is1101_Push_Rule_A1 tester;
  tester.Test("cccc10010010nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0100x1
//    = LoadRegisterList => LoadMultiple {constraints: ,
//     pattern: 'cccc100100w1nnnnrrrrrrrrrrrrrrrr',
//     rule: 'Ldmdb_Ldmea_Rule_55_A1_P114'}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_op_25To20Is0100x1_Ldmdb_Ldmea_Rule_55_A1_P114_cccc100100w1nnnnrrrrrrrrrrrrrrrr_Test) {
  LoadRegisterListTester_op_25To20Is0100x1_Ldmdb_Ldmea_Rule_55_A1_P114 baseline_tester;
  NamedLoadMultiple_Ldmdb_Ldmea_Rule_55_A1_P114 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100100w1nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0110x0
//    = StoreRegisterList => StoreRegisterList {constraints: ,
//     pattern: 'cccc100110w0nnnnrrrrrrrrrrrrrrrr',
//     rule: 'Stmib_Stmfa_Rule_192_A1_P380'}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_op_25To20Is0110x0_Stmib_Stmfa_Rule_192_A1_P380_cccc100110w0nnnnrrrrrrrrrrrrrrrr_Test) {
  StoreRegisterListTester_op_25To20Is0110x0_Stmib_Stmfa_Rule_192_A1_P380 tester;
  tester.Test("cccc100110w0nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0110x1
//    = LoadRegisterList => LoadMultiple {constraints: ,
//     pattern: 'cccc100110w1nnnnrrrrrrrrrrrrrrrr',
//     rule: 'Ldmib_Ldmed_Rule_56_A1_P116'}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_op_25To20Is0110x1_Ldmib_Ldmed_Rule_56_A1_P116_cccc100110w1nnnnrrrrrrrrrrrrrrrr_Test) {
  LoadRegisterListTester_op_25To20Is0110x1_Ldmib_Ldmed_Rule_56_A1_P116 baseline_tester;
  NamedLoadMultiple_Ldmib_Ldmed_Rule_56_A1_P116 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100110w1nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0xx1x0
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc100pu100nnnnrrrrrrrrrrrrrrrr',
//     rule: 'Stm_Rule_11_B6_A1_P22'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op_25To20Is0xx1x0_Stm_Rule_11_B6_A1_P22_cccc100pu100nnnnrrrrrrrrrrrrrrrr_Test) {
  ForbiddenCondDecoderTester_op_25To20Is0xx1x0_Stm_Rule_11_B6_A1_P22 baseline_tester;
  NamedForbidden_Stm_Rule_11_B6_A1_P22 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100pu100nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0xx1x1 & R(15)=0
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc100pu101nnnn0rrrrrrrrrrrrrrr',
//     rule: 'Ldm_Rule_3_B6_A1_P7'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op_25To20Is0xx1x1_R_15Is0_Ldm_Rule_3_B6_A1_P7_cccc100pu101nnnn0rrrrrrrrrrrrrrr_Test) {
  ForbiddenCondDecoderTester_op_25To20Is0xx1x1_R_15Is0_Ldm_Rule_3_B6_A1_P7 baseline_tester;
  NamedForbidden_Ldm_Rule_3_B6_A1_P7 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100pu101nnnn0rrrrrrrrrrrrrrr");
}

// op(25:20)=0xx1x1 & R(15)=1
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc100pu1w1nnnn1rrrrrrrrrrrrrrr',
//     rule: 'Ldm_Rule_2_B6_A1_P5'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op_25To20Is0xx1x1_R_15Is1_Ldm_Rule_2_B6_A1_P5_cccc100pu1w1nnnn1rrrrrrrrrrrrrrr_Test) {
  ForbiddenCondDecoderTester_op_25To20Is0xx1x1_R_15Is1_Ldm_Rule_2_B6_A1_P5 baseline_tester;
  NamedForbidden_Ldm_Rule_2_B6_A1_P5 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100pu1w1nnnn1rrrrrrrrrrrrrrr");
}

// op(25:20)=10xxxx
//    = BranchImmediate24 => Branch {constraints: ,
//     pattern: 'cccc1010iiiiiiiiiiiiiiiiiiiiiiii',
//     rule: 'B_Rule_16_A1_P44'}
TEST_F(Arm32DecoderStateTests,
       BranchImmediate24Tester_op_25To20Is10xxxx_B_Rule_16_A1_P44_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_Test) {
  BranchImmediate24Tester_op_25To20Is10xxxx_B_Rule_16_A1_P44 baseline_tester;
  NamedBranch_B_Rule_16_A1_P44 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1010iiiiiiiiiiiiiiiiiiiiiiii");
}

// op(25:20)=11xxxx
//    = BranchImmediate24 => Branch {constraints: ,
//     pattern: 'cccc1011iiiiiiiiiiiiiiiiiiiiiiii',
//     rule: 'Bl_Blx_Rule_23_A1_P58'}
TEST_F(Arm32DecoderStateTests,
       BranchImmediate24Tester_op_25To20Is11xxxx_Bl_Blx_Rule_23_A1_P58_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_Test) {
  BranchImmediate24Tester_op_25To20Is11xxxx_Bl_Blx_Rule_23_A1_P58 baseline_tester;
  NamedBranch_Bl_Blx_Rule_23_A1_P58 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1011iiiiiiiiiiiiiiiiiiiiiiii");
}

// op1(25:20)=00000x
//    = UndefinedCondDecoder => Undefined {constraints: ,
//     pattern: 'cccc1100000xnnnnxxxxccccxxxoxxxx',
//     rule: 'Undefined_A5_6'}
TEST_F(Arm32DecoderStateTests,
       UndefinedCondDecoderTester_op1_25To20Is00000x_Undefined_A5_6_cccc1100000xnnnnxxxxccccxxxoxxxx_Test) {
  UndefinedCondDecoderTester_op1_25To20Is00000x_Undefined_A5_6 baseline_tester;
  NamedUndefined_Undefined_A5_6 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1100000xnnnnxxxxccccxxxoxxxx");
}

// op1(25:20)=11xxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc1111iiiiiiiiiiiiiiiiiiiiiiii',
//     rule: 'Svc_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op1_25To20Is11xxxx_Svc_Rule_A1_cccc1111iiiiiiiiiiiiiiiiiiiiiiii_Test) {
  ForbiddenCondDecoderTester_op1_25To20Is11xxxx_Svc_Rule_A1 baseline_tester;
  NamedForbidden_Svc_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1111iiiiiiiiiiiiiiiiiiiiiiii");
}

// coproc(11:8)=~101x & op1(25:20)=0xxxx0 & op1_repeated(25:20)=~000x00
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc110pudw0nnnnddddcccciiiiiiii',
//     rule: 'Stc_Rule_A2'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is0xxxx0_op1_repeated_25To20IsNot000x00_Stc_Rule_A2_cccc110pudw0nnnnddddcccciiiiiiii_Test) {
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is0xxxx0_op1_repeated_25To20IsNot000x00_Stc_Rule_A2 baseline_tester;
  NamedForbidden_Stc_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc110pudw0nnnnddddcccciiiiiiii");
}

// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=~1111 & op1_repeated(25:20)=~000x01
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc110pudw1nnnnddddcccciiiiiiii',
//     rule: 'Ldc_immediate_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16IsNot1111_op1_repeated_25To20IsNot000x01_Ldc_immediate_Rule_A1_cccc110pudw1nnnnddddcccciiiiiiii_Test) {
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16IsNot1111_op1_repeated_25To20IsNot000x01_Ldc_immediate_Rule_A1 baseline_tester;
  NamedForbidden_Ldc_immediate_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc110pudw1nnnnddddcccciiiiiiii");
}

// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=1111 & op1_repeated(25:20)=~000x01
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc110pudw11111ddddcccciiiiiiii',
//     rule: 'Ldc_literal_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16Is1111_op1_repeated_25To20IsNot000x01_Ldc_literal_Rule_A1_cccc110pudw11111ddddcccciiiiiiii_Test) {
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is0xxxx1_Rn_19To16Is1111_op1_repeated_25To20IsNot000x01_Ldc_literal_Rule_A1 baseline_tester;
  NamedForbidden_Ldc_literal_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc110pudw11111ddddcccciiiiiiii");
}

// coproc(11:8)=~101x & op1(25:20)=000100
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc11000100ttttttttccccoooommmm',
//     rule: 'Mcrr_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is000100_Mcrr_Rule_A1_cccc11000100ttttttttccccoooommmm_Test) {
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is000100_Mcrr_Rule_A1 baseline_tester;
  NamedForbidden_Mcrr_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11000100ttttttttccccoooommmm");
}

// coproc(11:8)=~101x & op1(25:20)=000101
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc11000101ttttttttccccoooommmm',
//     rule: 'Mrrc_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is000101_Mrrc_Rule_A1_cccc11000101ttttttttccccoooommmm_Test) {
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is000101_Mrrc_Rule_A1 baseline_tester;
  NamedForbidden_Mrrc_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11000101ttttttttccccoooommmm");
}

// coproc(11:8)=~101x & op1(25:20)=10xxxx & op(4)=0
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc1110oooonnnnddddccccooo0mmmm',
//     rule: 'Cdp_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is10xxxx_op_4Is0_Cdp_Rule_A1_cccc1110oooonnnnddddccccooo0mmmm_Test) {
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is10xxxx_op_4Is0_Cdp_Rule_A1 baseline_tester;
  NamedForbidden_Cdp_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1110oooonnnnddddccccooo0mmmm");
}

// coproc(11:8)=~101x & op1(25:20)=10xxx0 & op(4)=1
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc1110ooo0nnnnttttccccooo1mmmm',
//     rule: 'Mcr_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is10xxx0_op_4Is1_Mcr_Rule_A1_cccc1110ooo0nnnnttttccccooo1mmmm_Test) {
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is10xxx0_op_4Is1_Mcr_Rule_A1 baseline_tester;
  NamedForbidden_Mcr_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1110ooo0nnnnttttccccooo1mmmm");
}

// coproc(11:8)=~101x & op1(25:20)=10xxx1 & op(4)=1
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc1110ooo1nnnnttttccccooo1mmmm',
//     rule: 'Mrc_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is10xxx1_op_4Is1_Mrc_Rule_A1_cccc1110ooo1nnnnttttccccooo1mmmm_Test) {
  ForbiddenCondDecoderTester_coproc_11To8IsNot101x_op1_25To20Is10xxx1_op_4Is1_Mrc_Rule_A1 baseline_tester;
  NamedForbidden_Mrc_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1110ooo1nnnnttttccccooo1mmmm");
}

// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=1011
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc0000xx1xxxxxxxxxxxxx1011xxxx',
//     rule: 'extra_load_store_instructions_unpriviledged'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011_extra_load_store_instructions_unpriviledged_cccc0000xx1xxxxxxxxxxxxx1011xxxx_Test) {
  ForbiddenCondDecoderTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is1011_extra_load_store_instructions_unpriviledged baseline_tester;
  NamedForbidden_extra_load_store_instructions_unpriviledged actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000xx1xxxxxxxxxxxxx1011xxxx");
}

// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=11x1
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc0000xx1xxxxxxxxxxxxx11x1xxxx',
//     rule: 'extra_load_store_instructions_unpriviledged'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1_extra_load_store_instructions_unpriviledged_cccc0000xx1xxxxxxxxxxxxx11x1xxxx_Test) {
  ForbiddenCondDecoderTester_op_25Is0_op1_24To20Is0xx1x_op2_7To4Is11x1_extra_load_store_instructions_unpriviledged baseline_tester;
  NamedForbidden_extra_load_store_instructions_unpriviledged actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000xx1xxxxxxxxxxxxx11x1xxxx");
}

// op(25)=1 & op1(24:20)=10000
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: 'cccc00110000iiiiddddIIIIIIIIIIII',
//     rule: 'Mov_Rule_96_A2_P194',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10000RegsNotPc_Mov_Rule_96_A2_P194_cccc00110000iiiiddddIIIIIIIIIIII_Test) {
  Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10000RegsNotPc_Mov_Rule_96_A2_P194 baseline_tester;
  NamedDefs12To15_Mov_Rule_96_A2_P194 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110000iiiiddddIIIIIIIIIIII");
}

// op(25)=1 & op1(24:20)=10100
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: 'cccc00110100iiiiddddIIIIIIIIIIII',
//     rule: 'Mov_Rule_99_A1_P200',
//     safety: [Rd(15:12)=~1111]}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10100Safety_Rd_15To12IsNot1111_Mov_Rule_99_A1_P200_cccc00110100iiiiddddIIIIIIIIIIII_Test) {
  Unary1RegisterImmediateOpTester_op_25Is1_op1_24To20Is10100Safety_Rd_15To12IsNot1111_Mov_Rule_99_A1_P200 baseline_tester;
  NamedDefs12To15_Mov_Rule_99_A1_P200 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110100iiiiddddIIIIIIIIIIII");
}

// op(24:20)=0000x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0010000snnnnddddiiiiiiiiiiii',
//     rule: 'And_Rule_11_A1_P34',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0000xNotRdIsPcAndS_And_Rule_11_A1_P34_cccc0010000snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0000xNotRdIsPcAndS_And_Rule_11_A1_P34 baseline_tester;
  NamedDefs12To15_And_Rule_11_A1_P34 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010000snnnnddddiiiiiiiiiiii");
}

// op(24:20)=0001x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0010001snnnnddddiiiiiiiiiiii',
//     rule: 'Eor_Rule_44_A1_P94',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0001xNotRdIsPcAndS_Eor_Rule_44_A1_P94_cccc0010001snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0001xNotRdIsPcAndS_Eor_Rule_44_A1_P94 baseline_tester;
  NamedDefs12To15_Eor_Rule_44_A1_P94 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010001snnnnddddiiiiiiiiiiii");
}

// op(24:20)=0010x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0010010snnnnddddiiiiiiiiiiii',
//     rule: 'Sub_Rule_212_A1_P420',
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0010x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Sub_Rule_212_A1_P420_cccc0010010snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0010x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Sub_Rule_212_A1_P420 baseline_tester;
  NamedDefs12To15_Sub_Rule_212_A1_P420 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010010snnnnddddiiiiiiiiiiii");
}

// op(24:20)=00100 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: 'cccc001001001111ddddiiiiiiiiiiii',
//     rule: 'Adr_Rule_10_A2_P32'}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_op_24To20Is00100_Rn_19To16Is1111_Adr_Rule_10_A2_P32_cccc001001001111ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_op_24To20Is00100_Rn_19To16Is1111_Adr_Rule_10_A2_P32 baseline_tester;
  NamedDefs12To15_Adr_Rule_10_A2_P32 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001001001111ddddiiiiiiiiiiii");
}

// op(24:20)=00101 & Rn(19:16)=1111
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc00100101nnnn1111iiiiiiiiiiii',
//     rule: 'Subs_Pc_Lr_and_related_instructions_Rule_A1a'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op_24To20Is00101_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1a_cccc00100101nnnn1111iiiiiiiiiiii_Test) {
  ForbiddenCondDecoderTester_op_24To20Is00101_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1a baseline_tester;
  NamedForbidden_Subs_Pc_Lr_and_related_instructions_Rule_A1a actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00100101nnnn1111iiiiiiiiiiii");
}

// op(24:20)=0011x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0010011snnnnddddiiiiiiiiiiii',
//     rule: 'Rsb_Rule_142_A1_P284',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_142_A1_P284_cccc0010011snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_142_A1_P284 baseline_tester;
  NamedDefs12To15_Rsb_Rule_142_A1_P284 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010011snnnnddddiiiiiiiiiiii");
}

// op(24:20)=0100x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0010100snnnnddddiiiiiiiiiiii',
//     rule: 'Add_Rule_5_A1_P22',
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0100x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Add_Rule_5_A1_P22_cccc0010100snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0100x_Rn_19To16IsNot1111NeitherRdIsPcAndSNorRnIsPcAndNotS_Add_Rule_5_A1_P22 baseline_tester;
  NamedDefs12To15_Add_Rule_5_A1_P22 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010100snnnnddddiiiiiiiiiiii");
}

// op(24:20)=01000 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: 'cccc001010001111ddddiiiiiiiiiiii',
//     rule: 'Adr_Rule_10_A1_P32'}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_op_24To20Is01000_Rn_19To16Is1111_Adr_Rule_10_A1_P32_cccc001010001111ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_op_24To20Is01000_Rn_19To16Is1111_Adr_Rule_10_A1_P32 baseline_tester;
  NamedDefs12To15_Adr_Rule_10_A1_P32 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001010001111ddddiiiiiiiiiiii");
}

// op(24:20)=01001 & Rn(19:16)=1111
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc00101001nnnn1111iiiiiiiiiiii',
//     rule: 'Subs_Pc_Lr_and_related_instructions_Rule_A1b'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op_24To20Is01001_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1b_cccc00101001nnnn1111iiiiiiiiiiii_Test) {
  ForbiddenCondDecoderTester_op_24To20Is01001_Rn_19To16Is1111_Subs_Pc_Lr_and_related_instructions_Rule_A1b baseline_tester;
  NamedForbidden_Subs_Pc_Lr_and_related_instructions_Rule_A1b actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00101001nnnn1111iiiiiiiiiiii");
}

// op(24:20)=0101x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0010101snnnnddddiiiiiiiiiiii',
//     rule: 'Adc_Rule_6_A1_P14',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0101xNotRdIsPcAndS_Adc_Rule_6_A1_P14_cccc0010101snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0101xNotRdIsPcAndS_Adc_Rule_6_A1_P14 baseline_tester;
  NamedDefs12To15_Adc_Rule_6_A1_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010101snnnnddddiiiiiiiiiiii");
}

// op(24:20)=0110x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0010110snnnnddddiiiiiiiiiiii',
//     rule: 'Sbc_Rule_151_A1_P302',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_151_A1_P302_cccc0010110snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_151_A1_P302 baseline_tester;
  NamedDefs12To15_Sbc_Rule_151_A1_P302 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010110snnnnddddiiiiiiiiiiii");
}

// op(24:20)=0111x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0010111snnnnddddiiiiiiiiiiii',
//     rule: 'Rsc_Rule_145_A1_P290',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_145_A1_P290_cccc0010111snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_145_A1_P290 baseline_tester;
  NamedDefs12To15_Rsc_Rule_145_A1_P290 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010111snnnnddddiiiiiiiiiiii");
}

// op(24:20)=10001
//    = MaskedBinaryRegisterImmediateTest => TestIfAddressMasked {constraints: ,
//     pattern: 'cccc00110001nnnn0000iiiiiiiiiiii',
//     rule: 'Tst_Rule_230_A1_P454'}
TEST_F(Arm32DecoderStateTests,
       MaskedBinaryRegisterImmediateTestTester_op_24To20Is10001_Tst_Rule_230_A1_P454_cccc00110001nnnn0000iiiiiiiiiiii_Test) {
  MaskedBinaryRegisterImmediateTestTester_op_24To20Is10001_Tst_Rule_230_A1_P454 baseline_tester;
  NamedTestIfAddressMasked_Tst_Rule_230_A1_P454 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110001nnnn0000iiiiiiiiiiii");
}

// op(24:20)=10011
//    = BinaryRegisterImmediateTest => DontCareInst {constraints: ,
//     pattern: 'cccc00110011nnnn0000iiiiiiiiiiii',
//     rule: 'Teq_Rule_227_A1_P448'}
TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_op_24To20Is10011_Teq_Rule_227_A1_P448_cccc00110011nnnn0000iiiiiiiiiiii_Test) {
  BinaryRegisterImmediateTestTester_op_24To20Is10011_Teq_Rule_227_A1_P448 baseline_tester;
  NamedDontCareInst_Teq_Rule_227_A1_P448 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110011nnnn0000iiiiiiiiiiii");
}

// op(24:20)=10101
//    = BinaryRegisterImmediateTest => DontCareInst {constraints: ,
//     pattern: 'cccc00110101nnnn0000iiiiiiiiiiii',
//     rule: 'Cmp_Rule_35_A1_P80'}
TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_op_24To20Is10101_Cmp_Rule_35_A1_P80_cccc00110101nnnn0000iiiiiiiiiiii_Test) {
  BinaryRegisterImmediateTestTester_op_24To20Is10101_Cmp_Rule_35_A1_P80 baseline_tester;
  NamedDontCareInst_Cmp_Rule_35_A1_P80 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110101nnnn0000iiiiiiiiiiii");
}

// op(24:20)=10111
//    = BinaryRegisterImmediateTest => DontCareInst {constraints: ,
//     pattern: 'cccc00110111nnnn0000iiiiiiiiiiii',
//     rule: 'Cmn_Rule_32_A1_P74'}
TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_op_24To20Is10111_Cmn_Rule_32_A1_P74_cccc00110111nnnn0000iiiiiiiiiiii_Test) {
  BinaryRegisterImmediateTestTester_op_24To20Is10111_Cmn_Rule_32_A1_P74 baseline_tester;
  NamedDontCareInst_Cmn_Rule_32_A1_P74 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110111nnnn0000iiiiiiiiiiii");
}

// op(24:20)=1100x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0011100snnnnddddiiiiiiiiiiii',
//     rule: 'Orr_Rule_113_A1_P228',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_op_24To20Is1100xNotRdIsPcAndS_Orr_Rule_113_A1_P228_cccc0011100snnnnddddiiiiiiiiiiii_Test) {
  Binary2RegisterImmediateOpTester_op_24To20Is1100xNotRdIsPcAndS_Orr_Rule_113_A1_P228 baseline_tester;
  NamedDefs12To15_Orr_Rule_113_A1_P228 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011100snnnnddddiiiiiiiiiiii");
}

// op(24:20)=1101x
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0011101s0000ddddiiiiiiiiiiii',
//     rule: 'Mov_Rule_96_A1_P194',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_op_24To20Is1101xNotRdIsPcAndS_Mov_Rule_96_A1_P194_cccc0011101s0000ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_op_24To20Is1101xNotRdIsPcAndS_Mov_Rule_96_A1_P194 baseline_tester;
  NamedDefs12To15_Mov_Rule_96_A1_P194 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011101s0000ddddiiiiiiiiiiii");
}

// op(24:20)=1110x
//    = MaskedBinary2RegisterImmediateOp => MaskAddress {constraints: ,
//     pattern: 'cccc0011110snnnnddddiiiiiiiiiiii',
//     rule: 'Bic_Rule_19_A1_P50',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       MaskedBinary2RegisterImmediateOpTester_op_24To20Is1110xNotRdIsPcAndS_Bic_Rule_19_A1_P50_cccc0011110snnnnddddiiiiiiiiiiii_Test) {
  MaskedBinary2RegisterImmediateOpTester_op_24To20Is1110xNotRdIsPcAndS_Bic_Rule_19_A1_P50 baseline_tester;
  NamedMaskAddress_Bic_Rule_19_A1_P50 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011110snnnnddddiiiiiiiiiiii");
}

// op(24:20)=1111x
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0011111s0000ddddiiiiiiiiiiii',
//     rule: 'Mvn_Rule_106_A1_P214',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_op_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_106_A1_P214_cccc0011111s0000ddddiiiiiiiiiiii_Test) {
  Unary1RegisterImmediateOpTester_op_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_106_A1_P214 baseline_tester;
  NamedDefs12To15_Mvn_Rule_106_A1_P214 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011111s0000ddddiiiiiiiiiiii");
}

// op1(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0000000unnnnddddiiiiitt0mmmm',
//     rule: 'And_Rule_7_A1_P36',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0000xNotRdIsPcAndS_And_Rule_7_A1_P36_cccc0000000unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0000xNotRdIsPcAndS_And_Rule_7_A1_P36 baseline_tester;
  NamedDefs12To15_And_Rule_7_A1_P36 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000unnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0001x
//    = Binary3RegisterImmedShiftedOp => Defs12To15CondsDontCare {constraints: ,
//     pattern: 'cccc0000001unnnnddddiiiiitt0mmmm',
//     rule: 'Eor_Rule_45_A1_P96',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0001xNotRdIsPcAndS_Eor_Rule_45_A1_P96_cccc0000001unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0001xNotRdIsPcAndS_Eor_Rule_45_A1_P96 baseline_tester;
  NamedDefs12To15CondsDontCare_Eor_Rule_45_A1_P96 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001unnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0010x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0000010unnnnddddiiiiitt0mmmm',
//     rule: 'Sub_Rule_213_A1_P422',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0010xNotRdIsPcAndS_Sub_Rule_213_A1_P422_cccc0000010unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0010xNotRdIsPcAndS_Sub_Rule_213_A1_P422 baseline_tester;
  NamedDefs12To15_Sub_Rule_213_A1_P422 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000010unnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0011x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0000011unnnnddddiiiiitt0mmmm',
//     rule: 'Rsb_Rule_143_P286',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_143_P286_cccc0000011unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0011xNotRdIsPcAndS_Rsb_Rule_143_P286 baseline_tester;
  NamedDefs12To15_Rsb_Rule_143_P286 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000011unnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0100x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0000100unnnnddddiiiiitt0mmmm',
//     rule: 'Add_Rule_6_A1_P24',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0100xNotRdIsPcAndS_Add_Rule_6_A1_P24_cccc0000100unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0100xNotRdIsPcAndS_Add_Rule_6_A1_P24 baseline_tester;
  NamedDefs12To15_Add_Rule_6_A1_P24 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100unnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0000101unnnnddddiiiiitt0mmmm',
//     rule: 'Adc_Rule_2_A1_P16',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0101xNotRdIsPcAndS_Adc_Rule_2_A1_P16_cccc0000101unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0101xNotRdIsPcAndS_Adc_Rule_2_A1_P16 baseline_tester;
  NamedDefs12To15_Adc_Rule_2_A1_P16 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101unnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0110x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0000110unnnnddddiiiiitt0mmmm',
//     rule: 'Sbc_Rule_152_A1_P304',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_152_A1_P304_cccc0000110unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0110xNotRdIsPcAndS_Sbc_Rule_152_A1_P304 baseline_tester;
  NamedDefs12To15_Sbc_Rule_152_A1_P304 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110unnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0000111unnnnddddiiiiitt0mmmm',
//     rule: 'Rsc_Rule_146_A1_P292',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_146_A1_P292_cccc0000111unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is0111xNotRdIsPcAndS_Rsc_Rule_146_A1_P292 baseline_tester;
  NamedDefs12To15_Rsc_Rule_146_A1_P292 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111unnnnddddiiiiitt0mmmm");
}

// op1(24:20)=10001
//    = Binary2RegisterImmedShiftedTest => DontCareInst {constraints: ,
//     pattern: 'cccc00010001nnnn0000iiiiitt0mmmm',
//     rule: 'Tst_Rule_231_A1_P456'}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_op1_24To20Is10001_Tst_Rule_231_A1_P456_cccc00010001nnnn0000iiiiitt0mmmm_Test) {
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10001_Tst_Rule_231_A1_P456 baseline_tester;
  NamedDontCareInst_Tst_Rule_231_A1_P456 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010001nnnn0000iiiiitt0mmmm");
}

// op1(24:20)=10011
//    = Binary2RegisterImmedShiftedTest => DontCareInst {constraints: ,
//     pattern: 'cccc00010011nnnn0000iiiiitt0mmmm',
//     rule: 'Teq_Rule_228_A1_P450'}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_op1_24To20Is10011_Teq_Rule_228_A1_P450_cccc00010011nnnn0000iiiiitt0mmmm_Test) {
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10011_Teq_Rule_228_A1_P450 baseline_tester;
  NamedDontCareInst_Teq_Rule_228_A1_P450 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010011nnnn0000iiiiitt0mmmm");
}

// op1(24:20)=10101
//    = Binary2RegisterImmedShiftedTest => DontCareInst {constraints: ,
//     pattern: 'cccc00010101nnnn0000iiiiitt0mmmm',
//     rule: 'Cmp_Rule_36_A1_P82'}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_op1_24To20Is10101_Cmp_Rule_36_A1_P82_cccc00010101nnnn0000iiiiitt0mmmm_Test) {
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10101_Cmp_Rule_36_A1_P82 baseline_tester;
  NamedDontCareInst_Cmp_Rule_36_A1_P82 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010101nnnn0000iiiiitt0mmmm");
}

// op1(24:20)=10111
//    = Binary2RegisterImmedShiftedTest => DontCareInst {constraints: ,
//     pattern: 'cccc00010111nnnn0000iiiiitt0mmmm',
//     rule: 'Cmn_Rule_33_A1_P76'}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_op1_24To20Is10111_Cmn_Rule_33_A1_P76_cccc00010111nnnn0000iiiiitt0mmmm_Test) {
  Binary2RegisterImmedShiftedTestTester_op1_24To20Is10111_Cmn_Rule_33_A1_P76 baseline_tester;
  NamedDontCareInst_Cmn_Rule_33_A1_P76 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010111nnnn0000iiiiitt0mmmm");
}

// op1(24:20)=1100x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0001100unnnnddddiiiiitt0mmmm',
//     rule: 'Orr_Rule_114_A1_P230',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is1100xNotRdIsPcAndS_Orr_Rule_114_A1_P230_cccc0001100unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is1100xNotRdIsPcAndS_Orr_Rule_114_A1_P230 baseline_tester;
  NamedDefs12To15_Orr_Rule_114_A1_P230 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001100unnnnddddiiiiitt0mmmm");
}

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00
//    = Unary2RegisterOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0001101u0000dddd00000000mmmm',
//     rule: 'Mov_Rule_97_A1_P196',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is00NotRdIsPcAndS_Mov_Rule_97_A1_P196_cccc0001101u0000dddd00000000mmmm_Test) {
  Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is00NotRdIsPcAndS_Mov_Rule_97_A1_P196 baseline_tester;
  NamedDefs12To15_Mov_Rule_97_A1_P196 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000dddd00000000mmmm");
}

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0001101u0000ddddiiiii000mmmm',
//     rule: 'Lsl_Rule_88_A1_P178',
//     safety: ['NeitherImm5NotZeroNorNotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS_Lsl_Rule_88_A1_P178_cccc0001101u0000ddddiiiii000mmmm_Test) {
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is00NeitherImm5NotZeroNorNotRdIsPcAndS_Lsl_Rule_88_A1_P178 baseline_tester;
  NamedDefs12To15_Lsl_Rule_88_A1_P178 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii000mmmm");
}

// op1(24:20)=1101x & op3(6:5)=01
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0001101u0000ddddiiiii010mmmm',
//     rule: 'Lsr_Rule_90_A1_P182',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is01NotRdIsPcAndS_Lsr_Rule_90_A1_P182_cccc0001101u0000ddddiiiii010mmmm_Test) {
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is01NotRdIsPcAndS_Lsr_Rule_90_A1_P182 baseline_tester;
  NamedDefs12To15_Lsr_Rule_90_A1_P182 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii010mmmm");
}

// op1(24:20)=1101x & op3(6:5)=10
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0001101u0000ddddiiiii100mmmm',
//     rule: 'Asr_Rule_14_A1_P40',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is10NotRdIsPcAndS_Asr_Rule_14_A1_P40_cccc0001101u0000ddddiiiii100mmmm_Test) {
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op3_6To5Is10NotRdIsPcAndS_Asr_Rule_14_A1_P40 baseline_tester;
  NamedDefs12To15_Asr_Rule_14_A1_P40 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii100mmmm");
}

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11
//    = Unary2RegisterOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0001101u0000dddd00000110mmmm',
//     rule: 'Rrx_Rule_141_A1_P282',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is11NotRdIsPcAndS_Rrx_Rule_141_A1_P282_cccc0001101u0000dddd00000110mmmm_Test) {
  Unary2RegisterOpTester_op1_24To20Is1101x_op2_11To7Is00000_op3_6To5Is11NotRdIsPcAndS_Rrx_Rule_141_A1_P282 baseline_tester;
  NamedDefs12To15_Rrx_Rule_141_A1_P282 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000dddd00000110mmmm");
}

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0001101u0000ddddiiiii110mmmm',
//     rule: 'Ror_Rule_139_A1_P278',
//     safety: ['NeitherImm5NotZeroNorNotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS_Ror_Rule_139_A1_P278_cccc0001101u0000ddddiiiii110mmmm_Test) {
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1101x_op2_11To7IsNot00000_op3_6To5Is11NeitherImm5NotZeroNorNotRdIsPcAndS_Ror_Rule_139_A1_P278 baseline_tester;
  NamedDefs12To15_Ror_Rule_139_A1_P278 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii110mmmm");
}

// op1(24:20)=1110x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0001110unnnnddddiiiiitt0mmmm',
//     rule: 'Bic_Rule_20_A1_P52',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_op1_24To20Is1110xNotRdIsPcAndS_Bic_Rule_20_A1_P52_cccc0001110unnnnddddiiiiitt0mmmm_Test) {
  Binary3RegisterImmedShiftedOpTester_op1_24To20Is1110xNotRdIsPcAndS_Bic_Rule_20_A1_P52 baseline_tester;
  NamedDefs12To15_Bic_Rule_20_A1_P52 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001110unnnnddddiiiiitt0mmmm");
}

// op1(24:20)=1111x
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: 'cccc0001111u0000ddddiiiiitt0mmmm',
//     rule: 'Mvn_Rule_107_A1_P216',
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_op1_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_107_A1_P216_cccc0001111u0000ddddiiiiitt0mmmm_Test) {
  Unary2RegisterImmedShiftedOpTester_op1_24To20Is1111xNotRdIsPcAndS_Mvn_Rule_107_A1_P216 baseline_tester;
  NamedDefs12To15_Mvn_Rule_107_A1_P216 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001111u0000ddddiiiiitt0mmmm");
}

// op1(24:20)=0000x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: 'cccc0000000snnnnddddssss0tt1mmmm',
//     rule: 'And_Rule_13_A1_P38',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0000xRegsNotPc_And_Rule_13_A1_P38_cccc0000000snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0000xRegsNotPc_And_Rule_13_A1_P38 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_And_Rule_13_A1_P38 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000snnnnddddssss0tt1mmmm");
}

// op1(24:20)=0001x
//    = Binary4RegisterShiftedOp => Defs12To15CondsDontCareRdRnRsRmNotPc {constraints: ,
//     pattern: 'cccc0000001snnnnddddssss0tt1mmmm',
//     rule: 'Eor_Rule_46_A1_P98',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0001xRegsNotPc_Eor_Rule_46_A1_P98_cccc0000001snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0001xRegsNotPc_Eor_Rule_46_A1_P98 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnRsRmNotPc_Eor_Rule_46_A1_P98 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001snnnnddddssss0tt1mmmm");
}

// op1(24:20)=0010x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: 'cccc0000010snnnnddddssss0tt1mmmm',
//     rule: 'Sub_Rule_214_A1_P424',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0010xRegsNotPc_Sub_Rule_214_A1_P424_cccc0000010snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0010xRegsNotPc_Sub_Rule_214_A1_P424 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Sub_Rule_214_A1_P424 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000010snnnnddddssss0tt1mmmm");
}

// op1(24:20)=0011x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: 'cccc0000011snnnnddddssss0tt1mmmm',
//     rule: 'Rsb_Rule_144_A1_P288',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0011xRegsNotPc_Rsb_Rule_144_A1_P288_cccc0000011snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0011xRegsNotPc_Rsb_Rule_144_A1_P288 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Rsb_Rule_144_A1_P288 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000011snnnnddddssss0tt1mmmm");
}

// op1(24:20)=0100x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: 'cccc0000100snnnnddddssss0tt1mmmm',
//     rule: 'Add_Rule_7_A1_P26',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0100xRegsNotPc_Add_Rule_7_A1_P26_cccc0000100snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0100xRegsNotPc_Add_Rule_7_A1_P26 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Add_Rule_7_A1_P26 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100snnnnddddssss0tt1mmmm");
}

// op1(24:20)=0101x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: 'cccc0000101snnnnddddssss0tt1mmmm',
//     rule: 'Adc_Rule_3_A1_P18',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0101xRegsNotPc_Adc_Rule_3_A1_P18_cccc0000101snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0101xRegsNotPc_Adc_Rule_3_A1_P18 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Adc_Rule_3_A1_P18 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101snnnnddddssss0tt1mmmm");
}

// op1(24:20)=0110x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: 'cccc0000110snnnnddddssss0tt1mmmm',
//     rule: 'Sbc_Rule_153_A1_P306',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0110xRegsNotPc_Sbc_Rule_153_A1_P306_cccc0000110snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0110xRegsNotPc_Sbc_Rule_153_A1_P306 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Sbc_Rule_153_A1_P306 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110snnnnddddssss0tt1mmmm");
}

// op1(24:20)=0111x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: 'cccc0000111snnnnddddssss0tt1mmmm',
//     rule: 'Rsc_Rule_147_A1_P294',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is0111xRegsNotPc_Rsc_Rule_147_A1_P294_cccc0000111snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is0111xRegsNotPc_Rsc_Rule_147_A1_P294 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Rsc_Rule_147_A1_P294 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111snnnnddddssss0tt1mmmm");
}

// op1(24:20)=10001
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: 'cccc00010001nnnn0000ssss0tt1mmmm',
//     rule: 'Tst_Rule_232_A1_P458',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_op1_24To20Is10001RegsNotPc_Tst_Rule_232_A1_P458_cccc00010001nnnn0000ssss0tt1mmmm_Test) {
  Binary3RegisterShiftedTestTester_op1_24To20Is10001RegsNotPc_Tst_Rule_232_A1_P458 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Tst_Rule_232_A1_P458 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010001nnnn0000ssss0tt1mmmm");
}

// op1(24:20)=10011
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: 'cccc00010011nnnn0000ssss0tt1mmmm',
//     rule: 'Teq_Rule_229_A1_P452',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_op1_24To20Is10011RegsNotPc_Teq_Rule_229_A1_P452_cccc00010011nnnn0000ssss0tt1mmmm_Test) {
  Binary3RegisterShiftedTestTester_op1_24To20Is10011RegsNotPc_Teq_Rule_229_A1_P452 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Teq_Rule_229_A1_P452 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010011nnnn0000ssss0tt1mmmm");
}

// op1(24:20)=10101
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: 'cccc00010101nnnn0000ssss0tt1mmmm',
//     rule: 'Cmp_Rule_37_A1_P84',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_op1_24To20Is10101RegsNotPc_Cmp_Rule_37_A1_P84_cccc00010101nnnn0000ssss0tt1mmmm_Test) {
  Binary3RegisterShiftedTestTester_op1_24To20Is10101RegsNotPc_Cmp_Rule_37_A1_P84 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Cmp_Rule_37_A1_P84 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010101nnnn0000ssss0tt1mmmm");
}

// op1(24:20)=10111
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: 'cccc00010111nnnn0000ssss0tt1mmmm',
//     rule: 'Cmn_Rule_34_A1_P78',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_op1_24To20Is10111RegsNotPc_Cmn_Rule_34_A1_P78_cccc00010111nnnn0000ssss0tt1mmmm_Test) {
  Binary3RegisterShiftedTestTester_op1_24To20Is10111RegsNotPc_Cmn_Rule_34_A1_P78 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Cmn_Rule_34_A1_P78 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010111nnnn0000ssss0tt1mmmm");
}

// op1(24:20)=1100x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: 'cccc0001100snnnnddddssss0tt1mmmm',
//     rule: 'Orr_Rule_115_A1_P212',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is1100xRegsNotPc_Orr_Rule_115_A1_P212_cccc0001100snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is1100xRegsNotPc_Orr_Rule_115_A1_P212 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Orr_Rule_115_A1_P212 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001100snnnnddddssss0tt1mmmm");
}

// op1(24:20)=1101x & op2(6:5)=00
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: 'cccc0001101s0000ddddmmmm0001nnnn',
//     rule: 'Lsl_Rule_89_A1_P180',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is00RegsNotPc_Lsl_Rule_89_A1_P180_cccc0001101s0000ddddmmmm0001nnnn_Test) {
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is00RegsNotPc_Lsl_Rule_89_A1_P180 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Lsl_Rule_89_A1_P180 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0001nnnn");
}

// op1(24:20)=1101x & op2(6:5)=01
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: 'cccc0001101s0000ddddmmmm0011nnnn',
//     rule: 'Lsr_Rule_91_A1_P184',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is01RegsNotPc_Lsr_Rule_91_A1_P184_cccc0001101s0000ddddmmmm0011nnnn_Test) {
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is01RegsNotPc_Lsr_Rule_91_A1_P184 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Lsr_Rule_91_A1_P184 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0011nnnn");
}

// op1(24:20)=1101x & op2(6:5)=10
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: 'cccc0001101s0000ddddmmmm0101nnnn',
//     rule: 'Asr_Rule_15_A1_P42',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is10RegsNotPc_Asr_Rule_15_A1_P42_cccc0001101s0000ddddmmmm0101nnnn_Test) {
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is10RegsNotPc_Asr_Rule_15_A1_P42 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Asr_Rule_15_A1_P42 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0101nnnn");
}

// op1(24:20)=1101x & op2(6:5)=11
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: 'cccc0001101s0000ddddmmmm0111nnnn',
//     rule: 'Ror_Rule_140_A1_P280',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is11RegsNotPc_Ror_Rule_140_A1_P280_cccc0001101s0000ddddmmmm0111nnnn_Test) {
  Binary3RegisterOpTester_op1_24To20Is1101x_op2_6To5Is11RegsNotPc_Ror_Rule_140_A1_P280 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Ror_Rule_140_A1_P280 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0111nnnn");
}

// op1(24:20)=1110x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: 'cccc0001110snnnnddddssss0tt1mmmm',
//     rule: 'Bic_Rule_21_A1_P54',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_op1_24To20Is1110xRegsNotPc_Bic_Rule_21_A1_P54_cccc0001110snnnnddddssss0tt1mmmm_Test) {
  Binary4RegisterShiftedOpTester_op1_24To20Is1110xRegsNotPc_Bic_Rule_21_A1_P54 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Bic_Rule_21_A1_P54 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001110snnnnddddssss0tt1mmmm");
}

// op1(24:20)=1111x
//    = Unary3RegisterShiftedOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: 'cccc0001111s0000ddddssss0tt1mmmm',
//     rule: 'Mvn_Rule_108_A1_P218',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary3RegisterShiftedOpTester_op1_24To20Is1111xRegsNotPc_Mvn_Rule_108_A1_P218_cccc0001111s0000ddddssss0tt1mmmm_Test) {
  Unary3RegisterShiftedOpTester_op1_24To20Is1111xRegsNotPc_Mvn_Rule_108_A1_P218 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Mvn_Rule_108_A1_P218 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001111s0000ddddssss0tt1mmmm");
}

// opcode(24:20)=01x00
//    = StoreVectorRegisterList => StoreVectorRegisterList {constraints: ,
//     pattern: 'cccc11001d00nnnndddd101xiiiiiiii',
//     rule: 'Vstm_Rule_399_A1_A2_P784'}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_opcode_24To20Is01x00_Vstm_Rule_399_A1_A2_P784_cccc11001d00nnnndddd101xiiiiiiii_Test) {
  StoreVectorRegisterListTester_opcode_24To20Is01x00_Vstm_Rule_399_A1_A2_P784 tester;
  tester.Test("cccc11001d00nnnndddd101xiiiiiiii");
}

// opcode(24:20)=01x10
//    = StoreVectorRegisterList => StoreVectorRegisterList {constraints: ,
//     pattern: 'cccc11001d10nnnndddd101xiiiiiiii',
//     rule: 'Vstm_Rule_399_A1_A2_P784'}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_opcode_24To20Is01x10_Vstm_Rule_399_A1_A2_P784_cccc11001d10nnnndddd101xiiiiiiii_Test) {
  StoreVectorRegisterListTester_opcode_24To20Is01x10_Vstm_Rule_399_A1_A2_P784 tester;
  tester.Test("cccc11001d10nnnndddd101xiiiiiiii");
}

// opcode(24:20)=1xx00
//    = StoreVectorRegister => StoreVectorRegister {constraints: ,
//     pattern: 'cccc1101ud00nnnndddd101xiiiiiiii',
//     rule: 'Vstr_Rule_400_A1_A2_P786'}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterTester_opcode_24To20Is1xx00_Vstr_Rule_400_A1_A2_P786_cccc1101ud00nnnndddd101xiiiiiiii_Test) {
  StoreVectorRegisterTester_opcode_24To20Is1xx00_Vstr_Rule_400_A1_A2_P786 tester;
  tester.Test("cccc1101ud00nnnndddd101xiiiiiiii");
}

// opcode(24:20)=10x10 & Rn(19:16)=~1101
//    = StoreVectorRegisterList => StoreVectorRegisterList {constraints: ,
//     pattern: 'cccc11010d10nnnndddd101xiiiiiiii',
//     rule: 'Vstm_Rule_399_A1_A2_P784',
//     safety: ['NotRnIsSp']}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_opcode_24To20Is10x10_Rn_19To16IsNot1101NotRnIsSp_Vstm_Rule_399_A1_A2_P784_cccc11010d10nnnndddd101xiiiiiiii_Test) {
  StoreVectorRegisterListTester_opcode_24To20Is10x10_Rn_19To16IsNot1101NotRnIsSp_Vstm_Rule_399_A1_A2_P784 tester;
  tester.Test("cccc11010d10nnnndddd101xiiiiiiii");
}

// opcode(24:20)=10x10 & Rn(19:16)=1101
//    = StoreVectorRegisterList => StoreVectorRegisterList {constraints: ,
//     pattern: 'cccc11010d101101dddd101xiiiiiiii',
//     rule: 'Vpush_355_A1_A2_P696'}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_opcode_24To20Is10x10_Rn_19To16Is1101_Vpush_355_A1_A2_P696_cccc11010d101101dddd101xiiiiiiii_Test) {
  StoreVectorRegisterListTester_opcode_24To20Is10x10_Rn_19To16Is1101_Vpush_355_A1_A2_P696 tester;
  tester.Test("cccc11010d101101dddd101xiiiiiiii");
}

// opcode(24:20)=01x01
//    = LoadVectorRegisterList => LoadVectorRegisterList {constraints: ,
//     pattern: 'cccc11001d01nnnndddd101xiiiiiiii',
//     rule: 'Vldm_Rule_319_A1_A2_P626'}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_opcode_24To20Is01x01_Vldm_Rule_319_A1_A2_P626_cccc11001d01nnnndddd101xiiiiiiii_Test) {
  LoadVectorRegisterListTester_opcode_24To20Is01x01_Vldm_Rule_319_A1_A2_P626 tester;
  tester.Test("cccc11001d01nnnndddd101xiiiiiiii");
}

// opcode(24:20)=01x11 & Rn(19:16)=~1101
//    = LoadVectorRegisterList => LoadVectorRegisterList {constraints: ,
//     pattern: 'cccc11001d11nnnndddd101xiiiiiiii',
//     rule: 'Vldm_Rule_319_A1_A2_P626',
//     safety: ['NotRnIsSp']}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_opcode_24To20Is01x11_Rn_19To16IsNot1101NotRnIsSp_Vldm_Rule_319_A1_A2_P626_cccc11001d11nnnndddd101xiiiiiiii_Test) {
  LoadVectorRegisterListTester_opcode_24To20Is01x11_Rn_19To16IsNot1101NotRnIsSp_Vldm_Rule_319_A1_A2_P626 tester;
  tester.Test("cccc11001d11nnnndddd101xiiiiiiii");
}

// opcode(24:20)=01x11 & Rn(19:16)=1101
//    = LoadVectorRegisterList => LoadVectorRegisterList {constraints: ,
//     pattern: 'cccc11001d111101dddd101xiiiiiiii',
//     rule: 'Vpop_Rule_354_A1_A2_P694'}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_opcode_24To20Is01x11_Rn_19To16Is1101_Vpop_Rule_354_A1_A2_P694_cccc11001d111101dddd101xiiiiiiii_Test) {
  LoadVectorRegisterListTester_opcode_24To20Is01x11_Rn_19To16Is1101_Vpop_Rule_354_A1_A2_P694 tester;
  tester.Test("cccc11001d111101dddd101xiiiiiiii");
}

// opcode(24:20)=1xx01
//    = LoadVectorRegister => LoadVectorRegister {constraints: ,
//     pattern: 'cccc1101ud01nnnndddd101xiiiiiiii',
//     rule: 'Vldr_Rule_320_A1_A2_P628'}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterTester_opcode_24To20Is1xx01_Vldr_Rule_320_A1_A2_P628_cccc1101ud01nnnndddd101xiiiiiiii_Test) {
  LoadVectorRegisterTester_opcode_24To20Is1xx01_Vldr_Rule_320_A1_A2_P628 tester;
  tester.Test("cccc1101ud01nnnndddd101xiiiiiiii");
}

// opcode(24:20)=10x11
//    = LoadVectorRegisterList => LoadVectorRegisterList {constraints: ,
//     pattern: 'cccc11010d11nnnndddd101xiiiiiiii',
//     rule: 'Vldm_Rule_318_A1_A2_P626'}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_opcode_24To20Is10x11_Vldm_Rule_318_A1_A2_P626_cccc11010d11nnnndddd101xiiiiiiii_Test) {
  LoadVectorRegisterListTester_opcode_24To20Is10x11_Vldm_Rule_318_A1_A2_P626 tester;
  tester.Test("cccc11010d11nnnndddd101xiiiiiiii");
}

// op2(6:5)=01 & op1(24:20)=xx0x0
//    = Store3RegisterOp => StoreBasedOffsetMemory {constraints: ,
//     pattern: 'cccc000pu0w0nnnntttt00001011mmmm',
//     rule: 'Strh_Rule_208_A1_P412'}
TEST_F(Arm32DecoderStateTests,
       Store3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x0_Strh_Rule_208_A1_P412_cccc000pu0w0nnnntttt00001011mmmm_Test) {
  Store3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x0_Strh_Rule_208_A1_P412 baseline_tester;
  NamedStoreBasedOffsetMemory_Strh_Rule_208_A1_P412 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w0nnnntttt00001011mmmm");
}

// op2(6:5)=01 & op1(24:20)=xx0x1
//    = Load3RegisterOp => LoadBasedOffsetMemory {constraints: ,
//     pattern: 'cccc000pu0w1nnnntttt00001011mmmm',
//     rule: 'Ldrh_Rule_76_A1_P156'}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x1_Ldrh_Rule_76_A1_P156_cccc000pu0w1nnnntttt00001011mmmm_Test) {
  Load3RegisterOpTester_op2_6To5Is01_op1_24To20Isxx0x1_Ldrh_Rule_76_A1_P156 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrh_Rule_76_A1_P156 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w1nnnntttt00001011mmmm");
}

// op2(6:5)=01 & op1(24:20)=xx1x0
//    = Store2RegisterImm8Op => StoreBasedImmedMemory {constraints: ,
//     pattern: 'cccc000pu1w0nnnnttttiiii1011iiii',
//     rule: 'Strh_Rule_207_A1_P410'}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x0_Strh_Rule_207_A1_P410_cccc000pu1w0nnnnttttiiii1011iiii_Test) {
  Store2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x0_Strh_Rule_207_A1_P410 baseline_tester;
  NamedStoreBasedImmedMemory_Strh_Rule_207_A1_P410 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w0nnnnttttiiii1011iiii");
}

// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {constraints: ,
//     pattern: 'cccc000pu1w1nnnnttttiiii1011iiii',
//     rule: 'Ldrh_Rule_74_A1_P152'}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16IsNot1111_Ldrh_Rule_74_A1_P152_cccc000pu1w1nnnnttttiiii1011iiii_Test) {
  Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16IsNot1111_Ldrh_Rule_74_A1_P152 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrh_Rule_74_A1_P152 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w1nnnnttttiiii1011iiii");
}

// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {constraints: ,
//     pattern: 'cccc0001u1011111ttttiiii1011iiii',
//     rule: 'Ldrh_Rule_75_A1_P154'}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrh_Rule_75_A1_P154_cccc0001u1011111ttttiiii1011iiii_Test) {
  Load2RegisterImm8OpTester_op2_6To5Is01_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrh_Rule_75_A1_P154 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrh_Rule_75_A1_P154 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1011111ttttiiii1011iiii");
}

// op2(6:5)=10 & op1(24:20)=xx0x0
//    = Load3RegisterDoubleOp => LoadBasedOffsetMemoryDouble {constraints: ,
//     pattern: 'cccc000pu0w0nnnntttt00001101mmmm',
//     rule: 'Ldrd_Rule_68_A1_P140'}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterDoubleOpTester_op2_6To5Is10_op1_24To20Isxx0x0_Ldrd_Rule_68_A1_P140_cccc000pu0w0nnnntttt00001101mmmm_Test) {
  Load3RegisterDoubleOpTester_op2_6To5Is10_op1_24To20Isxx0x0_Ldrd_Rule_68_A1_P140 baseline_tester;
  NamedLoadBasedOffsetMemoryDouble_Ldrd_Rule_68_A1_P140 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w0nnnntttt00001101mmmm");
}

// op2(6:5)=10 & op1(24:20)=xx0x1
//    = Load3RegisterOp => LoadBasedOffsetMemory {constraints: ,
//     pattern: 'cccc000pu0w1nnnntttt00001101mmmm',
//     rule: 'Ldrsb_Rule_80_A1_P164'}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_op2_6To5Is10_op1_24To20Isxx0x1_Ldrsb_Rule_80_A1_P164_cccc000pu0w1nnnntttt00001101mmmm_Test) {
  Load3RegisterOpTester_op2_6To5Is10_op1_24To20Isxx0x1_Ldrsb_Rule_80_A1_P164 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrsb_Rule_80_A1_P164 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w1nnnntttt00001101mmmm");
}

// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=~1111
//    = Load2RegisterImm8DoubleOp => LoadBasedImmedMemoryDouble {constraints: ,
//     pattern: 'cccc000pu1w0nnnnttttiiii1101iiii',
//     rule: 'Ldrd_Rule_66_A1_P136'}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16IsNot1111_Ldrd_Rule_66_A1_P136_cccc000pu1w0nnnnttttiiii1101iiii_Test) {
  Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16IsNot1111_Ldrd_Rule_66_A1_P136 baseline_tester;
  NamedLoadBasedImmedMemoryDouble_Ldrd_Rule_66_A1_P136 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w0nnnnttttiiii1101iiii");
}

// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=1111
//    = Load2RegisterImm8DoubleOp => LoadBasedImmedMemoryDouble {constraints: ,
//     pattern: 'cccc0001u1001111ttttiiii1101iiii',
//     rule: 'Ldrd_Rule_67_A1_P138'}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16Is1111_Ldrd_Rule_67_A1_P138_cccc0001u1001111ttttiiii1101iiii_Test) {
  Load2RegisterImm8DoubleOpTester_op2_6To5Is10_op1_24To20Isxx1x0_Rn_19To16Is1111_Ldrd_Rule_67_A1_P138 baseline_tester;
  NamedLoadBasedImmedMemoryDouble_Ldrd_Rule_67_A1_P138 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1001111ttttiiii1101iiii");
}

// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {constraints: ,
//     pattern: 'cccc000pu1w1nnnnttttiiii1101iiii',
//     rule: 'Ldrsb_Rule_78_A1_P160'}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16IsNot1111_Ldrsb_Rule_78_A1_P160_cccc000pu1w1nnnnttttiiii1101iiii_Test) {
  Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16IsNot1111_Ldrsb_Rule_78_A1_P160 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrsb_Rule_78_A1_P160 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w1nnnnttttiiii1101iiii");
}

// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {constraints: ,
//     pattern: 'cccc0001u1011111ttttiiii1101iiii',
//     rule: 'ldrsb_Rule_79_A1_162'}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16Is1111_ldrsb_Rule_79_A1_162_cccc0001u1011111ttttiiii1101iiii_Test) {
  Load2RegisterImm8OpTester_op2_6To5Is10_op1_24To20Isxx1x1_Rn_19To16Is1111_ldrsb_Rule_79_A1_162 baseline_tester;
  NamedLoadBasedImmedMemory_ldrsb_Rule_79_A1_162 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1011111ttttiiii1101iiii");
}

// op2(6:5)=11 & op1(24:20)=xx0x0
//    = Store3RegisterDoubleOp => StoreBasedOffsetMemoryDouble {constraints: ,
//     pattern: 'cccc000pu0w0nnnntttt00001111mmmm',
//     rule: 'Strd_Rule_201_A1_P398'}
TEST_F(Arm32DecoderStateTests,
       Store3RegisterDoubleOpTester_op2_6To5Is11_op1_24To20Isxx0x0_Strd_Rule_201_A1_P398_cccc000pu0w0nnnntttt00001111mmmm_Test) {
  Store3RegisterDoubleOpTester_op2_6To5Is11_op1_24To20Isxx0x0_Strd_Rule_201_A1_P398 baseline_tester;
  NamedStoreBasedOffsetMemoryDouble_Strd_Rule_201_A1_P398 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w0nnnntttt00001111mmmm");
}

// op2(6:5)=11 & op1(24:20)=xx0x1
//    = Load3RegisterOp => LoadBasedOffsetMemory {constraints: ,
//     pattern: 'cccc000pu0w1nnnntttt00001111mmmm',
//     rule: 'Ldrsh_Rule_84_A1_P172'}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_op2_6To5Is11_op1_24To20Isxx0x1_Ldrsh_Rule_84_A1_P172_cccc000pu0w1nnnntttt00001111mmmm_Test) {
  Load3RegisterOpTester_op2_6To5Is11_op1_24To20Isxx0x1_Ldrsh_Rule_84_A1_P172 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrsh_Rule_84_A1_P172 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w1nnnntttt00001111mmmm");
}

// op2(6:5)=11 & op1(24:20)=xx1x0
//    = Store2RegisterImm8DoubleOp => StoreBasedImmedMemoryDouble {constraints: ,
//     pattern: 'cccc000pu1w0nnnnttttiiii1111iiii',
//     rule: 'Strd_Rule_200_A1_P396'}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm8DoubleOpTester_op2_6To5Is11_op1_24To20Isxx1x0_Strd_Rule_200_A1_P396_cccc000pu1w0nnnnttttiiii1111iiii_Test) {
  Store2RegisterImm8DoubleOpTester_op2_6To5Is11_op1_24To20Isxx1x0_Strd_Rule_200_A1_P396 baseline_tester;
  NamedStoreBasedImmedMemoryDouble_Strd_Rule_200_A1_P396 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w0nnnnttttiiii1111iiii");
}

// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {constraints: ,
//     pattern: 'cccc000pu1w1nnnnttttiiii1111iiii',
//     rule: 'Ldrsh_Rule_82_A1_P168'}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16IsNot1111_Ldrsh_Rule_82_A1_P168_cccc000pu1w1nnnnttttiiii1111iiii_Test) {
  Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16IsNot1111_Ldrsh_Rule_82_A1_P168 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrsh_Rule_82_A1_P168 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w1nnnnttttiiii1111iiii");
}

// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {constraints: ,
//     pattern: 'cccc0001u1011111ttttiiii1111iiii',
//     rule: 'Ldrsh_Rule_83_A1_P170'}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrsh_Rule_83_A1_P170_cccc0001u1011111ttttiiii1111iiii_Test) {
  Load2RegisterImm8OpTester_op2_6To5Is11_op1_24To20Isxx1x1_Rn_19To16Is1111_Ldrsh_Rule_83_A1_P170 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrsh_Rule_83_A1_P170 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1011111ttttiiii1111iiii");
}

// opc1(23:20)=0x00
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11100d00nnnndddd101snom0mmmm',
//     rule: 'Vmla_vmls_Rule_423_A2_P636'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is0x00_Vmla_vmls_Rule_423_A2_P636_cccc11100d00nnnndddd101snom0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is0x00_Vmla_vmls_Rule_423_A2_P636 baseline_tester;
  NamedVfpOp_Vmla_vmls_Rule_423_A2_P636 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d00nnnndddd101snom0mmmm");
}

// opc1(23:20)=0x01
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11100d01nnnndddd101snom0mmmm',
//     rule: 'Vnmla_vnmls_Rule_343_A1_P674'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is0x01_Vnmla_vnmls_Rule_343_A1_P674_cccc11100d01nnnndddd101snom0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is0x01_Vnmla_vnmls_Rule_343_A1_P674 baseline_tester;
  NamedVfpOp_Vnmla_vnmls_Rule_343_A1_P674 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d01nnnndddd101snom0mmmm");
}

// opc1(23:20)=0x10 & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11100d10nnnndddd101sn1m0mmmm',
//     rule: 'Vnmul_Rule_343_A2_P674'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx1_Vnmul_Rule_343_A2_P674_cccc11100d10nnnndddd101sn1m0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx1_Vnmul_Rule_343_A2_P674 baseline_tester;
  NamedVfpOp_Vnmul_Rule_343_A2_P674 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d10nnnndddd101sn1m0mmmm");
}

// opc1(23:20)=0x10 & opc3(7:6)=x0
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11100d10nnnndddd101sn0m0mmmm',
//     rule: 'Vmul_Rule_338_A2_P664'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx0_Vmul_Rule_338_A2_P664_cccc11100d10nnnndddd101sn0m0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is0x10_opc3_7To6Isx0_Vmul_Rule_338_A2_P664 baseline_tester;
  NamedVfpOp_Vmul_Rule_338_A2_P664 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d10nnnndddd101sn0m0mmmm");
}

// opc1(23:20)=0x11 & opc3(7:6)=x0
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11100d11nnnndddd101sn0m0mmmm',
//     rule: 'Vadd_Rule_271_A2_P536'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx0_Vadd_Rule_271_A2_P536_cccc11100d11nnnndddd101sn0m0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx0_Vadd_Rule_271_A2_P536 baseline_tester;
  NamedVfpOp_Vadd_Rule_271_A2_P536 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d11nnnndddd101sn0m0mmmm");
}

// opc1(23:20)=0x11 & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11100d11nnnndddd101sn1m0mmmm',
//     rule: 'Vsub_Rule_402_A2_P790'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx1_Vsub_Rule_402_A2_P790_cccc11100d11nnnndddd101sn1m0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is0x11_opc3_7To6Isx1_Vsub_Rule_402_A2_P790 baseline_tester;
  NamedVfpOp_Vsub_Rule_402_A2_P790 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d11nnnndddd101sn1m0mmmm");
}

// opc1(23:20)=1x00 & opc3(7:6)=x0
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d00nnnndddd101sn0m0mmmm',
//     rule: 'Vdiv_Rule_301_A1_P590'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is1x00_opc3_7To6Isx0_Vdiv_Rule_301_A1_P590_cccc11101d00nnnndddd101sn0m0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is1x00_opc3_7To6Isx0_Vdiv_Rule_301_A1_P590 baseline_tester;
  NamedVfpOp_Vdiv_Rule_301_A1_P590 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d00nnnndddd101sn0m0mmmm");
}

// opc1(23:20)=1x01
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d01nnnndddd101snom0mmmm',
//     rule: 'Vfnma_vfnms_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is1x01_Vfnma_vfnms_Rule_A1_cccc11101d01nnnndddd101snom0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is1x01_Vfnma_vfnms_Rule_A1 baseline_tester;
  NamedVfpOp_Vfnma_vfnms_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d01nnnndddd101snom0mmmm");
}

// opc1(23:20)=1x10
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d10nnnndddd101snom0mmmm',
//     rule: 'Vfma_vfms_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc1_23To20Is1x10_Vfma_vfms_Rule_A1_cccc11101d10nnnndddd101snom0mmmm_Test) {
  CondVfpOpTester_opc1_23To20Is1x10_Vfma_vfms_Rule_A1 baseline_tester;
  NamedVfpOp_Vfma_vfms_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d10nnnndddd101snom0mmmm");
}

// op1(22:21)=00
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: 'cccc00010000ddddaaaammmm1xx0nnnn',
//     rule: 'Smlaxx_Rule_166_A1_P330',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op1_22To21Is00RegsNotPc_Smlaxx_Rule_166_A1_P330_cccc00010000ddddaaaammmm1xx0nnnn_Test) {
  Binary4RegisterDualOpTester_op1_22To21Is00RegsNotPc_Smlaxx_Rule_166_A1_P330 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010000ddddaaaammmm1xx0nnnn");
}

// op1(22:21)=01 & op(5)=0
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: 'cccc00010010ddddaaaammmm1x00nnnn',
//     rule: 'Smlawx_Rule_171_A1_340',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op1_22To21Is01_op_5Is0RegsNotPc_Smlawx_Rule_171_A1_340_cccc00010010ddddaaaammmm1x00nnnn_Test) {
  Binary4RegisterDualOpTester_op1_22To21Is01_op_5Is0RegsNotPc_Smlawx_Rule_171_A1_340 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlawx_Rule_171_A1_340 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010ddddaaaammmm1x00nnnn");
}

// op1(22:21)=01 & op(5)=1
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc00010010dddd0000mmmm1x10nnnn',
//     rule: 'Smulwx_Rule_180_A1_P358',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op1_22To21Is01_op_5Is1RegsNotPc_Smulwx_Rule_180_A1_P358_cccc00010010dddd0000mmmm1x10nnnn_Test) {
  Binary3RegisterOpAltATester_op1_22To21Is01_op_5Is1RegsNotPc_Smulwx_Rule_180_A1_P358 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010dddd0000mmmm1x10nnnn");
}

// op1(22:21)=10
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc00010100hhhhllllmmmm1xx0nnnn',
//     rule: 'Smlalxx_Rule_169_A1_P336',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op1_22To21Is10RegsNotPc_Smlalxx_Rule_169_A1_P336_cccc00010100hhhhllllmmmm1xx0nnnn_Test) {
  Binary4RegisterDualResultTester_op1_22To21Is10RegsNotPc_Smlalxx_Rule_169_A1_P336 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010100hhhhllllmmmm1xx0nnnn");
}

// op1(22:21)=11
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc00010110dddd0000mmmm1xx0nnnn',
//     rule: 'Smulxx_Rule_178_P354',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op1_22To21Is11RegsNotPc_Smulxx_Rule_178_P354_cccc00010110dddd0000mmmm1xx0nnnn_Test) {
  Binary3RegisterOpAltATester_op1_22To21Is11RegsNotPc_Smulxx_Rule_178_P354 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010110dddd0000mmmm1xx0nnnn");
}

// A(25)=1 & op1(24:20)=xx0x0 & B(4)=0 & op1_repeated(24:20)=~0x010
//    = Store3RegisterImm5Op => StoreBasedOffsetMemory {constraints: ,
//     pattern: 'cccc011pd0w0nnnnttttiiiiitt0mmmm',
//     rule: 'Str_Rule_195_A1_P386'}
TEST_F(Arm32DecoderStateTests,
       Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x0_B_4Is0_op1_repeated_24To20IsNot0x010_Str_Rule_195_A1_P386_cccc011pd0w0nnnnttttiiiiitt0mmmm_Test) {
  Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x0_B_4Is0_op1_repeated_24To20IsNot0x010_Str_Rule_195_A1_P386 baseline_tester;
  NamedStoreBasedOffsetMemory_Str_Rule_195_A1_P386 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pd0w0nnnnttttiiiiitt0mmmm");
}

// A(25)=0 & op1(24:20)=0x010
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc0100u010nnnnttttiiiiiiiiiiii',
//     rule: 'Strt_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x010_Strt_Rule_A1_cccc0100u010nnnnttttiiiiiiiiiiii_Test) {
  ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x010_Strt_Rule_A1 baseline_tester;
  NamedForbidden_Strt_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0100u010nnnnttttiiiiiiiiiiii");
}

// A(25)=1 & op1(24:20)=0x010 & B(4)=0
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc0110u010nnnnttttiiiiitt0mmmm',
//     rule: 'Strt_Rule_A2'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x010_B_4Is0_Strt_Rule_A2_cccc0110u010nnnnttttiiiiitt0mmmm_Test) {
  ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x010_B_4Is0_Strt_Rule_A2 baseline_tester;
  NamedForbidden_Strt_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110u010nnnnttttiiiiitt0mmmm");
}

// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x011
//    = Load2RegisterImm12Op => LoadBasedImmedMemory {constraints: ,
//     pattern: 'cccc010pu0w1nnnnttttiiiiiiiiiiii',
//     rule: 'Ldr_Rule_58_A1_P120',
//     safety: ['NotRnIsPc']}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x011NotRnIsPc_Ldr_Rule_58_A1_P120_cccc010pu0w1nnnnttttiiiiiiiiiiii_Test) {
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x011NotRnIsPc_Ldr_Rule_58_A1_P120 baseline_tester;
  NamedLoadBasedImmedMemory_Ldr_Rule_58_A1_P120 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pu0w1nnnnttttiiiiiiiiiiii");
}

// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x011
//    = Load2RegisterImm12Op => LoadBasedImmedMemory {constraints: ,
//     pattern: 'cccc0101u0011111ttttiiiiiiiiiiii',
//     rule: 'Ldr_Rule_59_A1_P122'}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x011_Ldr_Rule_59_A1_P122_cccc0101u0011111ttttiiiiiiiiiiii_Test) {
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx0x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x011_Ldr_Rule_59_A1_P122 baseline_tester;
  NamedLoadBasedImmedMemory_Ldr_Rule_59_A1_P122 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0101u0011111ttttiiiiiiiiiiii");
}

// A(25)=1 & op1(24:20)=xx0x1 & B(4)=0 & op1_repeated(24:20)=~0x011
//    = Load3RegisterImm5Op => LoadBasedOffsetMemory {constraints: ,
//     pattern: 'cccc011pu0w1nnnnttttiiiiitt0mmmm',
//     rule: 'Ldr_Rule_60_A1_P124'}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x1_B_4Is0_op1_repeated_24To20IsNot0x011_Ldr_Rule_60_A1_P124_cccc011pu0w1nnnnttttiiiiitt0mmmm_Test) {
  Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx0x1_B_4Is0_op1_repeated_24To20IsNot0x011_Ldr_Rule_60_A1_P124 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldr_Rule_60_A1_P124 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pu0w1nnnnttttiiiiitt0mmmm");
}

// A(25)=0 & op1(24:20)=0x011
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc0100u011nnnnttttiiiiiiiiiiii',
//     rule: 'Ldrt_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x011_Ldrt_Rule_A1_cccc0100u011nnnnttttiiiiiiiiiiii_Test) {
  ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x011_Ldrt_Rule_A1 baseline_tester;
  NamedForbidden_Ldrt_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0100u011nnnnttttiiiiiiiiiiii");
}

// A(25)=1 & op1(24:20)=0x011 & B(4)=0
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc0110u011nnnnttttiiiiitt0mmmm',
//     rule: 'Ldrt_Rule_A2'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x011_B_4Is0_Ldrt_Rule_A2_cccc0110u011nnnnttttiiiiitt0mmmm_Test) {
  ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x011_B_4Is0_Ldrt_Rule_A2 baseline_tester;
  NamedForbidden_Ldrt_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110u011nnnnttttiiiiitt0mmmm");
}

// A(25)=0 & op1(24:20)=xx1x0 & op1_repeated(24:20)=~0x110
//    = Store2RegisterImm12Op => StoreBasedImmedMemory {constraints: ,
//     pattern: 'cccc010pu1w0nnnnttttiiiiiiiiiiii',
//     rule: 'Strb_Rule_197_A1_P390'}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x0_op1_repeated_24To20IsNot0x110_Strb_Rule_197_A1_P390_cccc010pu1w0nnnnttttiiiiiiiiiiii_Test) {
  Store2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x0_op1_repeated_24To20IsNot0x110_Strb_Rule_197_A1_P390 baseline_tester;
  NamedStoreBasedImmedMemory_Strb_Rule_197_A1_P390 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pu1w0nnnnttttiiiiiiiiiiii");
}

// A(25)=1 & op1(24:20)=xx1x0 & B(4)=0 & op1_repeated(24:20)=~0x110
//    = Store3RegisterImm5Op => StoreBasedOffsetMemory {constraints: ,
//     pattern: 'cccc011pu1w0nnnnttttiiiiitt0mmmm',
//     rule: 'Strb_Rule_198_A1_P392'}
TEST_F(Arm32DecoderStateTests,
       Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x0_B_4Is0_op1_repeated_24To20IsNot0x110_Strb_Rule_198_A1_P392_cccc011pu1w0nnnnttttiiiiitt0mmmm_Test) {
  Store3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x0_B_4Is0_op1_repeated_24To20IsNot0x110_Strb_Rule_198_A1_P392 baseline_tester;
  NamedStoreBasedOffsetMemory_Strb_Rule_198_A1_P392 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pu1w0nnnnttttiiiiitt0mmmm");
}

// A(25)=0 & op1(24:20)=0x110
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc0100u110nnnnttttiiiiiiiiiiii',
//     rule: 'Strtb_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x110_Strtb_Rule_A1_cccc0100u110nnnnttttiiiiiiiiiiii_Test) {
  ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x110_Strtb_Rule_A1 baseline_tester;
  NamedForbidden_Strtb_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0100u110nnnnttttiiiiiiiiiiii");
}

// A(25)=1 & op1(24:20)=0x110 & B(4)=0
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc0110u110nnnnttttiiiiitt0mmmm',
//     rule: 'Strtb_Rule_A2'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x110_B_4Is0_Strtb_Rule_A2_cccc0110u110nnnnttttiiiiitt0mmmm_Test) {
  ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x110_B_4Is0_Strtb_Rule_A2 baseline_tester;
  NamedForbidden_Strtb_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110u110nnnnttttiiiiitt0mmmm");
}

// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x111
//    = Load2RegisterImm12Op => LoadBasedImmedMemory {constraints: ,
//     pattern: 'cccc010pu1w1nnnnttttiiiiiiiiiiii',
//     rule: 'Ldrb_Rule_62_A1_P128',
//     safety: ['NotRnIsPc']}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x111NotRnIsPc_Ldrb_Rule_62_A1_P128_cccc010pu1w1nnnnttttiiiiiiiiiiii_Test) {
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_Rn_19To16IsNot1111_op1_repeated_24To20IsNot0x111NotRnIsPc_Ldrb_Rule_62_A1_P128 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrb_Rule_62_A1_P128 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pu1w1nnnnttttiiiiiiiiiiii");
}

// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x111
//    = Load2RegisterImm12Op => LoadBasedImmedMemory {constraints: ,
//     pattern: 'cccc0101u1011111ttttiiiiiiiiiiii',
//     rule: 'Ldrb_Rule_63_A1_P130'}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x111_Ldrb_Rule_63_A1_P130_cccc0101u1011111ttttiiiiiiiiiiii_Test) {
  Load2RegisterImm12OpTester_A_25Is0_op1_24To20Isxx1x1_Rn_19To16Is1111_op1_repeated_24To20IsNot0x111_Ldrb_Rule_63_A1_P130 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrb_Rule_63_A1_P130 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0101u1011111ttttiiiiiiiiiiii");
}

// A(25)=1 & op1(24:20)=xx1x1 & B(4)=0 & op1_repeated(24:20)=~0x111
//    = Load3RegisterImm5Op => LoadBasedOffsetMemory {constraints: ,
//     pattern: 'cccc011pu1w1nnnnttttiiiiitt0mmmm',
//     rule: 'Ldrb_Rule_64_A1_P132'}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x1_B_4Is0_op1_repeated_24To20IsNot0x111_Ldrb_Rule_64_A1_P132_cccc011pu1w1nnnnttttiiiiitt0mmmm_Test) {
  Load3RegisterImm5OpTester_A_25Is1_op1_24To20Isxx1x1_B_4Is0_op1_repeated_24To20IsNot0x111_Ldrb_Rule_64_A1_P132 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrb_Rule_64_A1_P132 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pu1w1nnnnttttiiiiitt0mmmm");
}

// A(25)=0 & op1(24:20)=0x111
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc0100u111nnnnttttiiiiiiiiiiii',
//     rule: 'Ldrtb_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x111_Ldrtb_Rule_A1_cccc0100u111nnnnttttiiiiiiiiiiii_Test) {
  ForbiddenCondDecoderTester_A_25Is0_op1_24To20Is0x111_Ldrtb_Rule_A1 baseline_tester;
  NamedForbidden_Ldrtb_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0100u111nnnnttttiiiiiiiiiiii");
}

// A(25)=1 & op1(24:20)=0x111 & B(4)=0
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc0110u111nnnnttttiiiiitt0mmmm',
//     rule: 'Ldrtb_Rule_A2'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x111_B_4Is0_Ldrtb_Rule_A2_cccc0110u111nnnnttttiiiiitt0mmmm_Test) {
  ForbiddenCondDecoderTester_A_25Is1_op1_24To20Is0x111_B_4Is0_Ldrtb_Rule_A2 baseline_tester;
  NamedForbidden_Ldrtb_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110u111nnnnttttiiiiitt0mmmm");
}

// Flags(24:21)=1001 & Rn(19:16)=1101 & Imm12(11:0)=000000000100
//    = Store2RegisterImm12OpRnNotRtOnWriteback => Store2RegisterImm12OpRnNotRtOnWriteback {constraints: ,
//     pattern: 'cccc010100101101tttt000000000100',
//     rule: 'Push_Rule_123_A2_P248'}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm12OpRnNotRtOnWritebackTester_Flags_24To21Is1001_Rn_19To16Is1101_Imm12_11To0Is000000000100_Push_Rule_123_A2_P248_cccc010100101101tttt000000000100_Test) {
  Store2RegisterImm12OpRnNotRtOnWritebackTester_Flags_24To21Is1001_Rn_19To16Is1101_Imm12_11To0Is000000000100_Push_Rule_123_A2_P248 tester;
  tester.Test("cccc010100101101tttt000000000100");
}

// 
//    = Store2RegisterImm12Op => StoreBasedImmedMemory {constraints: & ~cccc010100101101tttt000000000100 ,
//     pattern: 'cccc010pu0w0nnnnttttiiiiiiiiiiii',
//     rule: 'Str_Rule_194_A1_P384'}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm12OpTester_Notcccc010100101101tttt000000000100_Str_Rule_194_A1_P384_cccc010pu0w0nnnnttttiiiiiiiiiiii_Test) {
  Store2RegisterImm12OpTester_Notcccc010100101101tttt000000000100_Str_Rule_194_A1_P384 baseline_tester;
  NamedStoreBasedImmedMemory_Str_Rule_194_A1_P384 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pu0w0nnnnttttiiiiiiiiiiii");
}

// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=1111
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc01111000dddd1111mmmm0001nnnn',
//     rule: 'Usad8_Rule_253_A1_P500',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op1_24To20Is11000_op2_7To5Is000_Rd_15To12Is1111RegsNotPc_Usad8_Rule_253_A1_P500_cccc01111000dddd1111mmmm0001nnnn_Test) {
  Binary3RegisterOpAltATester_op1_24To20Is11000_op2_7To5Is000_Rd_15To12Is1111RegsNotPc_Usad8_Rule_253_A1_P500 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01111000dddd1111mmmm0001nnnn");
}

// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=~1111
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: 'cccc01111000ddddaaaammmm0001nnnn',
//     rule: 'Usada8_Rule_254_A1_P502',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op1_24To20Is11000_op2_7To5Is000_Rd_15To12IsNot1111RegsNotPc_Usada8_Rule_254_A1_P502_cccc01111000ddddaaaammmm0001nnnn_Test) {
  Binary4RegisterDualOpTester_op1_24To20Is11000_op2_7To5Is000_Rd_15To12IsNot1111RegsNotPc_Usada8_Rule_254_A1_P502 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Usada8_Rule_254_A1_P502 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01111000ddddaaaammmm0001nnnn");
}

// op1(24:20)=1101x & op2(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract => Defs12To15CondsDontCareRdRnNotPcBitfieldExtract {constraints: ,
//     pattern: 'cccc0111101wwwwwddddlllll101nnnn',
//     rule: 'Sbfx_Rule_154_A1_P308',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_op1_24To20Is1101x_op2_7To5Isx10RegsNotPc_Sbfx_Rule_154_A1_P308_cccc0111101wwwwwddddlllll101nnnn_Test) {
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_op1_24To20Is1101x_op2_7To5Isx10RegsNotPc_Sbfx_Rule_154_A1_P308 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPcBitfieldExtract_Sbfx_Rule_154_A1_P308 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0111101wwwwwddddlllll101nnnn");
}

// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=1111
//    = Unary1RegisterBitRangeMsbGeLsb => Unary1RegisterBitRangeMsbGeLsb {constraints: ,
//     pattern: 'cccc0111110mmmmmddddlllll0011111',
//     rule: 'Bfc_17_A1_P46',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterBitRangeMsbGeLsbTester_op1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111RegsNotPc_Bfc_17_A1_P46_cccc0111110mmmmmddddlllll0011111_Test) {
  Unary1RegisterBitRangeMsbGeLsbTester_op1_24To20Is1110x_op2_7To5Isx00_Rn_3To0Is1111RegsNotPc_Bfc_17_A1_P46 tester;
  tester.Test("cccc0111110mmmmmddddlllll0011111");
}

// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=~1111
//    = Binary2RegisterBitRangeMsbGeLsb => Defs12To15CondsDontCareMsbGeLsb {constraints: ,
//     pattern: 'cccc0111110mmmmmddddlllll001nnnn',
//     rule: 'Bfi_Rule_18_A1_P48',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterBitRangeMsbGeLsbTester_op1_24To20Is1110x_op2_7To5Isx00_Rn_3To0IsNot1111RegsNotPc_Bfi_Rule_18_A1_P48_cccc0111110mmmmmddddlllll001nnnn_Test) {
  Binary2RegisterBitRangeMsbGeLsbTester_op1_24To20Is1110x_op2_7To5Isx00_Rn_3To0IsNot1111RegsNotPc_Bfi_Rule_18_A1_P48 baseline_tester;
  NamedDefs12To15CondsDontCareMsbGeLsb_Bfi_Rule_18_A1_P48 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0111110mmmmmddddlllll001nnnn");
}

// op1(24:20)=1111x & op2(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract => Defs12To15CondsDontCareRdRnNotPcBitfieldExtract {constraints: ,
//     pattern: 'cccc0111111mmmmmddddlllll101nnnn',
//     rule: 'Ubfx_Rule_236_A1_P466',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_op1_24To20Is1111x_op2_7To5Isx10RegsNotPc_Ubfx_Rule_236_A1_P466_cccc0111111mmmmmddddlllll101nnnn_Test) {
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_op1_24To20Is1111x_op2_7To5Isx10RegsNotPc_Ubfx_Rule_236_A1_P466 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPcBitfieldExtract_Ubfx_Rule_236_A1_P466 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0111111mmmmmddddlllll101nnnn");
}

// op1(24:20)=11111 & op2(7:5)=111
//    = Roadblock => Roadblock {constraints: ,
//     pattern: 'cccc01111111iiiiiiiiiiii1111iiii',
//     rule: 'Udf_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       RoadblockTester_op1_24To20Is11111_op2_7To5Is111_Udf_Rule_A1_cccc01111111iiiiiiiiiiii1111iiii_Test) {
  RoadblockTester_op1_24To20Is11111_op2_7To5Is111_Udf_Rule_A1 tester;
  tester.Test("cccc01111111iiiiiiiiiiii1111iiii");
}

// op1(26:20)=0010000 & op2(7:4)=xx0x & Rn(19:16)=xxx0
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '111100010000iii00000000iii0iiiii',
//     rule: 'Cps_Rule_b6_1_1_A1_B6_3'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_26To20Is0010000_op2_7To4Isxx0x_Rn_19To16Isxxx0_Cps_Rule_b6_1_1_A1_B6_3_111100010000iii00000000iii0iiiii_Test) {
  ForbiddenUncondDecoderTester_op1_26To20Is0010000_op2_7To4Isxx0x_Rn_19To16Isxxx0_Cps_Rule_b6_1_1_A1_B6_3 baseline_tester;
  NamedForbidden_Cps_Rule_b6_1_1_A1_B6_3 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100010000iii00000000iii0iiiii");
}

// op1(26:20)=0010000 & op2(7:4)=0000 & Rn(19:16)=xxx1
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '1111000100000001000000i000000000',
//     rule: 'Setend_Rule_157_P314'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_26To20Is0010000_op2_7To4Is0000_Rn_19To16Isxxx1_Setend_Rule_157_P314_1111000100000001000000i000000000_Test) {
  ForbiddenUncondDecoderTester_op1_26To20Is0010000_op2_7To4Is0000_Rn_19To16Isxxx1_Setend_Rule_157_P314 baseline_tester;
  NamedForbidden_Setend_Rule_157_P314 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111000100000001000000i000000000");
}

// op1(26:20)=100x001
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '11110100x001xxxxxxxxxxxxxxxxxxxx',
//     rule: 'Unallocated_hints'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_26To20Is100x001_Unallocated_hints_11110100x001xxxxxxxxxxxxxxxxxxxx_Test) {
  ForbiddenUncondDecoderTester_op1_26To20Is100x001_Unallocated_hints baseline_tester;
  NamedForbidden_Unallocated_hints actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110100x001xxxxxxxxxxxxxxxxxxxx");
}

// op1(26:20)=100x101
//    = PreloadRegisterImm12Op => DontCareInst {constraints: ,
//     pattern: '11110100u101nnnn1111iiiiiiiiiiii',
//     rule: 'Pli_Rule_120_A1_P242'}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_op1_26To20Is100x101_Pli_Rule_120_A1_P242_11110100u101nnnn1111iiiiiiiiiiii_Test) {
  PreloadRegisterImm12OpTester_op1_26To20Is100x101_Pli_Rule_120_A1_P242 baseline_tester;
  NamedDontCareInst_Pli_Rule_120_A1_P242 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110100u101nnnn1111iiiiiiiiiiii");
}

// op1(26:20)=100xx11
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: '11110100xx11xxxxxxxxxxxxxxxxxxxx',
//     rule: 'Unpredictable'}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_op1_26To20Is100xx11_Unpredictable_11110100xx11xxxxxxxxxxxxxxxxxxxx_Test) {
  UnpredictableUncondDecoderTester_op1_26To20Is100xx11_Unpredictable baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110100xx11xxxxxxxxxxxxxxxxxxxx");
}

// op1(26:20)=101x001 & Rn(19:16)=~1111
//    = PreloadRegisterImm12Op => DontCareInst {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     pattern: '11110101u001nnnn1111iiiiiiiiiiii',
//     rule: 'Pldw_Rule_117_A1_P236'}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_op1_26To20Is101x001_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx_Pldw_Rule_117_A1_P236_11110101u001nnnn1111iiiiiiiiiiii_Test) {
  PreloadRegisterImm12OpTester_op1_26To20Is101x001_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx_Pldw_Rule_117_A1_P236 baseline_tester;
  NamedDontCareInst_Pldw_Rule_117_A1_P236 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110101u001nnnn1111iiiiiiiiiiii");
}

// op1(26:20)=101x001 & Rn(19:16)=1111
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: '11110101x001xxxxxxxxxxxxxxxxxxxx',
//     rule: 'Unpredictable'}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_op1_26To20Is101x001_Rn_19To16Is1111_Unpredictable_11110101x001xxxxxxxxxxxxxxxxxxxx_Test) {
  UnpredictableUncondDecoderTester_op1_26To20Is101x001_Rn_19To16Is1111_Unpredictable baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110101x001xxxxxxxxxxxxxxxxxxxx");
}

// op1(26:20)=101x101 & Rn(19:16)=~1111
//    = PreloadRegisterImm12Op => DontCareInst {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     pattern: '11110101u101nnnn1111iiiiiiiiiiii',
//     rule: 'Pld_Rule_117_A1_P236'}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_op1_26To20Is101x101_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx_Pld_Rule_117_A1_P236_11110101u101nnnn1111iiiiiiiiiiii_Test) {
  PreloadRegisterImm12OpTester_op1_26To20Is101x101_Rn_19To16IsNot1111Notxxxxxxxxxxxx1111xxxxxxxxxxxxxxxx_Pld_Rule_117_A1_P236 baseline_tester;
  NamedDontCareInst_Pld_Rule_117_A1_P236 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110101u101nnnn1111iiiiiiiiiiii");
}

// op1(26:20)=101x101 & Rn(19:16)=1111
//    = PreloadRegisterImm12Op => DontCareInst {constraints: ,
//     pattern: '11110101u10111111111iiiiiiiiiiii',
//     rule: 'Pld_Rule_118_A1_P238'}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_op1_26To20Is101x101_Rn_19To16Is1111_Pld_Rule_118_A1_P238_11110101u10111111111iiiiiiiiiiii_Test) {
  PreloadRegisterImm12OpTester_op1_26To20Is101x101_Rn_19To16Is1111_Pld_Rule_118_A1_P238 baseline_tester;
  NamedDontCareInst_Pld_Rule_118_A1_P238 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110101u10111111111iiiiiiiiiiii");
}

// op1(26:20)=1010011
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: '111101010011xxxxxxxxxxxxxxxxxxxx',
//     rule: 'Unpredictable'}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_op1_26To20Is1010011_Unpredictable_111101010011xxxxxxxxxxxxxxxxxxxx_Test) {
  UnpredictableUncondDecoderTester_op1_26To20Is1010011_Unpredictable baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010011xxxxxxxxxxxxxxxxxxxx");
}

// op1(26:20)=1010111 & op2(7:4)=0000
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: '111101010111xxxxxxxxxxxx0000xxxx',
//     rule: 'Unpredictable'}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is0000_Unpredictable_111101010111xxxxxxxxxxxx0000xxxx_Test) {
  UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is0000_Unpredictable baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010111xxxxxxxxxxxx0000xxxx");
}

// op1(26:20)=1010111 & op2(7:4)=0001
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '11110101011111111111000000011111',
//     rule: 'Clrex_Rule_30_A1_P70'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is0001_Clrex_Rule_30_A1_P70_11110101011111111111000000011111_Test) {
  ForbiddenUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is0001_Clrex_Rule_30_A1_P70 baseline_tester;
  NamedForbidden_Clrex_Rule_30_A1_P70 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110101011111111111000000011111");
}

// op1(26:20)=1010111 & op2(7:4)=001x
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: '111101010111xxxxxxxxxxxx001xxxxx',
//     rule: 'Unpredictable'}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is001x_Unpredictable_111101010111xxxxxxxxxxxx001xxxxx_Test) {
  UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is001x_Unpredictable baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010111xxxxxxxxxxxx001xxxxx");
}

// op1(26:20)=1010111 & op2(7:4)=0100
//    = DataBarrier => DataBarrier {constraints: ,
//     pattern: '1111010101111111111100000100xxxx',
//     rule: 'Dsb_Rule_42_A1_P92'}
TEST_F(Arm32DecoderStateTests,
       DataBarrierTester_op1_26To20Is1010111_op2_7To4Is0100_Dsb_Rule_42_A1_P92_1111010101111111111100000100xxxx_Test) {
  DataBarrierTester_op1_26To20Is1010111_op2_7To4Is0100_Dsb_Rule_42_A1_P92 tester;
  tester.Test("1111010101111111111100000100xxxx");
}

// op1(26:20)=1010111 & op2(7:4)=0101
//    = DataBarrier => DataBarrier {constraints: ,
//     pattern: '1111010101111111111100000101xxxx',
//     rule: 'Dmb_Rule_41_A1_P90'}
TEST_F(Arm32DecoderStateTests,
       DataBarrierTester_op1_26To20Is1010111_op2_7To4Is0101_Dmb_Rule_41_A1_P90_1111010101111111111100000101xxxx_Test) {
  DataBarrierTester_op1_26To20Is1010111_op2_7To4Is0101_Dmb_Rule_41_A1_P90 tester;
  tester.Test("1111010101111111111100000101xxxx");
}

// op1(26:20)=1010111 & op2(7:4)=0110
//    = InstructionBarrier => InstructionBarrier {constraints: ,
//     pattern: '1111010101111111111100000110xxxx',
//     rule: 'Isb_Rule_49_A1_P102'}
TEST_F(Arm32DecoderStateTests,
       InstructionBarrierTester_op1_26To20Is1010111_op2_7To4Is0110_Isb_Rule_49_A1_P102_1111010101111111111100000110xxxx_Test) {
  InstructionBarrierTester_op1_26To20Is1010111_op2_7To4Is0110_Isb_Rule_49_A1_P102 tester;
  tester.Test("1111010101111111111100000110xxxx");
}

// op1(26:20)=1010111 & op2(7:4)=0111
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: '111101010111xxxxxxxxxxxx0111xxxx',
//     rule: 'Unpredictable'}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is0111_Unpredictable_111101010111xxxxxxxxxxxx0111xxxx_Test) {
  UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is0111_Unpredictable baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010111xxxxxxxxxxxx0111xxxx");
}

// op1(26:20)=1010111 & op2(7:4)=1xxx
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: '111101010111xxxxxxxxxxxx1xxxxxxx',
//     rule: 'Unpredictable'}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is1xxx_Unpredictable_111101010111xxxxxxxxxxxx1xxxxxxx_Test) {
  UnpredictableUncondDecoderTester_op1_26To20Is1010111_op2_7To4Is1xxx_Unpredictable baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010111xxxxxxxxxxxx1xxxxxxx");
}

// op1(26:20)=1011x11
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: '111101011x11xxxxxxxxxxxxxxxxxxxx',
//     rule: 'Unpredictable'}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_op1_26To20Is1011x11_Unpredictable_111101011x11xxxxxxxxxxxxxxxxxxxx_Test) {
  UnpredictableUncondDecoderTester_op1_26To20Is1011x11_Unpredictable baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101011x11xxxxxxxxxxxxxxxxxxxx");
}

// op1(26:20)=110x001 & op2(7:4)=xxx0
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '11110110x001xxxxxxxxxxxxxxx0xxxx',
//     rule: 'Unallocated_hints'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_26To20Is110x001_op2_7To4Isxxx0_Unallocated_hints_11110110x001xxxxxxxxxxxxxxx0xxxx_Test) {
  ForbiddenUncondDecoderTester_op1_26To20Is110x001_op2_7To4Isxxx0_Unallocated_hints baseline_tester;
  NamedForbidden_Unallocated_hints actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110110x001xxxxxxxxxxxxxxx0xxxx");
}

// op1(26:20)=110x101 & op2(7:4)=xxx0
//    = PreloadRegisterPairOp => PreloadRegisterPairOp {constraints: ,
//     pattern: '11110110u101nnnn1111iiiiitt0mmmm',
//     rule: 'Pli_Rule_121_A1_P244'}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterPairOpTester_op1_26To20Is110x101_op2_7To4Isxxx0_Pli_Rule_121_A1_P244_11110110u101nnnn1111iiiiitt0mmmm_Test) {
  PreloadRegisterPairOpTester_op1_26To20Is110x101_op2_7To4Isxxx0_Pli_Rule_121_A1_P244 tester;
  tester.Test("11110110u101nnnn1111iiiiitt0mmmm");
}

// op1(26:20)=111x001 & op2(7:4)=xxx0
//    = PreloadRegisterPairOpWAndRnNotPc => PreloadRegisterPairOpWAndRnNotPc {constraints: ,
//     pattern: '11110111u001nnnn1111iiiiitt0mmmm',
//     rule: 'Pldw_Rule_119_A1_P240'}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterPairOpWAndRnNotPcTester_op1_26To20Is111x001_op2_7To4Isxxx0_Pldw_Rule_119_A1_P240_11110111u001nnnn1111iiiiitt0mmmm_Test) {
  PreloadRegisterPairOpWAndRnNotPcTester_op1_26To20Is111x001_op2_7To4Isxxx0_Pldw_Rule_119_A1_P240 tester;
  tester.Test("11110111u001nnnn1111iiiiitt0mmmm");
}

// op1(26:20)=111x101 & op2(7:4)=xxx0
//    = PreloadRegisterPairOpWAndRnNotPc => PreloadRegisterPairOpWAndRnNotPc {constraints: ,
//     pattern: '11110111u101nnnn1111iiiiitt0mmmm',
//     rule: 'Pld_Rule_119_A1_P240'}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterPairOpWAndRnNotPcTester_op1_26To20Is111x101_op2_7To4Isxxx0_Pld_Rule_119_A1_P240_11110111u101nnnn1111iiiiitt0mmmm_Test) {
  PreloadRegisterPairOpWAndRnNotPcTester_op1_26To20Is111x101_op2_7To4Isxxx0_Pld_Rule_119_A1_P240 tester;
  tester.Test("11110111u101nnnn1111iiiiitt0mmmm");
}

// op1(26:20)=11xxx11 & op2(7:4)=xxx0
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: '1111011xxx11xxxxxxxxxxxxxxx0xxxx',
//     rule: 'Unpredictable'}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_op1_26To20Is11xxx11_op2_7To4Isxxx0_Unpredictable_1111011xxx11xxxxxxxxxxxxxxx0xxxx_Test) {
  UnpredictableUncondDecoderTester_op1_26To20Is11xxx11_op2_7To4Isxxx0_Unpredictable baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111011xxx11xxxxxxxxxxxxxxx0xxxx");
}

// op2(6:4)=000 & B(9)=1 & op(22:21)=x0
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc00010r00mmmmdddd001m00000000',
//     rule: 'Msr_Rule_Banked_register_A1_B9_1990'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is1_op_22To21Isx0_Msr_Rule_Banked_register_A1_B9_1990_cccc00010r00mmmmdddd001m00000000_Test) {
  ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is1_op_22To21Isx0_Msr_Rule_Banked_register_A1_B9_1990 baseline_tester;
  NamedForbidden_Msr_Rule_Banked_register_A1_B9_1990 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r00mmmmdddd001m00000000");
}

// op2(6:4)=000 & B(9)=1 & op(22:21)=x1
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc00010r10mmmm1111001m0000nnnn',
//     rule: 'Msr_Rule_Banked_register_A1_B9_1992'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is1_op_22To21Isx1_Msr_Rule_Banked_register_A1_B9_1992_cccc00010r10mmmm1111001m0000nnnn_Test) {
  ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is1_op_22To21Isx1_Msr_Rule_Banked_register_A1_B9_1992 baseline_tester;
  NamedForbidden_Msr_Rule_Banked_register_A1_B9_1992 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r10mmmm1111001m0000nnnn");
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=x0
//    = Unary1RegisterSet => Unary1RegisterSet {constraints: ,
//     pattern: 'cccc00010r001111dddd000000000000',
//     rule: 'Mrs_Rule_102_A1_P206_Or_B6_10'}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterSetTester_op2_6To4Is000_B_9Is0_op_22To21Isx0_Mrs_Rule_102_A1_P206_Or_B6_10_cccc00010r001111dddd000000000000_Test) {
  Unary1RegisterSetTester_op2_6To4Is000_B_9Is0_op_22To21Isx0_Mrs_Rule_102_A1_P206_Or_B6_10 tester;
  tester.Test("cccc00010r001111dddd000000000000");
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00
//    = Unary1RegisterUse => Unary1RegisterUse {constraints: ,
//     pattern: 'cccc00010010mm00111100000000nnnn',
//     rule: 'Msr_Rule_104_A1_P210'}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterUseTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx00_Msr_Rule_104_A1_P210_cccc00010010mm00111100000000nnnn_Test) {
  Unary1RegisterUseTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx00_Msr_Rule_104_A1_P210 tester;
  tester.Test("cccc00010010mm00111100000000nnnn");
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx01
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc00010010mm01111100000000nnnn',
//     rule: 'Msr_Rule_B6_1_7_P14'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01_Msr_Rule_B6_1_7_P14_cccc00010010mm01111100000000nnnn_Test) {
  ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx01_Msr_Rule_B6_1_7_P14 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_7_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010mm01111100000000nnnn");
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx1x
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc00010010mm1m111100000000nnnn',
//     rule: 'Msr_Rule_B6_1_7_P14'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x_Msr_Rule_B6_1_7_P14_cccc00010010mm1m111100000000nnnn_Test) {
  ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is0_op_22To21Is01_op1_19To16Isxx1x_Msr_Rule_B6_1_7_P14 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_7_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010mm1m111100000000nnnn");
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=11
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc00010110mmmm111100000000nnnn',
//     rule: 'Msr_Rule_B6_1_7_P14'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is0_op_22To21Is11_Msr_Rule_B6_1_7_P14_cccc00010110mmmm111100000000nnnn_Test) {
  ForbiddenCondDecoderTester_op2_6To4Is000_B_9Is0_op_22To21Is11_Msr_Rule_B6_1_7_P14 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_7_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010110mmmm111100000000nnnn");
}

// op2(6:4)=001 & op(22:21)=01
//    = BranchToRegister => BxBlx {constraints: ,
//     pattern: 'cccc000100101111111111110001mmmm',
//     rule: 'Bx_Rule_25_A1_P62'}
TEST_F(Arm32DecoderStateTests,
       BranchToRegisterTester_op2_6To4Is001_op_22To21Is01_Bx_Rule_25_A1_P62_cccc000100101111111111110001mmmm_Test) {
  BranchToRegisterTester_op2_6To4Is001_op_22To21Is01_Bx_Rule_25_A1_P62 baseline_tester;
  NamedBxBlx_Bx_Rule_25_A1_P62 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110001mmmm");
}

// op2(6:4)=001 & op(22:21)=11
//    = Unary2RegisterOpNotRmIsPc => Defs12To15RdRnNotPc {constraints: ,
//     pattern: 'cccc000101101111dddd11110001mmmm',
//     rule: 'Clz_Rule_31_A1_P72'}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcTester_op2_6To4Is001_op_22To21Is11_Clz_Rule_31_A1_P72_cccc000101101111dddd11110001mmmm_Test) {
  Unary2RegisterOpNotRmIsPcTester_op2_6To4Is001_op_22To21Is11_Clz_Rule_31_A1_P72 baseline_tester;
  NamedDefs12To15RdRnNotPc_Clz_Rule_31_A1_P72 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000101101111dddd11110001mmmm");
}

// op2(6:4)=010 & op(22:21)=01
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc000100101111111111110010mmmm',
//     rule: 'Bxj_Rule_26_A1_P64'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op2_6To4Is010_op_22To21Is01_Bxj_Rule_26_A1_P64_cccc000100101111111111110010mmmm_Test) {
  ForbiddenCondDecoderTester_op2_6To4Is010_op_22To21Is01_Bxj_Rule_26_A1_P64 baseline_tester;
  NamedForbidden_Bxj_Rule_26_A1_P64 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110010mmmm");
}

// op2(6:4)=011 & op(22:21)=01
//    = BranchToRegister => BxBlx {constraints: ,
//     pattern: 'cccc000100101111111111110011mmmm',
//     rule: 'Blx_Rule_24_A1_P60',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       BranchToRegisterTester_op2_6To4Is011_op_22To21Is01RegsNotPc_Blx_Rule_24_A1_P60_cccc000100101111111111110011mmmm_Test) {
  BranchToRegisterTester_op2_6To4Is011_op_22To21Is01RegsNotPc_Blx_Rule_24_A1_P60 baseline_tester;
  NamedBxBlx_Blx_Rule_24_A1_P60 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110011mmmm");
}

// op2(6:4)=110 & op(22:21)=11
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc0001011000000000000001101110',
//     rule: 'Eret_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op2_6To4Is110_op_22To21Is11_Eret_Rule_A1_cccc0001011000000000000001101110_Test) {
  ForbiddenCondDecoderTester_op2_6To4Is110_op_22To21Is11_Eret_Rule_A1 baseline_tester;
  NamedForbidden_Eret_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001011000000000000001101110");
}

// op2(6:4)=111 & op(22:21)=01
//    = BreakPointAndConstantPoolHead => Breakpoint {constraints: ,
//     pattern: 'cccc00010010iiiiiiiiiiii0111iiii',
//     rule: 'Bkpt_Rule_22_A1_P56'}
TEST_F(Arm32DecoderStateTests,
       BreakPointAndConstantPoolHeadTester_op2_6To4Is111_op_22To21Is01_Bkpt_Rule_22_A1_P56_cccc00010010iiiiiiiiiiii0111iiii_Test) {
  BreakPointAndConstantPoolHeadTester_op2_6To4Is111_op_22To21Is01_Bkpt_Rule_22_A1_P56 baseline_tester;
  NamedBreakpoint_Bkpt_Rule_22_A1_P56 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010iiiiiiiiiiii0111iiii");
}

// op2(6:4)=111 & op(22:21)=10
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc00010100iiiiiiiiiiii0111iiii',
//     rule: 'Hvc_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op2_6To4Is111_op_22To21Is10_Hvc_Rule_A1_cccc00010100iiiiiiiiiiii0111iiii_Test) {
  ForbiddenCondDecoderTester_op2_6To4Is111_op_22To21Is10_Hvc_Rule_A1 baseline_tester;
  NamedForbidden_Hvc_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010100iiiiiiiiiiii0111iiii");
}

// op2(6:4)=111 & op(22:21)=11
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc000101100000000000000111iiii',
//     rule: 'Smc_Rule_B6_1_9_P18'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op2_6To4Is111_op_22To21Is11_Smc_Rule_B6_1_9_P18_cccc000101100000000000000111iiii_Test) {
  ForbiddenCondDecoderTester_op2_6To4Is111_op_22To21Is11_Smc_Rule_B6_1_9_P18 baseline_tester;
  NamedForbidden_Smc_Rule_B6_1_9_P18 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000101100000000000000111iiii");
}

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000000
//    = CondDecoder => DontCareInst {constraints: ,
//     pattern: 'cccc0011001000001111000000000000',
//     rule: 'Nop_Rule_110_A1_P222'}
TEST_F(Arm32DecoderStateTests,
       CondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000000_Nop_Rule_110_A1_P222_cccc0011001000001111000000000000_Test) {
  CondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000000_Nop_Rule_110_A1_P222 baseline_tester;
  NamedDontCareInst_Nop_Rule_110_A1_P222 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000000");
}

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000001
//    = CondDecoder => DontCareInst {constraints: ,
//     pattern: 'cccc0011001000001111000000000001',
//     rule: 'Yield_Rule_413_A1_P812'}
TEST_F(Arm32DecoderStateTests,
       CondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000001_Yield_Rule_413_A1_P812_cccc0011001000001111000000000001_Test) {
  CondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000001_Yield_Rule_413_A1_P812 baseline_tester;
  NamedDontCareInst_Yield_Rule_413_A1_P812 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000001");
}

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000010
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc0011001000001111000000000010',
//     rule: 'Wfe_Rule_411_A1_P808'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000010_Wfe_Rule_411_A1_P808_cccc0011001000001111000000000010_Test) {
  ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000010_Wfe_Rule_411_A1_P808 baseline_tester;
  NamedForbidden_Wfe_Rule_411_A1_P808 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000010");
}

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000011
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc0011001000001111000000000011',
//     rule: 'Wfi_Rule_412_A1_P810'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000011_Wfi_Rule_412_A1_P810_cccc0011001000001111000000000011_Test) {
  ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000011_Wfi_Rule_412_A1_P810 baseline_tester;
  NamedForbidden_Wfi_Rule_412_A1_P810 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000011");
}

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000100
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc0011001000001111000000000100',
//     rule: 'Sev_Rule_158_A1_P316'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000100_Sev_Rule_158_A1_P316_cccc0011001000001111000000000100_Test) {
  ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is00000100_Sev_Rule_158_A1_P316 baseline_tester;
  NamedForbidden_Sev_Rule_158_A1_P316 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000100");
}

// op(22)=0 & op1(19:16)=0000 & op2(7:0)=1111xxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc001100100000111100001111iiii',
//     rule: 'Dbg_Rule_40_A1_P88'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx_Dbg_Rule_40_A1_P88_cccc001100100000111100001111iiii_Test) {
  ForbiddenCondDecoderTester_op_22Is0_op1_19To16Is0000_op2_7To0Is1111xxxx_Dbg_Rule_40_A1_P88 baseline_tester;
  NamedForbidden_Dbg_Rule_40_A1_P88 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001100100000111100001111iiii");
}

// op(22)=0 & op1(19:16)=0100
//    = MoveImmediate12ToApsr => DontCareInst {constraints: ,
//     pattern: 'cccc0011001001001111iiiiiiiiiiii',
//     rule: 'Msr_Rule_103_A1_P208'}
TEST_F(Arm32DecoderStateTests,
       MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is0100_Msr_Rule_103_A1_P208_cccc0011001001001111iiiiiiiiiiii_Test) {
  MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is0100_Msr_Rule_103_A1_P208 baseline_tester;
  NamedDontCareInst_Msr_Rule_103_A1_P208 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001001001111iiiiiiiiiiii");
}

// op(22)=0 & op1(19:16)=1x00
//    = MoveImmediate12ToApsr => DontCareInst {constraints: ,
//     pattern: 'cccc001100101x001111iiiiiiiiiiii',
//     rule: 'Msr_Rule_103_A1_P208'}
TEST_F(Arm32DecoderStateTests,
       MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is1x00_Msr_Rule_103_A1_P208_cccc001100101x001111iiiiiiiiiiii_Test) {
  MoveImmediate12ToApsrTester_op_22Is0_op1_19To16Is1x00_Msr_Rule_103_A1_P208 baseline_tester;
  NamedDontCareInst_Msr_Rule_103_A1_P208 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001100101x001111iiiiiiiiiiii");
}

// op(22)=0 & op1(19:16)=xx01
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc00110010ii011111iiiiiiiiiiii',
//     rule: 'Msr_Rule_B6_1_6_A1_PB6_12'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op_22Is0_op1_19To16Isxx01_Msr_Rule_B6_1_6_A1_PB6_12_cccc00110010ii011111iiiiiiiiiiii_Test) {
  ForbiddenCondDecoderTester_op_22Is0_op1_19To16Isxx01_Msr_Rule_B6_1_6_A1_PB6_12 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110010ii011111iiiiiiiiiiii");
}

// op(22)=0 & op1(19:16)=xx1x
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc00110010ii1i1111iiiiiiiiiiii',
//     rule: 'Msr_Rule_B6_1_6_A1_PB6_12'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op_22Is0_op1_19To16Isxx1x_Msr_Rule_B6_1_6_A1_PB6_12_cccc00110010ii1i1111iiiiiiiiiiii_Test) {
  ForbiddenCondDecoderTester_op_22Is0_op1_19To16Isxx1x_Msr_Rule_B6_1_6_A1_PB6_12 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110010ii1i1111iiiiiiiiiiii");
}

// op(22)=1
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: 'cccc00110110iiii1111iiiiiiiiiiii',
//     rule: 'Msr_Rule_B6_1_6_A1_PB6_12'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_op_22Is1_Msr_Rule_B6_1_6_A1_PB6_12_cccc00110110iiii1111iiiiiiiiiiii_Test) {
  ForbiddenCondDecoderTester_op_22Is1_Msr_Rule_B6_1_6_A1_PB6_12 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110110iiii1111iiiiiiiiiiii");
}

// op(23:20)=000x
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc0000000sdddd0000mmmm1001nnnn',
//     rule: 'Mul_Rule_105_A1_P212',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op_23To20Is000xRegsNotPc_Mul_Rule_105_A1_P212_cccc0000000sdddd0000mmmm1001nnnn_Test) {
  Binary3RegisterOpAltATester_op_23To20Is000xRegsNotPc_Mul_Rule_105_A1_P212 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Mul_Rule_105_A1_P212 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000sdddd0000mmmm1001nnnn");
}

// op(23:20)=001x
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: 'cccc0000001sddddaaaammmm1001nnnn',
//     rule: 'Mla_Rule_94_A1_P190',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op_23To20Is001xRegsNotPc_Mla_Rule_94_A1_P190_cccc0000001sddddaaaammmm1001nnnn_Test) {
  Binary4RegisterDualOpTester_op_23To20Is001xRegsNotPc_Mla_Rule_94_A1_P190 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Mla_Rule_94_A1_P190 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001sddddaaaammmm1001nnnn");
}

// op(23:20)=0100
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc00000100hhhhllllmmmm1001nnnn',
//     rule: 'Umaal_Rule_244_A1_P482',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op_23To20Is0100RegsNotPc_Umaal_Rule_244_A1_P482_cccc00000100hhhhllllmmmm1001nnnn_Test) {
  Binary4RegisterDualResultTester_op_23To20Is0100RegsNotPc_Umaal_Rule_244_A1_P482 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Umaal_Rule_244_A1_P482 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00000100hhhhllllmmmm1001nnnn");
}

// op(23:20)=0101
//    = UndefinedCondDecoder => Undefined {constraints: ,
//     pattern: 'cccc00000101xxxxxxxxxxxx1001xxxx',
//     rule: 'Undefined_A5_2_5_0101'}
TEST_F(Arm32DecoderStateTests,
       UndefinedCondDecoderTester_op_23To20Is0101_Undefined_A5_2_5_0101_cccc00000101xxxxxxxxxxxx1001xxxx_Test) {
  UndefinedCondDecoderTester_op_23To20Is0101_Undefined_A5_2_5_0101 baseline_tester;
  NamedUndefined_Undefined_A5_2_5_0101 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00000101xxxxxxxxxxxx1001xxxx");
}

// op(23:20)=0110
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: 'cccc00000110ddddaaaammmm1001nnnn',
//     rule: 'Mls_Rule_95_A1_P192',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op_23To20Is0110RegsNotPc_Mls_Rule_95_A1_P192_cccc00000110ddddaaaammmm1001nnnn_Test) {
  Binary4RegisterDualOpTester_op_23To20Is0110RegsNotPc_Mls_Rule_95_A1_P192 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Mls_Rule_95_A1_P192 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00000110ddddaaaammmm1001nnnn");
}

// op(23:20)=0111
//    = UndefinedCondDecoder => Undefined {constraints: ,
//     pattern: 'cccc00000111xxxxxxxxxxxx1001xxxx',
//     rule: 'Undefined_A5_2_5_0111'}
TEST_F(Arm32DecoderStateTests,
       UndefinedCondDecoderTester_op_23To20Is0111_Undefined_A5_2_5_0111_cccc00000111xxxxxxxxxxxx1001xxxx_Test) {
  UndefinedCondDecoderTester_op_23To20Is0111_Undefined_A5_2_5_0111 baseline_tester;
  NamedUndefined_Undefined_A5_2_5_0111 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00000111xxxxxxxxxxxx1001xxxx");
}

// op(23:20)=100x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc0000100shhhhllllmmmm1001nnnn',
//     rule: 'Umull_Rule_246_A1_P486',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op_23To20Is100xRegsNotPc_Umull_Rule_246_A1_P486_cccc0000100shhhhllllmmmm1001nnnn_Test) {
  Binary4RegisterDualResultTester_op_23To20Is100xRegsNotPc_Umull_Rule_246_A1_P486 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Umull_Rule_246_A1_P486 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100shhhhllllmmmm1001nnnn");
}

// op(23:20)=101x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc0000101shhhhllllmmmm1001nnnn',
//     rule: 'Umlal_Rule_245_A1_P484',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op_23To20Is101xRegsNotPc_Umlal_Rule_245_A1_P484_cccc0000101shhhhllllmmmm1001nnnn_Test) {
  Binary4RegisterDualResultTester_op_23To20Is101xRegsNotPc_Umlal_Rule_245_A1_P484 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Umlal_Rule_245_A1_P484 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101shhhhllllmmmm1001nnnn");
}

// op(23:20)=110x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc0000110shhhhllllmmmm1001nnnn',
//     rule: 'Smull_Rule_179_A1_P356',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op_23To20Is110xRegsNotPc_Smull_Rule_179_A1_P356_cccc0000110shhhhllllmmmm1001nnnn_Test) {
  Binary4RegisterDualResultTester_op_23To20Is110xRegsNotPc_Smull_Rule_179_A1_P356 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smull_Rule_179_A1_P356 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110shhhhllllmmmm1001nnnn");
}

// op(23:20)=111x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc0000111shhhhllllmmmm1001nnnn',
//     rule: 'Smlal_Rule_168_A1_P334',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op_23To20Is111xRegsNotPc_Smlal_Rule_168_A1_P334_cccc0000111shhhhllllmmmm1001nnnn_Test) {
  Binary4RegisterDualResultTester_op_23To20Is111xRegsNotPc_Smlal_Rule_168_A1_P334 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlal_Rule_168_A1_P334 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111shhhhllllmmmm1001nnnn");
}

// opc3(7:6)=x0
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d11iiiidddd101s0000iiii',
//     rule: 'Vmov_Rule_326_A2_P640'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc3_7To6Isx0_Vmov_Rule_326_A2_P640_cccc11101d11iiiidddd101s0000iiii_Test) {
  CondVfpOpTester_opc3_7To6Isx0_Vmov_Rule_326_A2_P640 baseline_tester;
  NamedVfpOp_Vmov_Rule_326_A2_P640 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11iiiidddd101s0000iiii");
}

// opc2(19:16)=0000 & opc3(7:6)=01
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d110000dddd101s01m0mmmm',
//     rule: 'Vmov_Rule_327_A2_P642'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is0000_opc3_7To6Is01_Vmov_Rule_327_A2_P642_cccc11101d110000dddd101s01m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is0000_opc3_7To6Is01_Vmov_Rule_327_A2_P642 baseline_tester;
  NamedVfpOp_Vmov_Rule_327_A2_P642 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110000dddd101s01m0mmmm");
}

// opc2(19:16)=0000 & opc3(7:6)=11
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d110000dddd101s11m0mmmm',
//     rule: 'Vabs_Rule_269_A2_P532'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is0000_opc3_7To6Is11_Vabs_Rule_269_A2_P532_cccc11101d110000dddd101s11m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is0000_opc3_7To6Is11_Vabs_Rule_269_A2_P532 baseline_tester;
  NamedVfpOp_Vabs_Rule_269_A2_P532 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110000dddd101s11m0mmmm");
}

// opc2(19:16)=0001 & opc3(7:6)=01
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d110001dddd101s01m0mmmm',
//     rule: 'Vneg_Rule_342_A2_P672'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is01_Vneg_Rule_342_A2_P672_cccc11101d110001dddd101s01m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is01_Vneg_Rule_342_A2_P672 baseline_tester;
  NamedVfpOp_Vneg_Rule_342_A2_P672 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110001dddd101s01m0mmmm");
}

// opc2(19:16)=0001 & opc3(7:6)=11
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d110001dddd101s11m0mmmm',
//     rule: 'Vsqrt_Rule_388_A1_P762'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is11_Vsqrt_Rule_388_A1_P762_cccc11101d110001dddd101s11m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is0001_opc3_7To6Is11_Vsqrt_Rule_388_A1_P762 baseline_tester;
  NamedVfpOp_Vsqrt_Rule_388_A1_P762 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110001dddd101s11m0mmmm");
}

// opc2(19:16)=001x & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d11001odddd1010t1m0mmmm',
//     rule: 'Vcvtb_Vcvtt_Rule_300_A1_P588'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is001x_opc3_7To6Isx1_Vcvtb_Vcvtt_Rule_300_A1_P588_cccc11101d11001odddd1010t1m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is001x_opc3_7To6Isx1_Vcvtb_Vcvtt_Rule_300_A1_P588 baseline_tester;
  NamedVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11001odddd1010t1m0mmmm");
}

// opc2(19:16)=0100 & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d110100dddd101se1m0mmmm',
//     rule: 'Vcmp_Vcmpe_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is0100_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_A1_cccc11101d110100dddd101se1m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is0100_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_A1 baseline_tester;
  NamedVfpOp_Vcmp_Vcmpe_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110100dddd101se1m0mmmm");
}

// opc2(19:16)=0101 & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d110101dddd101se1000000',
//     rule: 'Vcmp_Vcmpe_Rule_A2'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is0101_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_A2_cccc11101d110101dddd101se1000000_Test) {
  CondVfpOpTester_opc2_19To16Is0101_opc3_7To6Isx1_Vcmp_Vcmpe_Rule_A2 baseline_tester;
  NamedVfpOp_Vcmp_Vcmpe_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110101dddd101se1000000");
}

// opc2(19:16)=0111 & opc3(7:6)=11
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d110111dddd101s11m0mmmm',
//     rule: 'Vcvt_Rule_298_A1_P584'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is0111_opc3_7To6Is11_Vcvt_Rule_298_A1_P584_cccc11101d110111dddd101s11m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is0111_opc3_7To6Is11_Vcvt_Rule_298_A1_P584 baseline_tester;
  NamedVfpOp_Vcvt_Rule_298_A1_P584 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110111dddd101s11m0mmmm");
}

// opc2(19:16)=1000 & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d111000dddd101sp1m0mmmm',
//     rule: 'Vcvt_Vcvtr_Rule_295_A1_P578'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is1000_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578_cccc11101d111000dddd101sp1m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is1000_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578 baseline_tester;
  NamedVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d111000dddd101sp1m0mmmm");
}

// opc2(19:16)=101x & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d11101udddd101fx1i0iiii',
//     rule: 'Vcvt_Rule_297_A1_P582'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is101x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582_cccc11101d11101udddd101fx1i0iiii_Test) {
  CondVfpOpTester_opc2_19To16Is101x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582 baseline_tester;
  NamedVfpOp_Vcvt_Rule_297_A1_P582 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11101udddd101fx1i0iiii");
}

// opc2(19:16)=110x & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d11110xdddd101sp1m0mmmm',
//     rule: 'Vcvt_Vcvtr_Rule_295_A1_P578'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is110x_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578_cccc11101d11110xdddd101sp1m0mmmm_Test) {
  CondVfpOpTester_opc2_19To16Is110x_opc3_7To6Isx1_Vcvt_Vcvtr_Rule_295_A1_P578 baseline_tester;
  NamedVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11110xdddd101sp1m0mmmm");
}

// opc2(19:16)=111x & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: 'cccc11101d11111udddd101fx1i0iiii',
//     rule: 'Vcvt_Rule_297_A1_P582'}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_opc2_19To16Is111x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582_cccc11101d11111udddd101fx1i0iiii_Test) {
  CondVfpOpTester_opc2_19To16Is111x_opc3_7To6Isx1_Vcvt_Rule_297_A1_P582 baseline_tester;
  NamedVfpOp_Vcvt_Rule_297_A1_P582 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11111udddd101fx1i0iiii");
}

// op1(22:20)=000 & op2(7:5)=xx0
//    = Binary3RegisterImmedShiftedOpRegsNotPc => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01101000nnnnddddiiiiit01mmmm',
//     rule: 'Pkh_Rule_116_A1_P234',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Isxx0RegsNotPc_Pkh_Rule_116_A1_P234_cccc01101000nnnnddddiiiiit01mmmm_Test) {
  Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Isxx0RegsNotPc_Pkh_Rule_116_A1_P234 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Pkh_Rule_116_A1_P234 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101000nnnnddddiiiiit01mmmm");
}

// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterImmedShiftedOpRegsNotPc => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01101000nnnnddddrr000111mmmm',
//     rule: 'Sxtab16_Rule_221_A1_P436',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Sxtab16_Rule_221_A1_P436_cccc01101000nnnnddddrr000111mmmm_Test) {
  Binary3RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Sxtab16_Rule_221_A1_P436 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtab16_Rule_221_A1_P436 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101000nnnnddddrr000111mmmm");
}

// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterImmedShiftedOpRegsNotPc => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: 'cccc011010001111ddddrr000111mmmm',
//     rule: 'Sxtb16_Rule_224_A1_P442',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_A_19To16Is1111RegsNotPc_Sxtb16_Rule_224_A1_P442_cccc011010001111ddddrr000111mmmm_Test) {
  Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is000_op2_7To5Is011_A_19To16Is1111RegsNotPc_Sxtb16_Rule_224_A1_P442 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxtb16_Rule_224_A1_P442 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010001111ddddrr000111mmmm");
}

// op1(22:20)=000 & op2(7:5)=101
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01101000nnnndddd11111011mmmm',
//     rule: 'Sel_Rule_156_A1_P312',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is000_op2_7To5Is101RegsNotPc_Sel_Rule_156_A1_P312_cccc01101000nnnndddd11111011mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is000_op2_7To5Is101RegsNotPc_Sel_Rule_156_A1_P312 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sel_Rule_156_A1_P312 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101000nnnndddd11111011mmmm");
}

// op1(22:20)=01x & op2(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: 'cccc0110101iiiiiddddiiiiis01nnnn',
//     rule: 'Ssat_Rule_183_A1_P362',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is01x_op2_7To5Isxx0RegsNotPc_Ssat_Rule_183_A1_P362_cccc0110101iiiiiddddiiiiis01nnnn_Test) {
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is01x_op2_7To5Isxx0RegsNotPc_Ssat_Rule_183_A1_P362 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Ssat_Rule_183_A1_P362 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110101iiiiiddddiiiiis01nnnn");
}

// op1(22:20)=010 & op2(7:5)=001
//    = Unary2RegisterSatImmedShiftedOp => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: 'cccc01101010iiiidddd11110011nnnn',
//     rule: 'Ssat16_Rule_184_A1_P364',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is010_op2_7To5Is001RegsNotPc_Ssat16_Rule_184_A1_P364_cccc01101010iiiidddd11110011nnnn_Test) {
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is010_op2_7To5Is001RegsNotPc_Ssat16_Rule_184_A1_P364 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Ssat16_Rule_184_A1_P364 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101010iiiidddd11110011nnnn");
}

// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01101010nnnnddddrr000111mmmm',
//     rule: 'Sxtab_Rule_220_A1_P434',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is010_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Sxtab_Rule_220_A1_P434_cccc01101010nnnnddddrr000111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is010_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Sxtab_Rule_220_A1_P434 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtab_Rule_220_A1_P434 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101010nnnnddddrr000111mmmm");
}

// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterImmedShiftedOpRegsNotPc => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: 'cccc011010101111ddddrr000111mmmm',
//     rule: 'Sxtb_Rule_223_A1_P440',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is010_op2_7To5Is011_A_19To16Is1111RegsNotPc_Sxtb_Rule_223_A1_P440_cccc011010101111ddddrr000111mmmm_Test) {
  Unary2RegisterImmedShiftedOpRegsNotPcTester_op1_22To20Is010_op2_7To5Is011_A_19To16Is1111RegsNotPc_Sxtb_Rule_223_A1_P440 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxtb_Rule_223_A1_P440 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010101111ddddrr000111mmmm");
}

// op1(22:20)=011 & op2(7:5)=001
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: 'cccc011010111111dddd11110011mmmm',
//     rule: 'Rev_Rule_135_A1_P272',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is001RegsNotPc_Rev_Rule_135_A1_P272_cccc011010111111dddd11110011mmmm_Test) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is001RegsNotPc_Rev_Rule_135_A1_P272 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Rev_Rule_135_A1_P272 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010111111dddd11110011mmmm");
}

// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01101011nnnnddddrr000111mmmm',
//     rule: 'Sxtah_Rule_222_A1_P438',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Sxtah_Rule_222_A1_P438_cccc01101011nnnnddddrr000111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Sxtah_Rule_222_A1_P438 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtah_Rule_222_A1_P438 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101011nnnnddddrr000111mmmm");
}

// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: 'cccc011010111111ddddrr000111mmmm',
//     rule: 'Sxth_Rule_225_A1_P444',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_A_19To16Is1111RegsNotPc_Sxth_Rule_225_A1_P444_cccc011010111111ddddrr000111mmmm_Test) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is011_A_19To16Is1111RegsNotPc_Sxth_Rule_225_A1_P444 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxth_Rule_225_A1_P444 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010111111ddddrr000111mmmm");
}

// op1(22:20)=011 & op2(7:5)=101
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: 'cccc011010111111dddd11111011mmmm',
//     rule: 'Rev16_Rule_136_A1_P274',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is101RegsNotPc_Rev16_Rule_136_A1_P274_cccc011010111111dddd11111011mmmm_Test) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is011_op2_7To5Is101RegsNotPc_Rev16_Rule_136_A1_P274 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Rev16_Rule_136_A1_P274 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010111111dddd11111011mmmm");
}

// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01101100nnnnddddrr000111mmmm',
//     rule: 'Uxtab16_Rule_262_A1_P516',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is100_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Uxtab16_Rule_262_A1_P516_cccc01101100nnnnddddrr000111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is100_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Uxtab16_Rule_262_A1_P516 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtab16_Rule_262_A1_P516 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101100nnnnddddrr000111mmmm");
}

// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: 'cccc011011001111ddddrr000111mmmm',
//     rule: 'Uxtb16_Rule_264_A1_P522',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is100_op2_7To5Is011_A_19To16Is1111RegsNotPc_Uxtb16_Rule_264_A1_P522_cccc011011001111ddddrr000111mmmm_Test) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is100_op2_7To5Is011_A_19To16Is1111RegsNotPc_Uxtb16_Rule_264_A1_P522 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxtb16_Rule_264_A1_P522 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011001111ddddrr000111mmmm");
}

// op1(22:20)=11x & op2(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: 'cccc0110111iiiiiddddiiiiis01nnnn',
//     rule: 'Usat_Rule_255_A1_P504',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is11x_op2_7To5Isxx0RegsNotPc_Usat_Rule_255_A1_P504_cccc0110111iiiiiddddiiiiis01nnnn_Test) {
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is11x_op2_7To5Isxx0RegsNotPc_Usat_Rule_255_A1_P504 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Usat_Rule_255_A1_P504 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110111iiiiiddddiiiiis01nnnn");
}

// op1(22:20)=110 & op2(7:5)=001
//    = Unary2RegisterSatImmedShiftedOp => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: 'cccc01101110iiiidddd11110011nnnn',
//     rule: 'Usat16_Rule_256_A1_P506',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is110_op2_7To5Is001RegsNotPc_Usat16_Rule_256_A1_P506_cccc01101110iiiidddd11110011nnnn_Test) {
  Unary2RegisterSatImmedShiftedOpTester_op1_22To20Is110_op2_7To5Is001RegsNotPc_Usat16_Rule_256_A1_P506 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Usat16_Rule_256_A1_P506 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101110iiiidddd11110011nnnn");
}

// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01101110nnnnddddrr000111mmmm',
//     rule: 'Uxtab_Rule_260_A1_P514',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is110_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Uxtab_Rule_260_A1_P514_cccc01101110nnnnddddrr000111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is110_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Uxtab_Rule_260_A1_P514 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtab_Rule_260_A1_P514 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101110nnnnddddrr000111mmmm");
}

// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: 'cccc011011101111ddddrr000111mmmm',
//     rule: 'Uxtb_Rule_263_A1_P520',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is110_op2_7To5Is011_A_19To16Is1111RegsNotPc_Uxtb_Rule_263_A1_P520_cccc011011101111ddddrr000111mmmm_Test) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is110_op2_7To5Is011_A_19To16Is1111RegsNotPc_Uxtb_Rule_263_A1_P520 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxtb_Rule_263_A1_P520 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011101111ddddrr000111mmmm");
}

// op1(22:20)=111 & op2(7:5)=001
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: 'cccc011011111111dddd11110011mmmm',
//     rule: 'Rbit_Rule_134_A1_P270',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is001RegsNotPc_Rbit_Rule_134_A1_P270_cccc011011111111dddd11110011mmmm_Test) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is001RegsNotPc_Rbit_Rule_134_A1_P270 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Rbit_Rule_134_A1_P270 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011111111dddd11110011mmmm");
}

// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=~1111
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01101111nnnnddddrr000111mmmm',
//     rule: 'Uxtah_Rule_262_A1_P518',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Uxtah_Rule_262_A1_P518_cccc01101111nnnnddddrr000111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is011_A_19To16IsNot1111RegsNotPc_Uxtah_Rule_262_A1_P518 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtah_Rule_262_A1_P518 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101111nnnnddddrr000111mmmm");
}

// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=1111
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: 'cccc011011111111ddddrr000111mmmm',
//     rule: 'Uxth_Rule_265_A1_P524',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is011_A_19To16Is1111RegsNotPc_Uxth_Rule_265_A1_P524_cccc011011111111ddddrr000111mmmm_Test) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is011_A_19To16Is1111RegsNotPc_Uxth_Rule_265_A1_P524 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxth_Rule_265_A1_P524 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011111111ddddrr000111mmmm");
}

// op1(22:20)=111 & op2(7:5)=101
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: 'cccc011011111111dddd11111011mmmm',
//     rule: 'Revsh_Rule_137_A1_P276',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is101RegsNotPc_Revsh_Rule_137_A1_P276_cccc011011111111dddd11111011mmmm_Test) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_op1_22To20Is111_op2_7To5Is101RegsNotPc_Revsh_Rule_137_A1_P276 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Revsh_Rule_137_A1_P276 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011111111dddd11111011mmmm");
}

// op1(21:20)=01 & op2(7:5)=000
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100001nnnndddd11110001mmmm',
//     rule: 'Sadd16_Rule_148_A1_P296',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is000RegsNotPc_Sadd16_Rule_148_A1_P296_cccc01100001nnnndddd11110001mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is000RegsNotPc_Sadd16_Rule_148_A1_P296 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110001mmmm");
}

// op1(21:20)=01 & op2(7:5)=001
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100001nnnndddd11110011mmmm',
//     rule: 'Sasx_Rule_150_A1_P300',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is001RegsNotPc_Sasx_Rule_150_A1_P300_cccc01100001nnnndddd11110011mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is001RegsNotPc_Sasx_Rule_150_A1_P300 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110011mmmm");
}

// op1(21:20)=01 & op2(7:5)=010
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100001nnnndddd11110101mmmm',
//     rule: 'Ssax_Rule_185_A1_P366',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is010RegsNotPc_Ssax_Rule_185_A1_P366_cccc01100001nnnndddd11110101mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is010RegsNotPc_Ssax_Rule_185_A1_P366 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110101mmmm");
}

// op1(21:20)=01 & op2(7:5)=011
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100001nnnndddd11110111mmmm',
//     rule: 'Ssub16_Rule_186_A1_P368',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is011RegsNotPc_Ssub16_Rule_186_A1_P368_cccc01100001nnnndddd11110111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is011RegsNotPc_Ssub16_Rule_186_A1_P368 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110111mmmm");
}

// op1(21:20)=01 & op2(7:5)=100
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100001nnnndddd11111001mmmm',
//     rule: 'Sadd8_Rule_149_A1_P298',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is100RegsNotPc_Sadd8_Rule_149_A1_P298_cccc01100001nnnndddd11111001mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is100RegsNotPc_Sadd8_Rule_149_A1_P298 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd8_Rule_149_A1_P298 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11111001mmmm");
}

// op1(21:20)=01 & op2(7:5)=111
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100001nnnndddd11111111mmmm',
//     rule: 'Ssub8_Rule_187_A1_P370',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is111RegsNotPc_Ssub8_Rule_187_A1_P370_cccc01100001nnnndddd11111111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is01_op2_7To5Is111RegsNotPc_Ssub8_Rule_187_A1_P370 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11111111mmmm");
}

// op1(21:20)=10 & op2(7:5)=000
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100010nnnndddd11110001mmmm',
//     rule: 'Qadd16_Rule_125_A1_P252',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is000RegsNotPc_Qadd16_Rule_125_A1_P252_cccc01100010nnnndddd11110001mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is000RegsNotPc_Qadd16_Rule_125_A1_P252 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110001mmmm");
}

// op1(21:20)=10 & op2(7:5)=001
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100010nnnndddd11110011mmmm',
//     rule: 'Qasx_Rule_127_A1_P256',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is001RegsNotPc_Qasx_Rule_127_A1_P256_cccc01100010nnnndddd11110011mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is001RegsNotPc_Qasx_Rule_127_A1_P256 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110011mmmm");
}

// op1(21:20)=10 & op2(7:5)=010
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100010nnnndddd11110101mmmm',
//     rule: 'Qsax_Rule_130_A1_P262',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is010RegsNotPc_Qsax_Rule_130_A1_P262_cccc01100010nnnndddd11110101mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is010RegsNotPc_Qsax_Rule_130_A1_P262 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110101mmmm");
}

// op1(21:20)=10 & op2(7:5)=011
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100010nnnndddd11110111mmmm',
//     rule: 'Qsub16_Rule_132_A1_P266',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is011RegsNotPc_Qsub16_Rule_132_A1_P266_cccc01100010nnnndddd11110111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is011RegsNotPc_Qsub16_Rule_132_A1_P266 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110111mmmm");
}

// op1(21:20)=10 & op2(7:5)=100
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100010nnnndddd11111001mmmm',
//     rule: 'Qadd8_Rule_126_A1_P254',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is100RegsNotPc_Qadd8_Rule_126_A1_P254_cccc01100010nnnndddd11111001mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is100RegsNotPc_Qadd8_Rule_126_A1_P254 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11111001mmmm");
}

// op1(21:20)=10 & op2(7:5)=111
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100010nnnndddd11111111mmmm',
//     rule: 'Qsub8_Rule_133_A1_P268',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is111RegsNotPc_Qsub8_Rule_133_A1_P268_cccc01100010nnnndddd11111111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is10_op2_7To5Is111RegsNotPc_Qsub8_Rule_133_A1_P268 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11111111mmmm");
}

// op1(21:20)=11 & op2(7:5)=000
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100011nnnndddd11110001mmmm',
//     rule: 'Shadd16_Rule_159_A1_P318',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is000RegsNotPc_Shadd16_Rule_159_A1_P318_cccc01100011nnnndddd11110001mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is000RegsNotPc_Shadd16_Rule_159_A1_P318 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110001mmmm");
}

// op1(21:20)=11 & op2(7:5)=001
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100011nnnndddd11110011mmmm',
//     rule: 'Shasx_Rule_161_A1_P322',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is001RegsNotPc_Shasx_Rule_161_A1_P322_cccc01100011nnnndddd11110011mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is001RegsNotPc_Shasx_Rule_161_A1_P322 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110011mmmm");
}

// op1(21:20)=11 & op2(7:5)=010
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100011nnnndddd11110101mmmm',
//     rule: 'Shsax_Rule_162_A1_P324',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is010RegsNotPc_Shsax_Rule_162_A1_P324_cccc01100011nnnndddd11110101mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is010RegsNotPc_Shsax_Rule_162_A1_P324 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110101mmmm");
}

// op1(21:20)=11 & op2(7:5)=011
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100011nnnndddd11110111mmmm',
//     rule: 'Shsub16_Rule_163_A1_P326',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is011RegsNotPc_Shsub16_Rule_163_A1_P326_cccc01100011nnnndddd11110111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is011RegsNotPc_Shsub16_Rule_163_A1_P326 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110111mmmm");
}

// op1(21:20)=11 & op2(7:5)=100
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100011nnnndddd11111001mmmm',
//     rule: 'Shadd8_Rule_160_A1_P320',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is100RegsNotPc_Shadd8_Rule_160_A1_P320_cccc01100011nnnndddd11111001mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is100RegsNotPc_Shadd8_Rule_160_A1_P320 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11111001mmmm");
}

// op1(21:20)=11 & op2(7:5)=111
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc01100011nnnndddd11111111mmmm',
//     rule: 'Shsub8_Rule_164_A1_P328',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is111RegsNotPc_Shsub8_Rule_164_A1_P328_cccc01100011nnnndddd11111111mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op1_21To20Is11_op2_7To5Is111RegsNotPc_Shsub8_Rule_164_A1_P328 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11111111mmmm");
}

// op(22:21)=00
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc00010000nnnndddd00000101mmmm',
//     rule: 'Qadd_Rule_124_A1_P250',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is00RegsNotPc_Qadd_Rule_124_A1_P250_cccc00010000nnnndddd00000101mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is00RegsNotPc_Qadd_Rule_124_A1_P250 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd_Rule_124_A1_P250 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010000nnnndddd00000101mmmm");
}

// op(22:21)=01
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc00010010nnnndddd00000101mmmm',
//     rule: 'Qsub_Rule_131_A1_P264',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is01RegsNotPc_Qsub_Rule_131_A1_P264_cccc00010010nnnndddd00000101mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is01RegsNotPc_Qsub_Rule_131_A1_P264 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub_Rule_131_A1_P264 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010nnnndddd00000101mmmm");
}

// op(22:21)=10
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc00010100nnnndddd00000101mmmm',
//     rule: 'Qdadd_Rule_128_A1_P258',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is10RegsNotPc_Qdadd_Rule_128_A1_P258_cccc00010100nnnndddd00000101mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is10RegsNotPc_Qdadd_Rule_128_A1_P258 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdadd_Rule_128_A1_P258 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010100nnnndddd00000101mmmm");
}

// op(22:21)=11
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: 'cccc00010110nnnndddd00000101mmmm',
//     rule: 'Qdsub_Rule_129_A1_P260',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is11RegsNotPc_Qdsub_Rule_129_A1_P260_cccc00010110nnnndddd00000101mmmm_Test) {
  Binary3RegisterOpAltBNoCondUpdatesTester_op_22To21Is11RegsNotPc_Qdsub_Rule_129_A1_P260 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdsub_Rule_129_A1_P260 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010110nnnndddd00000101mmmm");
}

// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=~1111
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: 'cccc01110000ddddaaaammmm00m1nnnn',
//     rule: 'Smlad_Rule_167_P332',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc_Smlad_Rule_167_P332_cccc01110000ddddaaaammmm00m1nnnn_Test) {
  Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc_Smlad_Rule_167_P332 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000ddddaaaammmm00m1nnnn");
}

// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=1111
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc01110000dddd1111mmmm00m1nnnn',
//     rule: 'Smuad_Rule_177_P352',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smuad_Rule_177_P352_cccc01110000dddd1111mmmm00m1nnnn_Test) {
  Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smuad_Rule_177_P352 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000dddd1111mmmm00m1nnnn");
}

// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=~1111
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: 'cccc01110000ddddaaaammmm01m1nnnn',
//     rule: 'Smlsd_Rule_172_P342',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is01x_A_15To12IsNot1111RegsNotPc_Smlsd_Rule_172_P342_cccc01110000ddddaaaammmm01m1nnnn_Test) {
  Binary4RegisterDualOpTester_op1_22To20Is000_op2_7To5Is01x_A_15To12IsNot1111RegsNotPc_Smlsd_Rule_172_P342 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000ddddaaaammmm01m1nnnn");
}

// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=1111
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc01110000dddd1111mmmm01m1nnnn',
//     rule: 'Smusd_Rule_181_P360',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is01x_A_15To12Is1111RegsNotPc_Smusd_Rule_181_P360_cccc01110000dddd1111mmmm01m1nnnn_Test) {
  Binary3RegisterOpAltATester_op1_22To20Is000_op2_7To5Is01x_A_15To12Is1111RegsNotPc_Smusd_Rule_181_P360 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000dddd1111mmmm01m1nnnn");
}

// op1(22:20)=001 & op2(7:5)=000
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc01110001dddd1111mmmm0001nnnn',
//     rule: 'Sdiv_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op1_22To20Is001_op2_7To5Is000_Sdiv_Rule_A1_cccc01110001dddd1111mmmm0001nnnn_Test) {
  Binary3RegisterOpAltATester_op1_22To20Is001_op2_7To5Is000_Sdiv_Rule_A1 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Sdiv_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110001dddd1111mmmm0001nnnn");
}

// op1(22:20)=011 & op2(7:5)=000
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc01110011dddd1111mmmm0001nnnn',
//     rule: 'Udiv_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op1_22To20Is011_op2_7To5Is000_Udiv_Rule_A1_cccc01110011dddd1111mmmm0001nnnn_Test) {
  Binary3RegisterOpAltATester_op1_22To20Is011_op2_7To5Is000_Udiv_Rule_A1 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Udiv_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110011dddd1111mmmm0001nnnn");
}

// op1(22:20)=100 & op2(7:5)=00x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc01110100hhhhllllmmmm00m1nnnn',
//     rule: 'Smlald_Rule_170_P336',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is00xRegsNotPc_Smlald_Rule_170_P336_cccc01110100hhhhllllmmmm00m1nnnn_Test) {
  Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is00xRegsNotPc_Smlald_Rule_170_P336 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110100hhhhllllmmmm00m1nnnn");
}

// op1(22:20)=100 & op2(7:5)=01x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc01110100hhhhllllmmmm01m1nnnn',
//     rule: 'Smlsld_Rule_173_P344',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is01xRegsNotPc_Smlsld_Rule_173_P344_cccc01110100hhhhllllmmmm01m1nnnn_Test) {
  Binary4RegisterDualResultTester_op1_22To20Is100_op2_7To5Is01xRegsNotPc_Smlsld_Rule_173_P344 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110100hhhhllllmmmm01m1nnnn");
}

// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=~1111
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: 'cccc01110101ddddaaaammmm00r1nnnn',
//     rule: 'Smmla_Rule_174_P346',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc_Smmla_Rule_174_P346_cccc01110101ddddaaaammmm00r1nnnn_Test) {
  Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is00x_A_15To12IsNot1111RegsNotPc_Smmla_Rule_174_P346 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110101ddddaaaammmm00r1nnnn");
}

// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=1111
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: 'cccc01110101dddd1111mmmm00r1nnnn',
//     rule: 'Smmul_Rule_176_P350',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_op1_22To20Is101_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smmul_Rule_176_P350_cccc01110101dddd1111mmmm00r1nnnn_Test) {
  Binary3RegisterOpAltATester_op1_22To20Is101_op2_7To5Is00x_A_15To12Is1111RegsNotPc_Smmul_Rule_176_P350 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110101dddd1111mmmm00r1nnnn");
}

// op1(22:20)=101 & op2(7:5)=11x
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: 'cccc01110101ddddaaaammmm11r1nnnn',
//     rule: 'Smmls_Rule_175_P348',
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is11xRegsNotPc_Smmls_Rule_175_P348_cccc01110101ddddaaaammmm11r1nnnn_Test) {
  Binary4RegisterDualOpTester_op1_22To20Is101_op2_7To5Is11xRegsNotPc_Smmls_Rule_175_P348 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110101ddddaaaammmm11r1nnnn");
}

// op(23:20)=0x00
//    = Deprecated => Deprecated {constraints: ,
//     pattern: 'cccc00010b00nnnntttt00001001tttt',
//     rule: 'Swp_Swpb_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       DeprecatedTester_op_23To20Is0x00_Swp_Swpb_Rule_A1_cccc00010b00nnnntttt00001001tttt_Test) {
  DeprecatedTester_op_23To20Is0x00_Swp_Swpb_Rule_A1 tester;
  tester.Test("cccc00010b00nnnntttt00001001tttt");
}

// op(23:20)=1000
//    = StoreExclusive3RegisterOp => StoreBasedMemoryRtBits0To3 {constraints: ,
//     pattern: 'cccc00011000nnnndddd11111001tttt',
//     rule: 'Strex_Rule_202_A1_P400'}
TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterOpTester_op_23To20Is1000_Strex_Rule_202_A1_P400_cccc00011000nnnndddd11111001tttt_Test) {
  StoreExclusive3RegisterOpTester_op_23To20Is1000_Strex_Rule_202_A1_P400 baseline_tester;
  NamedStoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011000nnnndddd11111001tttt");
}

// op(23:20)=1001
//    = LoadExclusive2RegisterOp => LoadBasedMemory {constraints: ,
//     pattern: 'cccc00011001nnnntttt111110011111',
//     rule: 'Ldrex_Rule_69_A1_P142'}
TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterOpTester_op_23To20Is1001_Ldrex_Rule_69_A1_P142_cccc00011001nnnntttt111110011111_Test) {
  LoadExclusive2RegisterOpTester_op_23To20Is1001_Ldrex_Rule_69_A1_P142 baseline_tester;
  NamedLoadBasedMemory_Ldrex_Rule_69_A1_P142 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011001nnnntttt111110011111");
}

// op(23:20)=1010
//    = StoreExclusive3RegisterDoubleOp => StoreBasedMemoryDoubleRtBits0To3 {constraints: ,
//     pattern: 'cccc00011010nnnndddd11111001tttt',
//     rule: 'Strexd_Rule_204_A1_P404'}
TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterDoubleOpTester_op_23To20Is1010_Strexd_Rule_204_A1_P404_cccc00011010nnnndddd11111001tttt_Test) {
  StoreExclusive3RegisterDoubleOpTester_op_23To20Is1010_Strexd_Rule_204_A1_P404 baseline_tester;
  NamedStoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011010nnnndddd11111001tttt");
}

// op(23:20)=1011
//    = LoadExclusive2RegisterDoubleOp => LoadBasedMemoryDouble {constraints: ,
//     pattern: 'cccc00011011nnnntttt111110011111',
//     rule: 'Ldrexd_Rule_71_A1_P146'}
TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterDoubleOpTester_op_23To20Is1011_Ldrexd_Rule_71_A1_P146_cccc00011011nnnntttt111110011111_Test) {
  LoadExclusive2RegisterDoubleOpTester_op_23To20Is1011_Ldrexd_Rule_71_A1_P146 baseline_tester;
  NamedLoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011011nnnntttt111110011111");
}

// op(23:20)=1100
//    = StoreExclusive3RegisterOp => StoreBasedMemoryRtBits0To3 {constraints: ,
//     pattern: 'cccc00011100nnnndddd11111001tttt',
//     rule: 'Strexb_Rule_203_A1_P402'}
TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterOpTester_op_23To20Is1100_Strexb_Rule_203_A1_P402_cccc00011100nnnndddd11111001tttt_Test) {
  StoreExclusive3RegisterOpTester_op_23To20Is1100_Strexb_Rule_203_A1_P402 baseline_tester;
  NamedStoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011100nnnndddd11111001tttt");
}

// op(23:20)=1101
//    = LoadExclusive2RegisterOp => LoadBasedMemory {constraints: ,
//     pattern: 'cccc00011101nnnntttt111110011111',
//     rule: 'Ldrexb_Rule_70_A1_P144'}
TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterOpTester_op_23To20Is1101_Ldrexb_Rule_70_A1_P144_cccc00011101nnnntttt111110011111_Test) {
  LoadExclusive2RegisterOpTester_op_23To20Is1101_Ldrexb_Rule_70_A1_P144 baseline_tester;
  NamedLoadBasedMemory_Ldrexb_Rule_70_A1_P144 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011101nnnntttt111110011111");
}

// op(23:20)=1110
//    = StoreExclusive3RegisterOp => StoreBasedMemoryRtBits0To3 {constraints: ,
//     pattern: 'cccc00011110nnnndddd11111001tttt',
//     rule: 'Strexh_Rule_205_A1_P406'}
TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterOpTester_op_23To20Is1110_Strexh_Rule_205_A1_P406_cccc00011110nnnndddd11111001tttt_Test) {
  StoreExclusive3RegisterOpTester_op_23To20Is1110_Strexh_Rule_205_A1_P406 baseline_tester;
  NamedStoreBasedMemoryRtBits0To3_Strexh_Rule_205_A1_P406 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011110nnnndddd11111001tttt");
}

// op(23:20)=1111
//    = LoadExclusive2RegisterOp => LoadBasedMemory {constraints: ,
//     pattern: 'cccc00011111nnnntttt111110011111',
//     rule: 'Ldrexh_Rule_72_A1_P148'}
TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterOpTester_op_23To20Is1111_Ldrexh_Rule_72_A1_P148_cccc00011111nnnntttt111110011111_Test) {
  LoadExclusive2RegisterOpTester_op_23To20Is1111_Ldrexh_Rule_72_A1_P148 baseline_tester;
  NamedLoadBasedMemory_Ldrexh_Rule_72_A1_P148 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011111nnnntttt111110011111");
}

// L(20)=0 & C(8)=0 & A(23:21)=000
//    = MoveVfpRegisterOp => MoveVfpRegisterOp {constraints: ,
//     pattern: 'cccc11100000nnnntttt1010n0010000',
//     rule: 'Vmov_Rule_330_A1_P648'}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpTester_L_20Is0_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648_cccc11100000nnnntttt1010n0010000_Test) {
  MoveVfpRegisterOpTester_L_20Is0_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648 tester;
  tester.Test("cccc11100000nnnntttt1010n0010000");
}

// L(20)=0 & C(8)=0 & A(23:21)=111
//    = VfpUsesRegOp => DontCareInstRdNotPc {constraints: ,
//     pattern: 'cccc111011100001tttt101000010000',
//     rule: 'Vmsr_Rule_336_A1_P660'}
TEST_F(Arm32DecoderStateTests,
       VfpUsesRegOpTester_L_20Is0_C_8Is0_A_23To21Is111_Vmsr_Rule_336_A1_P660_cccc111011100001tttt101000010000_Test) {
  VfpUsesRegOpTester_L_20Is0_C_8Is0_A_23To21Is111_Vmsr_Rule_336_A1_P660 baseline_tester;
  NamedDontCareInstRdNotPc_Vmsr_Rule_336_A1_P660 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc111011100001tttt101000010000");
}

// L(20)=0 & C(8)=1 & A(23:21)=0xx
//    = MoveVfpRegisterOpWithTypeSel => MoveVfpRegisterOpWithTypeSel {constraints: ,
//     pattern: 'cccc11100ii0ddddtttt1011dii10000',
//     rule: 'Vmov_Rule_328_A1_P644'}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpWithTypeSelTester_L_20Is0_C_8Is1_A_23To21Is0xx_Vmov_Rule_328_A1_P644_cccc11100ii0ddddtttt1011dii10000_Test) {
  MoveVfpRegisterOpWithTypeSelTester_L_20Is0_C_8Is1_A_23To21Is0xx_Vmov_Rule_328_A1_P644 tester;
  tester.Test("cccc11100ii0ddddtttt1011dii10000");
}

// L(20)=0 & C(8)=1 & A(23:21)=1xx & B(6:5)=0x
//    = DuplicateToAdvSIMDRegisters => DuplicateToAdvSIMDRegisters {constraints: ,
//     pattern: 'cccc11101bq0ddddtttt1011d0e10000',
//     rule: 'Vdup_Rule_303_A1_P594'}
TEST_F(Arm32DecoderStateTests,
       DuplicateToAdvSIMDRegistersTester_L_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x_Vdup_Rule_303_A1_P594_cccc11101bq0ddddtttt1011d0e10000_Test) {
  DuplicateToAdvSIMDRegistersTester_L_20Is0_C_8Is1_A_23To21Is1xx_B_6To5Is0x_Vdup_Rule_303_A1_P594 tester;
  tester.Test("cccc11101bq0ddddtttt1011d0e10000");
}

// L(20)=1 & C(8)=0 & A(23:21)=000
//    = MoveVfpRegisterOp => MoveVfpRegisterOp {constraints: ,
//     pattern: 'cccc11100001nnnntttt1010n0010000',
//     rule: 'Vmov_Rule_330_A1_P648'}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpTester_L_20Is1_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648_cccc11100001nnnntttt1010n0010000_Test) {
  MoveVfpRegisterOpTester_L_20Is1_C_8Is0_A_23To21Is000_Vmov_Rule_330_A1_P648 tester;
  tester.Test("cccc11100001nnnntttt1010n0010000");
}

// L(20)=1 & C(8)=0 & A(23:21)=111
//    = VfpMrsOp => VfpMrsOp {constraints: ,
//     pattern: 'cccc111011110001tttt101000010000',
//     rule: 'Vmrs_Rule_335_A1_P658'}
TEST_F(Arm32DecoderStateTests,
       VfpMrsOpTester_L_20Is1_C_8Is0_A_23To21Is111_Vmrs_Rule_335_A1_P658_cccc111011110001tttt101000010000_Test) {
  VfpMrsOpTester_L_20Is1_C_8Is0_A_23To21Is111_Vmrs_Rule_335_A1_P658 tester;
  tester.Test("cccc111011110001tttt101000010000");
}

// L(20)=1 & C(8)=1
//    = MoveVfpRegisterOpWithTypeSel => MoveVfpRegisterOpWithTypeSel {constraints: ,
//     pattern: 'cccc1110iii1nnnntttt1011nii10000',
//     rule: 'Vmov_Rule_329_A1_P646'}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpWithTypeSelTester_L_20Is1_C_8Is1_Vmov_Rule_329_A1_P646_cccc1110iii1nnnntttt1011nii10000_Test) {
  MoveVfpRegisterOpWithTypeSelTester_L_20Is1_C_8Is1_Vmov_Rule_329_A1_P646 tester;
  tester.Test("cccc1110iii1nnnntttt1011nii10000");
}

// C(8)=0 & op(7:4)=00x1
//    = MoveDoubleVfpRegisterOp => MoveDoubleVfpRegisterOp {constraints: ,
//     pattern: 'cccc1100010otttttttt101000m1mmmm',
//     rule: 'Vmov_two_S_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       MoveDoubleVfpRegisterOpTester_C_8Is0_op_7To4Is00x1_Vmov_two_S_Rule_A1_cccc1100010otttttttt101000m1mmmm_Test) {
  MoveDoubleVfpRegisterOpTester_C_8Is0_op_7To4Is00x1_Vmov_two_S_Rule_A1 tester;
  tester.Test("cccc1100010otttttttt101000m1mmmm");
}

// C(8)=1 & op(7:4)=00x1
//    = MoveDoubleVfpRegisterOp => MoveDoubleVfpRegisterOp {constraints: ,
//     pattern: 'cccc1100010otttttttt101100m1mmmm',
//     rule: 'Vmov_one_D_Rule_A1'}
TEST_F(Arm32DecoderStateTests,
       MoveDoubleVfpRegisterOpTester_C_8Is1_op_7To4Is00x1_Vmov_one_D_Rule_A1_cccc1100010otttttttt101100m1mmmm_Test) {
  MoveDoubleVfpRegisterOpTester_C_8Is1_op_7To4Is00x1_Vmov_one_D_Rule_A1 tester;
  tester.Test("cccc1100010otttttttt101100m1mmmm");
}

// op1(27:20)=100xx1x0
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '1111100pu1w0110100000101000iiiii',
//     rule: 'Srs_Rule_B6_1_10_A1_B6_20'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_27To20Is100xx1x0_Srs_Rule_B6_1_10_A1_B6_20_1111100pu1w0110100000101000iiiii_Test) {
  ForbiddenUncondDecoderTester_op1_27To20Is100xx1x0_Srs_Rule_B6_1_10_A1_B6_20 baseline_tester;
  NamedForbidden_Srs_Rule_B6_1_10_A1_B6_20 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111100pu1w0110100000101000iiiii");
}

// op1(27:20)=100xx0x1
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '1111100pu0w1nnnn0000101000000000',
//     rule: 'Rfe_Rule_B6_1_10_A1_B6_16'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_27To20Is100xx0x1_Rfe_Rule_B6_1_10_A1_B6_16_1111100pu0w1nnnn0000101000000000_Test) {
  ForbiddenUncondDecoderTester_op1_27To20Is100xx0x1_Rfe_Rule_B6_1_10_A1_B6_16 baseline_tester;
  NamedForbidden_Rfe_Rule_B6_1_10_A1_B6_16 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111100pu0w1nnnn0000101000000000");
}

// op1(27:20)=101xxxxx
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '1111101hiiiiiiiiiiiiiiiiiiiiiiii',
//     rule: 'Blx_Rule_23_A2_P58'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_27To20Is101xxxxx_Blx_Rule_23_A2_P58_1111101hiiiiiiiiiiiiiiiiiiiiiiii_Test) {
  ForbiddenUncondDecoderTester_op1_27To20Is101xxxxx_Blx_Rule_23_A2_P58 baseline_tester;
  NamedForbidden_Blx_Rule_23_A2_P58 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111101hiiiiiiiiiiiiiiiiiiiiiiii");
}

// op1(27:20)=110xxxx0 & op1_repeated(27:20)=~11000x00
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '1111110pudw0nnnniiiiiiiiiiiiiiii',
//     rule: 'Stc2_Rule_188_A2_P372'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_27To20Is110xxxx0_op1_repeated_27To20IsNot11000x00_Stc2_Rule_188_A2_P372_1111110pudw0nnnniiiiiiiiiiiiiiii_Test) {
  ForbiddenUncondDecoderTester_op1_27To20Is110xxxx0_op1_repeated_27To20IsNot11000x00_Stc2_Rule_188_A2_P372 baseline_tester;
  NamedForbidden_Stc2_Rule_188_A2_P372 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111110pudw0nnnniiiiiiiiiiiiiiii");
}

// op1(27:20)=110xxxx1 & Rn(19:16)=~1111 & op1_repeated(27:20)=~11000x01
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '1111110pudw1nnnniiiiiiiiiiiiiiii',
//     rule: 'Ldc2_Rule_51_A2_P106'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_27To20Is110xxxx1_Rn_19To16IsNot1111_op1_repeated_27To20IsNot11000x01_Ldc2_Rule_51_A2_P106_1111110pudw1nnnniiiiiiiiiiiiiiii_Test) {
  ForbiddenUncondDecoderTester_op1_27To20Is110xxxx1_Rn_19To16IsNot1111_op1_repeated_27To20IsNot11000x01_Ldc2_Rule_51_A2_P106 baseline_tester;
  NamedForbidden_Ldc2_Rule_51_A2_P106 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111110pudw1nnnniiiiiiiiiiiiiiii");
}

// op1(27:20)=110xxxx1 & Rn(19:16)=1111 & op1_repeated(27:20)=~11000x01
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '1111110pudw11111iiiiiiiiiiiiiiii',
//     rule: 'Ldc2_Rule_52_A2_P108'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_27To20Is110xxxx1_Rn_19To16Is1111_op1_repeated_27To20IsNot11000x01_Ldc2_Rule_52_A2_P108_1111110pudw11111iiiiiiiiiiiiiiii_Test) {
  ForbiddenUncondDecoderTester_op1_27To20Is110xxxx1_Rn_19To16Is1111_op1_repeated_27To20IsNot11000x01_Ldc2_Rule_52_A2_P108 baseline_tester;
  NamedForbidden_Ldc2_Rule_52_A2_P108 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111110pudw11111iiiiiiiiiiiiiiii");
}

// op1(27:20)=11000100
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '111111000100ssssttttiiiiiiiiiiii',
//     rule: 'Mcrr2_Rule_93_A2_P188'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_27To20Is11000100_Mcrr2_Rule_93_A2_P188_111111000100ssssttttiiiiiiiiiiii_Test) {
  ForbiddenUncondDecoderTester_op1_27To20Is11000100_Mcrr2_Rule_93_A2_P188 baseline_tester;
  NamedForbidden_Mcrr2_Rule_93_A2_P188 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111111000100ssssttttiiiiiiiiiiii");
}

// op1(27:20)=11000101
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '111111000101ssssttttiiiiiiiiiiii',
//     rule: 'Mrrc2_Rule_101_A2_P204'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_27To20Is11000101_Mrrc2_Rule_101_A2_P204_111111000101ssssttttiiiiiiiiiiii_Test) {
  ForbiddenUncondDecoderTester_op1_27To20Is11000101_Mrrc2_Rule_101_A2_P204 baseline_tester;
  NamedForbidden_Mrrc2_Rule_101_A2_P204 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111111000101ssssttttiiiiiiiiiiii");
}

// op1(27:20)=1110xxxx & op(4)=0
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '11111110iiiiiiiiiiiiiiiiiii0iiii',
//     rule: 'Cdp2_Rule_28_A2_P68'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_27To20Is1110xxxx_op_4Is0_Cdp2_Rule_28_A2_P68_11111110iiiiiiiiiiiiiiiiiii0iiii_Test) {
  ForbiddenUncondDecoderTester_op1_27To20Is1110xxxx_op_4Is0_Cdp2_Rule_28_A2_P68 baseline_tester;
  NamedForbidden_Cdp2_Rule_28_A2_P68 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11111110iiiiiiiiiiiiiiiiiii0iiii");
}

// op1(27:20)=1110xxx0 & op(4)=1
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '11111110iii0iiiittttiiiiiii1iiii',
//     rule: 'Mcr2_Rule_92_A2_P186'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_27To20Is1110xxx0_op_4Is1_Mcr2_Rule_92_A2_P186_11111110iii0iiiittttiiiiiii1iiii_Test) {
  ForbiddenUncondDecoderTester_op1_27To20Is1110xxx0_op_4Is1_Mcr2_Rule_92_A2_P186 baseline_tester;
  NamedForbidden_Mcr2_Rule_92_A2_P186 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11111110iii0iiiittttiiiiiii1iiii");
}

// op1(27:20)=1110xxx1 & op(4)=1
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: '11111110iii1iiiittttiiiiiii1iiii',
//     rule: 'Mrc2_Rule_100_A2_P202'}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_op1_27To20Is1110xxx1_op_4Is1_Mrc2_Rule_100_A2_P202_11111110iii1iiiittttiiiiiii1iiii_Test) {
  ForbiddenUncondDecoderTester_op1_27To20Is1110xxx1_op_4Is1_Mrc2_Rule_100_A2_P202 baseline_tester;
  NamedForbidden_Mrc2_Rule_100_A2_P202 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11111110iii1iiiittttiiiiiii1iiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
