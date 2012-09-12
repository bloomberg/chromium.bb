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
// inst(27:20)=11000100
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(27:20)=11000100
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase0
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase0(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0FF00000) != 0x0C400000 /* op1(27:20)=~11000100 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(27:20)=11000101
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(27:20)=11000101
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase1
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase1(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0FF00000) != 0x0C500000 /* op1(27:20)=~11000101 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=0010000 & inst(7:4)=0000 & inst(19:16)=xxx1 & inst(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=0010000 & op2(7:4)=0000 & Rn(19:16)=xxx1 & $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase2
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase2(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x01000000 /* op1(26:20)=~0010000 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x00000000 /* op2(7:4)=~0000 */) return false;
  if ((inst.Bits() & 0x00010000) != 0x00010000 /* Rn(19:16)=~xxx1 */) return false;
  if ((inst.Bits() & 0x000EFD0F) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx000x000000x0xxxx0000 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=0010000 & inst(7:4)=xx0x & inst(19:16)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=0010000 & op2(7:4)=xx0x & Rn(19:16)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase3
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase3(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x01000000 /* op1(26:20)=~0010000 */) return false;
  if ((inst.Bits() & 0x00000020) != 0x00000000 /* op2(7:4)=~xx0x */) return false;
  if ((inst.Bits() & 0x00010000) != 0x00000000 /* Rn(19:16)=~xxx0 */) return false;
  if ((inst.Bits() & 0x0000FE00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000000xxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=1010011
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=1010011
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase4
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase4(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x05300000 /* op1(26:20)=~1010011 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0000
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0000
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase5
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase5(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase5
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

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0001 & inst(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0001 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase6
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase6(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x05700000 /* op1(26:20)=~1010111 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x00000010 /* op2(7:4)=~0001 */) return false;
  if ((inst.Bits() & 0x000FFF0F) != 0x000FF00F /* $pattern(31:0)=~xxxxxxxxxxxx111111110000xxxx1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0100 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0100 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier {constraints: }
class DataBarrierTesterCase7
    : public DataBarrierTester {
 public:
  DataBarrierTesterCase7(const NamedClassDecoder& decoder)
    : DataBarrierTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool DataBarrierTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x05700000 /* op1(26:20)=~1010111 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x00000040 /* op2(7:4)=~0100 */) return false;
  if ((inst.Bits() & 0x000FFF00) != 0x000FF000 /* $pattern(31:0)=~xxxxxxxxxxxx111111110000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return DataBarrierTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0101 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0101 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier {constraints: }
class DataBarrierTesterCase8
    : public DataBarrierTester {
 public:
  DataBarrierTesterCase8(const NamedClassDecoder& decoder)
    : DataBarrierTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool DataBarrierTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x05700000 /* op1(26:20)=~1010111 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x00000050 /* op2(7:4)=~0101 */) return false;
  if ((inst.Bits() & 0x000FFF00) != 0x000FF000 /* $pattern(31:0)=~xxxxxxxxxxxx111111110000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return DataBarrierTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0110 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = InstructionBarrier {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0110 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = InstructionBarrier {constraints: }
class InstructionBarrierTesterCase9
    : public InstructionBarrierTester {
 public:
  InstructionBarrierTesterCase9(const NamedClassDecoder& decoder)
    : InstructionBarrierTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool InstructionBarrierTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07F00000) != 0x05700000 /* op1(26:20)=~1010111 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x00000060 /* op2(7:4)=~0110 */) return false;
  if ((inst.Bits() & 0x000FFF00) != 0x000FF000 /* $pattern(31:0)=~xxxxxxxxxxxx111111110000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return InstructionBarrierTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0111
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0111
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase10
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase10(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase10
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

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=001x
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=001x
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase11
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase11(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase11
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

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=1xxx
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=1xxx
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase12
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase12(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase12
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

// Neutral case:
// inst(25:20)=001001
//    = LoadRegisterList {'constraints': }
//
// Representaive case:
// op(25:20)=001001
//    = LoadRegisterList {constraints: }
class LoadStoreRegisterListTesterCase13
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase13(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase13
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
class LoadStoreRegisterListTesterCase14
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase14(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase14
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
class LoadStoreRegisterListTesterCase15
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase15(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase15
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
class LoadStoreRegisterListTesterCase16
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase16(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase16
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
class LoadStoreRegisterListTesterCase17
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase17(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase17
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
class LoadStoreRegisterListTesterCase18
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase18(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase18
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
// inst(26:20)=100x001
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=100x001
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase19
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase19(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x04100000 /* op1(26:20)=~100x001 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=100x101 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {'constraints': }
//
// Representaive case:
// op1(26:20)=100x101 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: }
class PreloadRegisterImm12OpTesterCase20
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterCase20(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterCase20
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x04500000 /* op1(26:20)=~100x101 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=101x001 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {'constraints': & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx }
//
// Representaive case:
// op1(26:20)=101x001 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx }
class PreloadRegisterImm12OpTesterCase21
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterCase21(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterCase21
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x05100000 /* op1(26:20)=~101x001 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check pattern restrictions of row.
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* constraint(31:0)=xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=101x001 & inst(19:16)=1111
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=101x001 & Rn(19:16)=1111
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase22
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase22(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase22
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

// Neutral case:
// inst(26:20)=101x101 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {'constraints': & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx }
//
// Representaive case:
// op1(26:20)=101x101 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx }
class PreloadRegisterImm12OpTesterCase23
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterCase23(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterCase23
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x05500000 /* op1(26:20)=~101x101 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check pattern restrictions of row.
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* constraint(31:0)=xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=101x101 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {'constraints': }
//
// Representaive case:
// op1(26:20)=101x101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: }
class PreloadRegisterImm12OpTesterCase24
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterCase24(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterCase24
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x05500000 /* op1(26:20)=~101x101 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=110x001 & inst(7:4)=xxx0
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=110x001 & op2(7:4)=xxx0
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase25
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase25(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase25
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

// Neutral case:
// inst(26:20)=110x101 & inst(7:4)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOp {'constraints': }
//
// Representaive case:
// op1(26:20)=110x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOp {constraints: }
class PreloadRegisterPairOpTesterCase26
    : public PreloadRegisterPairOpTester {
 public:
  PreloadRegisterPairOpTesterCase26(const NamedClassDecoder& decoder)
    : PreloadRegisterPairOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterPairOpTesterCase26
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x06500000 /* op1(26:20)=~110x101 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* op2(7:4)=~xxx0 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterPairOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=111x001 & inst(7:4)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc {'constraints': }
//
// Representaive case:
// op1(26:20)=111x001 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc {constraints: }
class PreloadRegisterPairOpWAndRnNotPcTesterCase27
    : public PreloadRegisterPairOpWAndRnNotPcTester {
 public:
  PreloadRegisterPairOpWAndRnNotPcTesterCase27(const NamedClassDecoder& decoder)
    : PreloadRegisterPairOpWAndRnNotPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterPairOpWAndRnNotPcTesterCase27
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x07100000 /* op1(26:20)=~111x001 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* op2(7:4)=~xxx0 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterPairOpWAndRnNotPcTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=111x101 & inst(7:4)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc {'constraints': }
//
// Representaive case:
// op1(26:20)=111x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc {constraints: }
class PreloadRegisterPairOpWAndRnNotPcTesterCase28
    : public PreloadRegisterPairOpWAndRnNotPcTester {
 public:
  PreloadRegisterPairOpWAndRnNotPcTesterCase28(const NamedClassDecoder& decoder)
    : PreloadRegisterPairOpWAndRnNotPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterPairOpWAndRnNotPcTesterCase28
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07700000) != 0x07500000 /* op1(26:20)=~111x101 */) return false;
  if ((inst.Bits() & 0x00000010) != 0x00000000 /* op2(7:4)=~xxx0 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterPairOpWAndRnNotPcTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=1011x11
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=1011x11
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase29
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase29(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase29
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07B00000) != 0x05B00000 /* op1(26:20)=~1011x11 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=00100 & inst(19:16)=1111
//    = Unary1RegisterImmediateOp {'constraints': }
//
// Representaive case:
// op(24:20)=00100 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp {constraints: }
class Unary1RegisterImmediateOpTesterCase30
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTesterCase30(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase30
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

// Neutral case:
// inst(24:20)=00101 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(24:20)=00101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase31
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase31(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase31
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x00500000 /* op(24:20)=~00101 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=01000 & inst(19:16)=1111
//    = Unary1RegisterImmediateOp {'constraints': }
//
// Representaive case:
// op(24:20)=01000 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp {constraints: }
class Unary1RegisterImmediateOpTesterCase32
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTesterCase32(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase32
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

// Neutral case:
// inst(24:20)=01001 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(24:20)=01001 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase33
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase33(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase33
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x00900000 /* op(24:20)=~01001 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': }
//
// Representaive case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: }
class Binary2RegisterImmedShiftedTestTesterCase34
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase34(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase34
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01100000 /* op1(24:20)=~10001 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = MaskedBinaryRegisterImmediateTest {'constraints': }
//
// Representaive case:
// op(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = MaskedBinaryRegisterImmediateTest {constraints: }
class BinaryRegisterImmediateTestTesterCase35
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterCase35(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BinaryRegisterImmediateTestTesterCase35
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01100000 /* op(24:20)=~10001 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTesterCase36
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTesterCase36(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedTestTesterCase36
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01100000 /* op1(24:20)=~10001 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': }
//
// Representaive case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: }
class Binary2RegisterImmedShiftedTestTesterCase37
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase37(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase37
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01300000 /* op1(24:20)=~10011 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {'constraints': }
//
// Representaive case:
// op(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {constraints: }
class BinaryRegisterImmediateTestTesterCase38
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterCase38(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BinaryRegisterImmediateTestTesterCase38
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01300000 /* op(24:20)=~10011 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTesterCase39
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTesterCase39(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedTestTesterCase39
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01300000 /* op1(24:20)=~10011 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': }
//
// Representaive case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: }
class Binary2RegisterImmedShiftedTestTesterCase40
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase40(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase40
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01500000 /* op1(24:20)=~10101 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {'constraints': }
//
// Representaive case:
// op(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {constraints: }
class BinaryRegisterImmediateTestTesterCase41
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterCase41(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BinaryRegisterImmediateTestTesterCase41
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01500000 /* op(24:20)=~10101 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTesterCase42
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTesterCase42(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedTestTesterCase42
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01500000 /* op1(24:20)=~10101 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': }
//
// Representaive case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: }
class Binary2RegisterImmedShiftedTestTesterCase43
    : public Binary2RegisterImmedShiftedTestTester {
 public:
  Binary2RegisterImmedShiftedTestTesterCase43(const NamedClassDecoder& decoder)
    : Binary2RegisterImmedShiftedTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmedShiftedTestTesterCase43
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01700000 /* op1(24:20)=~10111 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmedShiftedTestTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {'constraints': }
//
// Representaive case:
// op(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {constraints: }
class BinaryRegisterImmediateTestTesterCase44
    : public BinaryRegisterImmediateTestTester {
 public:
  BinaryRegisterImmediateTestTesterCase44(const NamedClassDecoder& decoder)
    : BinaryRegisterImmediateTestTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BinaryRegisterImmediateTestTesterCase44
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01700000 /* op(24:20)=~10111 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BinaryRegisterImmediateTestTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTesterCase45
    : public Binary3RegisterShiftedTestTesterRegsNotPc {
 public:
  Binary3RegisterShiftedTestTesterCase45(const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterShiftedTestTesterCase45
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01F00000) != 0x01700000 /* op1(24:20)=~10111 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterShiftedTestTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=11000 & inst(7:5)=000 & inst(15:12)=~1111
//    = Binary4RegisterDualOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterCase46
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase46(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase46
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

// Neutral case:
// inst(24:20)=11000 & inst(7:5)=000 & inst(15:12)=1111
//    = Binary3RegisterOpAltA {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATesterCase47
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase47(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase47
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

// Neutral case:
// inst(24:20)=11111 & inst(7:5)=111
//    = Roadblock {'constraints': }
//
// Representaive case:
// op1(24:20)=11111 & op2(7:5)=111
//    = Roadblock {constraints: }
class RoadblockTesterCase48
    : public RoadblockTester {
 public:
  RoadblockTesterCase48(const NamedClassDecoder& decoder)
    : RoadblockTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool RoadblockTesterCase48
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

// Neutral case:
// inst(25:20)=0000x0
//    = StoreRegisterList {'constraints': }
//
// Representaive case:
// op(25:20)=0000x0
//    = StoreRegisterList {constraints: }
class LoadStoreRegisterListTesterCase49
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase49(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase49
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
class LoadStoreRegisterListTesterCase50
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase50(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase50
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
class LoadStoreRegisterListTesterCase51
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase51(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase51
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
class LoadStoreRegisterListTesterCase52
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase52(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase52
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
class LoadStoreRegisterListTesterCase53
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase53(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase53
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
class LoadStoreRegisterListTesterCase54
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase54(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase54
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
// inst(26:20)=100xx11
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=100xx11
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase55
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase55(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase55
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x07300000) != 0x04300000 /* op1(26:20)=~100xx11 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(27:20)=100xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxx0000101000000000
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(27:20)=100xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000101000000000
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase56
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase56(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase56
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E500000) != 0x08100000 /* op1(27:20)=~100xx0x1 */) return false;
  if ((inst.Bits() & 0x0000FFFF) != 0x00000A00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx0000101000000000 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(27:20)=100xx1x0 & inst(31:0)=xxxxxxxxxxxx110100000101000xxxxx
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(27:20)=100xx1x0 & $pattern(31:0)=xxxxxxxxxxxx110100000101000xxxxx
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase57
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase57(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase57
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E500000) != 0x08400000 /* op1(27:20)=~100xx1x0 */) return false;
  if ((inst.Bits() & 0x000FFFE0) != 0x000D0500 /* $pattern(31:0)=~xxxxxxxxxxxx110100000101000xxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(27:20)=1110xxx0 & inst(4)=1
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(27:20)=1110xxx0 & op(4)=1
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase58
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase58(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase58
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

// Neutral case:
// inst(27:20)=1110xxx1 & inst(4)=1
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(27:20)=1110xxx1 & op(4)=1
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase59
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase59(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase59
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

// Neutral case:
// inst(19:16)=0000 & inst(7:6)=01
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc2(19:16)=0000 & opc3(7:6)=01
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase60
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase60(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase60
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

// Neutral case:
// inst(19:16)=0000 & inst(7:6)=11
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc2(19:16)=0000 & opc3(7:6)=11
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase61
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase61(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase61
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

// Neutral case:
// inst(19:16)=0001 & inst(7:6)=01
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc2(19:16)=0001 & opc3(7:6)=01
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase62
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase62(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase62
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

// Neutral case:
// inst(19:16)=0001 & inst(7:6)=11
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc2(19:16)=0001 & opc3(7:6)=11
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase63
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase63(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase63
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

// Neutral case:
// inst(19:16)=0100 & inst(7:6)=x1
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc2(19:16)=0100 & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase64
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase64(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase64
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

// Neutral case:
// inst(19:16)=0101 & inst(7:6)=x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc2(19:16)=0101 & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase65
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase65(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase65
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x000F0000) != 0x00050000 /* opc2(19:16)=~0101 */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6)=~x1 */) return false;
  if ((inst.Bits() & 0x0000002F) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(19:16)=0111 & inst(7:6)=11
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc2(19:16)=0111 & opc3(7:6)=11
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase66
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase66(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase66
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

// Neutral case:
// inst(19:16)=1000 & inst(7:6)=x1
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc2(19:16)=1000 & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase67
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase67(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase67
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

// Neutral case:
// inst(23:20)=0100
//    = Binary4RegisterDualResult {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op(23:20)=0100
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTesterCase68
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterCase68(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase68
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
class UnsafeCondDecoderTesterCase69
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase69(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase69
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
class Binary4RegisterDualOpTesterCase70
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase70(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase70
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
class UnsafeCondDecoderTesterCase71
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase71(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase71
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
// inst(23:20)=1000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp {'constraints': }
//
// Representaive case:
// op(23:20)=1000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp {constraints: }
class StoreExclusive3RegisterOpTesterCase72
    : public StoreExclusive3RegisterOpTester {
 public:
  StoreExclusive3RegisterOpTesterCase72(const NamedClassDecoder& decoder)
    : StoreExclusive3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreExclusive3RegisterOpTesterCase72
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
//    = LoadExclusive2RegisterOp {'constraints': }
//
// Representaive case:
// op(23:20)=1001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterOp {constraints: }
class LoadExclusive2RegisterOpTesterCase73
    : public LoadExclusive2RegisterOpTester {
 public:
  LoadExclusive2RegisterOpTesterCase73(const NamedClassDecoder& decoder)
    : LoadExclusive2RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadExclusive2RegisterOpTesterCase73
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
//    = StoreExclusive3RegisterDoubleOp {'constraints': }
//
// Representaive case:
// op(23:20)=1010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterDoubleOp {constraints: }
class StoreExclusive3RegisterDoubleOpTesterCase74
    : public StoreExclusive3RegisterDoubleOpTester {
 public:
  StoreExclusive3RegisterDoubleOpTesterCase74(const NamedClassDecoder& decoder)
    : StoreExclusive3RegisterDoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreExclusive3RegisterDoubleOpTesterCase74
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
//    = LoadExclusive2RegisterDoubleOp {'constraints': }
//
// Representaive case:
// op(23:20)=1011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterDoubleOp {constraints: }
class LoadExclusive2RegisterDoubleOpTesterCase75
    : public LoadExclusive2RegisterDoubleOpTester {
 public:
  LoadExclusive2RegisterDoubleOpTesterCase75(const NamedClassDecoder& decoder)
    : LoadExclusive2RegisterDoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadExclusive2RegisterDoubleOpTesterCase75
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
//    = StoreExclusive3RegisterOp {'constraints': }
//
// Representaive case:
// op(23:20)=1100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp {constraints: }
class StoreExclusive3RegisterOpTesterCase76
    : public StoreExclusive3RegisterOpTester {
 public:
  StoreExclusive3RegisterOpTesterCase76(const NamedClassDecoder& decoder)
    : StoreExclusive3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreExclusive3RegisterOpTesterCase76
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
//    = LoadExclusive2RegisterOp {'constraints': }
//
// Representaive case:
// op(23:20)=1101 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterOp {constraints: }
class LoadExclusive2RegisterOpTesterCase77
    : public LoadExclusive2RegisterOpTester {
 public:
  LoadExclusive2RegisterOpTesterCase77(const NamedClassDecoder& decoder)
    : LoadExclusive2RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadExclusive2RegisterOpTesterCase77
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
//    = StoreExclusive3RegisterOp {'constraints': }
//
// Representaive case:
// op(23:20)=1110 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp {constraints: }
class StoreExclusive3RegisterOpTesterCase78
    : public StoreExclusive3RegisterOpTester {
 public:
  StoreExclusive3RegisterOpTesterCase78(const NamedClassDecoder& decoder)
    : StoreExclusive3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreExclusive3RegisterOpTesterCase78
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
//    = LoadExclusive2RegisterOp {'constraints': }
//
// Representaive case:
// op(23:20)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterOp {constraints: }
class LoadExclusive2RegisterOpTesterCase79
    : public LoadExclusive2RegisterOpTester {
 public:
  LoadExclusive2RegisterOpTesterCase79(const NamedClassDecoder& decoder)
    : LoadExclusive2RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadExclusive2RegisterOpTesterCase79
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
// inst(24:20)=01x00
//    = StoreVectorRegisterList {'constraints': }
//
// Representaive case:
// opcode(24:20)=01x00
//    = StoreVectorRegisterList {constraints: }
class StoreVectorRegisterListTesterCase80
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesterCase80(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase80
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x00800000 /* opcode(24:20)=~01x00 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=01x01
//    = LoadVectorRegisterList {'constraints': }
//
// Representaive case:
// opcode(24:20)=01x01
//    = LoadVectorRegisterList {constraints: }
class LoadStoreVectorRegisterListTesterCase81
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesterCase81(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase81
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x00900000 /* opcode(24:20)=~01x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=01x10
//    = StoreVectorRegisterList {'constraints': }
//
// Representaive case:
// opcode(24:20)=01x10
//    = StoreVectorRegisterList {constraints: }
class StoreVectorRegisterListTesterCase82
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesterCase82(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase82
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x00A00000 /* opcode(24:20)=~01x10 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=~1101
//    = LoadVectorRegisterList {'constraints': ,
//     'safety': ["'NotRnIsSp'"]}
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=~1101
//    = LoadVectorRegisterList {constraints: ,
//     safety: ['NotRnIsSp']}
class LoadStoreVectorRegisterListTesterCase83
    : public LoadStoreVectorRegisterListTesterNotRnIsSp {
 public:
  LoadStoreVectorRegisterListTesterCase83(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTesterNotRnIsSp(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase83
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

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=1101
//    = LoadVectorRegisterList {'constraints': }
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=1101
//    = LoadVectorRegisterList {constraints: }
class LoadStoreVectorRegisterListTesterCase84
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesterCase84(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase84
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

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=~1101
//    = StoreVectorRegisterList {'constraints': ,
//     'safety': ["'NotRnIsSp'"]}
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=~1101
//    = StoreVectorRegisterList {constraints: ,
//     safety: ['NotRnIsSp']}
class StoreVectorRegisterListTesterCase85
    : public StoreVectorRegisterListTesterNotRnIsSp {
 public:
  StoreVectorRegisterListTesterCase85(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTesterNotRnIsSp(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase85
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

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=1101
//    = StoreVectorRegisterList {'constraints': }
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=1101
//    = StoreVectorRegisterList {constraints: }
class StoreVectorRegisterListTesterCase86
    : public StoreVectorRegisterListTester {
 public:
  StoreVectorRegisterListTesterCase86(const NamedClassDecoder& decoder)
    : StoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterListTesterCase86
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

// Neutral case:
// inst(24:20)=10x11
//    = LoadVectorRegisterList {'constraints': }
//
// Representaive case:
// opcode(24:20)=10x11
//    = LoadVectorRegisterList {constraints: }
class LoadStoreVectorRegisterListTesterCase87
    : public LoadStoreVectorRegisterListTester {
 public:
  LoadStoreVectorRegisterListTesterCase87(const NamedClassDecoder& decoder)
    : LoadStoreVectorRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreVectorRegisterListTesterCase87
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01B00000) != 0x01300000 /* opcode(24:20)=~10x11 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTesterCase88
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterCase88(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase88
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op1(24:20)=~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0000x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=0000x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase89
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase89(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase89
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op(24:20)=~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0000x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0000x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase90
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase90(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase90
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00000000 /* op1(24:20)=~0000x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0001x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=0001x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTesterCase91
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterCase91(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase91
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op1(24:20)=~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0001x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=0001x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase92
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase92(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase92
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op(24:20)=~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0001x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0001x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase93
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase93(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase93
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00200000 /* op1(24:20)=~0001x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0010x & inst(19:16)=~1111
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NeitherRdIsPcAndSNorRnIsPcAndNotS'"]}
//
// Representaive case:
// op(24:20)=0010x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
class Binary2RegisterImmediateOpTesterCase94
    : public Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS {
 public:
  Binary2RegisterImmediateOpTesterCase94(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase94
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

// Neutral case:
// inst(24:20)=0010x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=0010x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTesterCase95
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterCase95(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase95
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op1(24:20)=~0010x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0010x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0010x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase96
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase96(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase96
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00400000 /* op1(24:20)=~0010x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0011x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=0011x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTesterCase97
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterCase97(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase97
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op1(24:20)=~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0011x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=0011x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase98
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase98(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase98
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op(24:20)=~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0011x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0011x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase99
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase99(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase99
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00600000 /* op1(24:20)=~0011x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0100x & inst(19:16)=~1111
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NeitherRdIsPcAndSNorRnIsPcAndNotS'"]}
//
// Representaive case:
// op(24:20)=0100x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
class Binary2RegisterImmediateOpTesterCase100
    : public Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS {
 public:
  Binary2RegisterImmediateOpTesterCase100(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase100
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

// Neutral case:
// inst(24:20)=0100x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=0100x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTesterCase101
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterCase101(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase101
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op1(24:20)=~0100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0100x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase102
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase102(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase102
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00800000 /* op1(24:20)=~0100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTesterCase103
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterCase103(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase103
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op1(24:20)=~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0101x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=0101x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase104
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase104(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase104
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op(24:20)=~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0101x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0101x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase105
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase105(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase105
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00A00000 /* op1(24:20)=~0101x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0110x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=0110x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTesterCase106
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterCase106(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase106
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op1(24:20)=~0110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0110x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=0110x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase107
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase107(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase107
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op(24:20)=~0110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0110x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase108
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase108(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase108
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00C00000 /* op1(24:20)=~0110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTesterCase109
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterCase109(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase109
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op1(24:20)=~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0111x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=0111x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase110
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase110(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase110
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op(24:20)=~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=0111x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=0111x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase111
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase111(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase111
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x00E00000 /* op1(24:20)=~0111x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:21)=1001 & inst(19:16)=1101 & inst(11:0)=000000000100
//    = Store2RegisterImm12OpRnNotRtOnWriteback {'constraints': }
//
// Representaive case:
// Flags(24:21)=1001 & Rn(19:16)=1101 & Imm12(11:0)=000000000100
//    = Store2RegisterImm12OpRnNotRtOnWriteback {constraints: }
class LoadStore2RegisterImm12OpTesterCase112
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase112(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase112
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

// Neutral case:
// inst(24:20)=1100x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=1100x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTesterCase113
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterCase113(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase113
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op1(24:20)=~1100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1100x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=1100x
//    = Binary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase114
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase114(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase114
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op(24:20)=~1100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1100x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase115
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase115(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase115
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01800000 /* op1(24:20)=~1100x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'safety': ["'NeitherImm5NotZeroNorNotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: ['NeitherImm5NotZeroNorNotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTesterCase116
    : public Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTesterCase116(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase116
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7)=00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op3(6:5)=~00 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'safety': ["'NeitherImm5NotZeroNorNotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: ['NeitherImm5NotZeroNorNotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTesterCase117
    : public Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTesterCase117(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase117
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) == 0x00000000 /* op2(11:7)=00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op3(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTesterNeitherImm5NotZeroNorNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Unary2RegisterOpTesterCase118
    : public Unary2RegisterOpTesterNotRdIsPcAndS {
 public:
  Unary2RegisterOpTesterCase118(const NamedClassDecoder& decoder)
    : Unary2RegisterOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpTesterCase118
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7)=~00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op3(6:5)=~00 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Unary2RegisterOpTesterCase119
    : public Unary2RegisterOpTesterNotRdIsPcAndS {
 public:
  Unary2RegisterOpTesterCase119(const NamedClassDecoder& decoder)
    : Unary2RegisterOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpTesterCase119
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000F80) != 0x00000000 /* op2(11:7)=~00000 */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op3(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTesterCase120
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterCase120(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase120
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000000 /* op2(6:5)=~00 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTesterCase121
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterCase121(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase121
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTesterCase122
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterCase122(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase122
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1101x & op2(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract {constraints: ,
//     safety: ['RegsNotPc']}
class Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase123
    : public Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc {
 public:
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase123(const NamedClassDecoder& decoder)
    : Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase123
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

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1101x & op2(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTesterCase124
    : public Binary3RegisterOpTesterRegsNotPc {
 public:
  Binary3RegisterOpTesterCase124(const NamedClassDecoder& decoder)
    : Binary3RegisterOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpTesterCase124
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=1101x & op3(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTesterCase125
    : public Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTesterCase125(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase125
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op3(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=1101x & op3(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTesterCase126
    : public Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTesterCase126(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase126
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op1(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op3(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1101x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=1101x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Unary1RegisterImmediateOpTesterCase127
    : public Unary1RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Unary1RegisterImmediateOpTesterCase127(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase127
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01A00000 /* op(24:20)=~1101x */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1110x & inst(7:5)=x00 & inst(3:0)=~1111
//    = Binary2RegisterBitRangeMsbGeLsb {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=~1111
//    = Binary2RegisterBitRangeMsbGeLsb {constraints: ,
//     safety: ['RegsNotPc']}
class Binary2RegisterBitRangeMsbGeLsbTesterCase128
    : public Binary2RegisterBitRangeMsbGeLsbTesterRegsNotPc {
 public:
  Binary2RegisterBitRangeMsbGeLsbTesterCase128(const NamedClassDecoder& decoder)
    : Binary2RegisterBitRangeMsbGeLsbTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterBitRangeMsbGeLsbTesterCase128
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

// Neutral case:
// inst(24:20)=1110x & inst(7:5)=x00 & inst(3:0)=1111
//    = Unary1RegisterBitRangeMsbGeLsb {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=1111
//    = Unary1RegisterBitRangeMsbGeLsb {constraints: ,
//     safety: ['RegsNotPc']}
class Unary1RegisterBitRangeMsbGeLsbTesterCase129
    : public Unary1RegisterBitRangeMsbGeLsbTesterRegsNotPc {
 public:
  Unary1RegisterBitRangeMsbGeLsbTesterCase129(const NamedClassDecoder& decoder)
    : Unary1RegisterBitRangeMsbGeLsbTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterBitRangeMsbGeLsbTesterCase129
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

// Neutral case:
// inst(24:20)=1110x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=1110x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTesterCase130
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Binary3RegisterImmedShiftedOpTesterCase130(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase130
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20)=~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1110x
//    = MaskedBinary2RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=1110x
//    = MaskedBinary2RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTesterCase131
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterCase131(const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterImmediateOpTesterCase131
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op(24:20)=~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1110x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTesterCase132
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Binary4RegisterShiftedOpTesterCase132(const NamedClassDecoder& decoder)
    : Binary4RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterShiftedOpTesterCase132
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01C00000 /* op1(24:20)=~1110x */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1111x & inst(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1111x & op2(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract {constraints: ,
//     safety: ['RegsNotPc']}
class Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase133
    : public Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc {
 public:
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase133(const NamedClassDecoder& decoder)
    : Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase133
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

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTesterCase134
    : public Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  Unary2RegisterImmedShiftedOpTesterCase134(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpTesterCase134
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op1(24:20)=~1111x */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representaive case:
// op(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {constraints: ,
//     safety: ['NotRdIsPcAndS']}
class Unary1RegisterImmediateOpTesterCase135
    : public Unary1RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Unary1RegisterImmediateOpTesterCase135(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase135
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op(24:20)=~1111x */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Unary3RegisterShiftedOpTesterCase136
    : public Unary3RegisterShiftedOpTesterRegsNotPc {
 public:
  Unary3RegisterShiftedOpTesterCase136(const NamedClassDecoder& decoder)
    : Unary3RegisterShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary3RegisterShiftedOpTesterCase136
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01E00000) != 0x01E00000 /* op1(24:20)=~1111x */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary3RegisterShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=11xxx11 & inst(7:4)=xxx0
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=11xxx11 & op2(7:4)=xxx0
//    = UnpredictableUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase137
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase137(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase137
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

// Neutral case:
// inst(27:20)=110xxxx0 & inst(27:20)=~11000x00
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(27:20)=110xxxx0 & op1_repeated(27:20)=~11000x00
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase138
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase138(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase138
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

// Neutral case:
// inst(27:20)=110xxxx1 & inst(19:16)=~1111 & inst(27:20)=~11000x01
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(27:20)=110xxxx1 & Rn(19:16)=~1111 & op1_repeated(27:20)=~11000x01
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase139
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase139(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase139
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

// Neutral case:
// inst(27:20)=110xxxx1 & inst(19:16)=1111 & inst(27:20)=~11000x01
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(27:20)=110xxxx1 & Rn(19:16)=1111 & op1_repeated(27:20)=~11000x01
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase140
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase140(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase140
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

// Neutral case:
// inst(27:20)=1110xxxx & inst(4)=0
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(27:20)=1110xxxx & op(4)=0
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase141
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase141(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase141
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

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx00 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = Unary1RegisterUse {'constraints': }
//
// Representaive case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = Unary1RegisterUse {constraints: }
class Unary1RegisterUseTesterCase142
    : public Unary1RegisterUseTester {
 public:
  Unary1RegisterUseTesterCase142(const NamedClassDecoder& decoder)
    : Unary1RegisterUseTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterUseTesterCase142
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9)=~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00030000) != 0x00000000 /* op1(19:16)=~xx00 */) return false;
  if ((inst.Bits() & 0x0000FD00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterUseTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase143
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase143(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase143
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9)=~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00030000) != 0x00010000 /* op1(19:16)=~xx01 */) return false;
  if ((inst.Bits() & 0x0000FD00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase144
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase144(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase144
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9)=~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00020000) != 0x00020000 /* op1(19:16)=~xx1x */) return false;
  if ((inst.Bits() & 0x0000FD00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase145
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase145(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase145
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9)=~0 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21)=~11 */) return false;
  if ((inst.Bits() & 0x0000FD00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = Unary1RegisterSet {'constraints': }
//
// Representaive case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = Unary1RegisterSet {constraints: }
class Unary1RegisterSetTesterCase146
    : public Unary1RegisterSetTester {
 public:
  Unary1RegisterSetTesterCase146(const NamedClassDecoder& decoder)
    : Unary1RegisterSetTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterSetTesterCase146
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000000 /* B(9)=~0 */) return false;
  if ((inst.Bits() & 0x00200000) != 0x00000000 /* op(22:21)=~x0 */) return false;
  if ((inst.Bits() & 0x000F0D0F) != 0x000F0000 /* $pattern(31:0)=~xxxxxxxxxxxx1111xxxx00x0xxxx0000 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterSetTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase147
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase147(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase147
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000200 /* B(9)=~1 */) return false;
  if ((inst.Bits() & 0x00200000) != 0x00000000 /* op(22:21)=~x0 */) return false;
  if ((inst.Bits() & 0x00000C0F) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx00xxxxxx0000 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x1 & inst(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase148
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase148(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase148
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000000 /* op2(6:4)=~000 */) return false;
  if ((inst.Bits() & 0x00000200) != 0x00000200 /* B(9)=~1 */) return false;
  if ((inst.Bits() & 0x00200000) != 0x00200000 /* op(22:21)=~x1 */) return false;
  if ((inst.Bits() & 0x0000FC00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx111100xxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=001 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = BranchToRegister {'constraints': }
//
// Representaive case:
// op2(6:4)=001 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = BranchToRegister {constraints: }
class BranchToRegisterTesterCase149
    : public BranchToRegisterTester {
 public:
  BranchToRegisterTesterCase149(const NamedClassDecoder& decoder)
    : BranchToRegisterTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BranchToRegisterTesterCase149
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000010 /* op2(6:4)=~001 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x000FFF00) != 0x000FFF00 /* $pattern(31:0)=~xxxxxxxxxxxx111111111111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BranchToRegisterTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=001 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPc {'constraints': }
//
// Representaive case:
// op2(6:4)=001 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPc {constraints: }
class Unary2RegisterOpNotRmIsPcTesterCase150
    : public Unary2RegisterOpNotRmIsPcTester {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase150(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase150
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000010 /* op2(6:4)=~001 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21)=~11 */) return false;
  if ((inst.Bits() & 0x000F0F00) != 0x000F0F00 /* $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=010 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op2(6:4)=010 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase151
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase151(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase151
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000020 /* op2(6:4)=~010 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x000FFF00) != 0x000FFF00 /* $pattern(31:0)=~xxxxxxxxxxxx111111111111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=011 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = BranchToRegister {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op2(6:4)=011 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = BranchToRegister {constraints: ,
//     safety: ['RegsNotPc']}
class BranchToRegisterTesterCase152
    : public BranchToRegisterTesterRegsNotPc {
 public:
  BranchToRegisterTesterCase152(const NamedClassDecoder& decoder)
    : BranchToRegisterTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BranchToRegisterTesterCase152
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000030 /* op2(6:4)=~011 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x000FFF00) != 0x000FFF00 /* $pattern(31:0)=~xxxxxxxxxxxx111111111111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BranchToRegisterTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=110 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op2(6:4)=110 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase153
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase153(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase153
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000060 /* op2(6:4)=~110 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21)=~11 */) return false;
  if ((inst.Bits() & 0x000FFF0F) != 0x0000000E /* $pattern(31:0)=~xxxxxxxxxxxx000000000000xxxx1110 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=01
//    = BreakPointAndConstantPoolHead {'constraints': }
//
// Representaive case:
// op2(6:4)=111 & op(22:21)=01
//    = BreakPointAndConstantPoolHead {constraints: }
class Immediate16UseTesterCase154
    : public Immediate16UseTester {
 public:
  Immediate16UseTesterCase154(const NamedClassDecoder& decoder)
    : Immediate16UseTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Immediate16UseTesterCase154
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

// Neutral case:
// inst(6:4)=111 & inst(22:21)=10
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op2(6:4)=111 & op(22:21)=10
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase155
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase155(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase155
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

// Neutral case:
// inst(6:4)=111 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op2(6:4)=111 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase156
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase156(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase156
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000070) != 0x00000070 /* op2(6:4)=~111 */) return false;
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21)=~11 */) return false;
  if ((inst.Bits() & 0x000FFF00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxx000000000000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=000100
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=000100
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase157
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase157(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase157
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

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=000101
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=000101
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase158
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase158(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase158
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

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxx0 & inst(4)=1
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=10xxx0 & op(4)=1
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase159
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase159(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase159
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

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxx1 & inst(4)=1
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=10xxx1 & op(4)=1
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase160
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase160(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase160
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

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx0 & inst(25:20)=~000x00
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx0 & op1_repeated(25:20)=~000x00
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase161
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase161(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase161
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

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx1 & inst(19:16)=~1111 & inst(25:20)=~000x01
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=~1111 & op1_repeated(25:20)=~000x01
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase162
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase162(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase162
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

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx1 & inst(19:16)=1111 & inst(25:20)=~000x01
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=1111 & op1_repeated(25:20)=~000x01
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase163
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase163(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase163
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

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxxx & inst(4)=0
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// coproc(11:8)=~101x & op1(25:20)=10xxxx & op(4)=0
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase164
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase164(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase164
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

// Neutral case:
// inst(19:16)=001x & inst(7:6)=x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc2(19:16)=001x & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase165
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase165(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase165
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x000E0000) != 0x00020000 /* opc2(19:16)=~001x */) return false;
  if ((inst.Bits() & 0x00000040) != 0x00000040 /* opc3(7:6)=~x1 */) return false;
  if ((inst.Bits() & 0x00000100) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(19:16)=101x & inst(7:6)=x1
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc2(19:16)=101x & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase166
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase166(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase166
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

// Neutral case:
// inst(19:16)=110x & inst(7:6)=x1
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc2(19:16)=110x & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase167
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase167(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase167
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

// Neutral case:
// inst(19:16)=111x & inst(7:6)=x1
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc2(19:16)=111x & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase168
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase168(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase168
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

// Neutral case:
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterImmedShiftedOpRegsNotPc {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterImmedShiftedOpTesterCase169
    : public Binary3RegisterImmedShiftedOpTesterRegsNotPc {
 public:
  Binary3RegisterImmedShiftedOpTesterCase169(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase169
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20)=~000 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */) return false;
  if ((inst.Bits() & 0x00000300) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterImmedShiftedOpRegsNotPc {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterImmedShiftedOpRegsNotPcTesterCase170
    : public Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTesterCase170(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpRegsNotPcTesterCase170
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20)=~000 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x00000300) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase171
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase171(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase171
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00000000 /* op1(22:20)=~000 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000A0 /* op2(7:5)=~101 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=00x & inst(15:12)=~1111
//    = Binary4RegisterDualOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterCase172
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase172(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase172
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
//    = Binary3RegisterOpAltA {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATesterCase173
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase173(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase173
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
//    = Binary4RegisterDualOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterCase174
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase174(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase174
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
//    = Binary3RegisterOpAltA {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATesterCase175
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase175(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase175
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
// inst(22:20)=000 & inst(7:5)=xx0
//    = Binary3RegisterImmedShiftedOpRegsNotPc {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=000 & op2(7:5)=xx0
//    = Binary3RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterImmedShiftedOpTesterCase176
    : public Binary3RegisterImmedShiftedOpTesterRegsNotPc {
 public:
  Binary3RegisterImmedShiftedOpTesterCase176(const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterImmedShiftedOpTesterCase176
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

// Neutral case:
// inst(22:20)=001 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {'constraints': }
//
// Representaive case:
// op1(22:20)=001 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {constraints: }
class Binary3RegisterOpAltATesterCase177
    : public Binary3RegisterOpAltATester {
 public:
  Binary3RegisterOpAltATesterCase177(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase177
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
// inst(22:20)=010 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Unary2RegisterSatImmedShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=010 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTesterCase178
    : public Unary2RegisterSatImmedShiftedOpTesterRegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTesterCase178(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterCase178
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00200000 /* op1(22:20)=~010 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase179
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase179(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase179
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00200000 /* op1(22:20)=~010 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */) return false;
  if ((inst.Bits() & 0x00000300) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterImmedShiftedOpRegsNotPc {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterImmedShiftedOpRegsNotPcTesterCase180
    : public Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTesterCase180(const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterImmedShiftedOpRegsNotPcTesterCase180
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00200000 /* op1(22:20)=~010 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x00000300) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterImmedShiftedOpRegsNotPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {'constraints': }
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {constraints: }
class Binary3RegisterOpAltATesterCase181
    : public Binary3RegisterOpAltATester {
 public:
  Binary3RegisterOpAltATesterCase181(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase181
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
// inst(22:20)=011 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase182
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase182(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase182
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00300000 /* op1(22:20)=~011 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;
  if ((inst.Bits() & 0x000F0F00) != 0x000F0F00 /* $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase183
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase183(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase183
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00300000 /* op1(22:20)=~011 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */) return false;
  if ((inst.Bits() & 0x00000300) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase184
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase184(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase184
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00300000 /* op1(22:20)=~011 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x00000300) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=011 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase185
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase185(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase185
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00300000 /* op1(22:20)=~011 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000A0 /* op2(7:5)=~101 */) return false;
  if ((inst.Bits() & 0x000F0F00) != 0x000F0F00 /* $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase186
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase186(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase186
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00400000 /* op1(22:20)=~100 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */) return false;
  if ((inst.Bits() & 0x00000300) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase187
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase187(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase187
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00400000 /* op1(22:20)=~100 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x00000300) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=00x
//    = Binary4RegisterDualResult {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=100 & op2(7:5)=00x
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTesterCase188
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterCase188(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase188
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
//    = Binary4RegisterDualResult {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=100 & op2(7:5)=01x
//    = Binary4RegisterDualResult {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTesterCase189
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterCase189(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase189
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
//    = Binary4RegisterDualOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterCase190
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase190(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase190
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
//    = Binary3RegisterOpAltA {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATesterCase191
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase191(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase191
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
//    = Binary4RegisterDualOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=101 & op2(7:5)=11x
//    = Binary4RegisterDualOp {constraints: ,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTesterCase192
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase192(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase192
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

// Neutral case:
// inst(22:20)=110 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Unary2RegisterSatImmedShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=110 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTesterCase193
    : public Unary2RegisterSatImmedShiftedOpTesterRegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTesterCase193(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterCase193
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00600000 /* op1(22:20)=~110 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000F00 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterSatImmedShiftedOpTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase194
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase194(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase194
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00600000 /* op1(22:20)=~110 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */) return false;
  if ((inst.Bits() & 0x00000300) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase195
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase195(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase195
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00600000 /* op1(22:20)=~110 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x00000300) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase196
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase196(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase196
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00700000 /* op1(22:20)=~111 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000020 /* op2(7:5)=~001 */) return false;
  if ((inst.Bits() & 0x000F0F00) != 0x000F0F00 /* $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase197
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase197(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase197
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00700000 /* op1(22:20)=~111 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* A(19:16)=1111 */) return false;
  if ((inst.Bits() & 0x00000300) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase198
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase198(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase198
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00700000 /* op1(22:20)=~111 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x00000060 /* op2(7:5)=~011 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* A(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x00000300) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=111 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcTesterCase199
    : public Unary2RegisterOpNotRmIsPcTesterRegsNotPc {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase199(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase199
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00700000) != 0x00700000 /* op1(22:20)=~111 */) return false;
  if ((inst.Bits() & 0x000000E0) != 0x000000A0 /* op2(7:5)=~101 */) return false;
  if ((inst.Bits() & 0x000F0F00) != 0x000F0F00 /* $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary2RegisterOpNotRmIsPcTesterRegsNotPc::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=0x00 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Deprecated {'constraints': }
//
// Representaive case:
// op(23:20)=0x00 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Deprecated {constraints: }
class UnsafeCondDecoderTesterCase200
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase200(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase200
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

// Neutral case:
// inst(23:20)=0x00
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc1(23:20)=0x00
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase201
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase201(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase201
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00000000 /* opc1(23:20)=~0x00 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=0x01
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc1(23:20)=0x01
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase202
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase202(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase202
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00100000 /* opc1(23:20)=~0x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=0x10 & inst(7:6)=x0
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc1(23:20)=0x10 & opc3(7:6)=x0
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase203
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase203(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase203
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

// Neutral case:
// inst(23:20)=0x10 & inst(7:6)=x1
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc1(23:20)=0x10 & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase204
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase204(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase204
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

// Neutral case:
// inst(23:20)=0x11 & inst(7:6)=x0
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc1(23:20)=0x11 & opc3(7:6)=x0
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase205
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase205(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase205
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

// Neutral case:
// inst(23:20)=0x11 & inst(7:6)=x1
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc1(23:20)=0x11 & opc3(7:6)=x1
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase206
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase206(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase206
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

// Neutral case:
// inst(23:20)=1x00 & inst(7:6)=x0
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc1(23:20)=1x00 & opc3(7:6)=x0
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase207
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase207(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase207
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

// Neutral case:
// inst(23:20)=1x01
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc1(23:20)=1x01
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase208
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase208(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase208
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00900000 /* opc1(23:20)=~1x01 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(23:20)=1x10
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc1(23:20)=1x10
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase209
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase209(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase209
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00B00000) != 0x00A00000 /* opc1(23:20)=~1x10 */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
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
class Binary3RegisterOpAltATesterCase210
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase210(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase210
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
class Binary4RegisterDualOpTesterCase211
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase211(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase211
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
class Binary4RegisterDualResultTesterCase212
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterCase212(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase212
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
class Binary4RegisterDualResultTesterCase213
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterCase213(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase213
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
class Binary4RegisterDualResultTesterCase214
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterCase214(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase214
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
class Binary4RegisterDualResultTesterCase215
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterCase215(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase215
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
// inst(24:20)=1xx00
//    = StoreVectorRegister {'constraints': }
//
// Representaive case:
// opcode(24:20)=1xx00
//    = StoreVectorRegister {constraints: }
class StoreVectorRegisterTesterCase216
    : public StoreVectorRegisterTester {
 public:
  StoreVectorRegisterTesterCase216(const NamedClassDecoder& decoder)
    : StoreVectorRegisterTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool StoreVectorRegisterTesterCase216
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01300000) != 0x01000000 /* opcode(24:20)=~1xx00 */) return false;

  // Check other preconditions defined for the base decoder.
  return StoreVectorRegisterTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(24:20)=1xx01
//    = LoadVectorRegister {'constraints': }
//
// Representaive case:
// opcode(24:20)=1xx01
//    = LoadVectorRegister {constraints: }
class LoadStoreVectorOpTesterCase217
    : public LoadStoreVectorOpTester {
 public:
  LoadStoreVectorOpTesterCase217(const NamedClassDecoder& decoder)
    : LoadStoreVectorOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStoreVectorOpTesterCase217
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x01300000) != 0x01100000 /* opcode(24:20)=~1xx01 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreVectorOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=0xx1x0 & inst(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(25:20)=0xx1x0 & $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase218
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase218(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase218
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
class UnsafeCondDecoderTesterCase219
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase219(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase219
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
class UnsafeCondDecoderTesterCase220
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase220(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase220
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
// inst(27:20)=101xxxxx
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(27:20)=101xxxxx
//    = ForbiddenUncondDecoder {constraints: }
class UnsafeUncondDecoderTesterCase221
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase221(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase221
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x0E000000) != 0x0A000000 /* op1(27:20)=~101xxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Store3RegisterOp {'constraints': }
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Store3RegisterOp {constraints: }
class LoadStore3RegisterOpTesterCase222
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterCase222(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterOpTesterCase222
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00000000 /* op1(24:20)=~xx0x0 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp {'constraints': }
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp {constraints: }
class LoadStore3RegisterOpTesterCase223
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterCase223(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterOpTesterCase223
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20)=~xx0x1 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x0
//    = Store2RegisterImm8Op {'constraints': }
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx1x0
//    = Store2RegisterImm8Op {constraints: }
class LoadStore2RegisterImm8OpTesterCase224
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase224(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase224
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

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = Load2RegisterImm8Op {'constraints': }
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op {constraints: }
class LoadStore2RegisterImm8OpTesterCase225
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase225(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase225
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

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=1111
//    = Load2RegisterImm8Op {'constraints': }
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op {constraints: }
class LoadStore2RegisterImm8OpTesterCase226
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase226(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase226
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

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterDoubleOp {'constraints': }
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterDoubleOp {constraints: }
class LoadStore3RegisterDoubleOpTesterCase227
    : public LoadStore3RegisterDoubleOpTester {
 public:
  LoadStore3RegisterDoubleOpTesterCase227(const NamedClassDecoder& decoder)
    : LoadStore3RegisterDoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterDoubleOpTesterCase227
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00000000 /* op1(24:20)=~xx0x0 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterDoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp {'constraints': }
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp {constraints: }
class LoadStore3RegisterOpTesterCase228
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterCase228(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterOpTesterCase228
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20)=~xx0x1 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=~1111
//    = Load2RegisterImm8DoubleOp {'constraints': }
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=~1111
//    = Load2RegisterImm8DoubleOp {constraints: }
class LoadStore2RegisterImm8DoubleOpTesterCase229
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  LoadStore2RegisterImm8DoubleOpTesterCase229(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8DoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8DoubleOpTesterCase229
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

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=1111
//    = Load2RegisterImm8DoubleOp {'constraints': }
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=1111
//    = Load2RegisterImm8DoubleOp {constraints: }
class LoadStore2RegisterImm8DoubleOpTesterCase230
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  LoadStore2RegisterImm8DoubleOpTesterCase230(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8DoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8DoubleOpTesterCase230
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

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = Load2RegisterImm8Op {'constraints': }
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op {constraints: }
class LoadStore2RegisterImm8OpTesterCase231
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase231(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase231
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

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=1111
//    = Load2RegisterImm8Op {'constraints': }
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op {constraints: }
class LoadStore2RegisterImm8OpTesterCase232
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase232(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase232
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

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Store3RegisterDoubleOp {'constraints': }
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Store3RegisterDoubleOp {constraints: }
class LoadStore3RegisterDoubleOpTesterCase233
    : public LoadStore3RegisterDoubleOpTester {
 public:
  LoadStore3RegisterDoubleOpTesterCase233(const NamedClassDecoder& decoder)
    : LoadStore3RegisterDoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterDoubleOpTesterCase233
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00000000 /* op1(24:20)=~xx0x0 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterDoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp {'constraints': }
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp {constraints: }
class LoadStore3RegisterOpTesterCase234
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterCase234(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterOpTesterCase234
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20)=~xx0x1 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore3RegisterOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x0
//    = Store2RegisterImm8DoubleOp {'constraints': }
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx1x0
//    = Store2RegisterImm8DoubleOp {constraints: }
class LoadStore2RegisterImm8DoubleOpTesterCase235
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  LoadStore2RegisterImm8DoubleOpTesterCase235(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8DoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8DoubleOpTesterCase235
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

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = Load2RegisterImm8Op {'constraints': }
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op {constraints: }
class LoadStore2RegisterImm8OpTesterCase236
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase236(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase236
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

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=1111
//    = Load2RegisterImm8Op {'constraints': }
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op {constraints: }
class LoadStore2RegisterImm8OpTesterCase237
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase237(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase237
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

// Neutral case:
// inst(21:20)=01 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(21:20)=01 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase238
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase238(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase238
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase239
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase239(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase239
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase240
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase240(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase240
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase241
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase241(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase241
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase242
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase242(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase242
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase243
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase243(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase243
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase244
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase244(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase244
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase245
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase245(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase245
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase246
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase246(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase246
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase247
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase247(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase247
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase248
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase248(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase248
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase249
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase249(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase249
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase250
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase250(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase250
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase251
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase251(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase251
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase252
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase252(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase252
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase253
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase253(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase253
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase254
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase254(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase254
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
class Binary3RegisterOpAltBNoCondUpdatesTesterCase255
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase255(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase255
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

// Neutral case:
// inst(22:21)=00 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op(22:21)=00 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase256
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase256(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase256
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00000000 /* op(22:21)=~00 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
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
class Binary4RegisterDualOpTesterCase257
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase257(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase257
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
// inst(22:20)=01x & inst(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=01x & op2(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTesterCase258
    : public Unary2RegisterSatImmedShiftedOpTesterRegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTesterCase258(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterCase258
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

// Neutral case:
// inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase259
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase259(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase259
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00200000 /* op(22:21)=~01 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
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
class Binary4RegisterDualOpTesterCase260
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  Binary4RegisterDualOpTesterCase260(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpTesterCase260
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
class Binary3RegisterOpAltATesterCase261
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase261(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase261
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
// inst(22:21)=10 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op(22:21)=10 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase262
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase262(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase262
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00400000 /* op(22:21)=~10 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
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
class Binary4RegisterDualResultTesterCase263
    : public Binary4RegisterDualResultTesterRegsNotPc {
 public:
  Binary4RegisterDualResultTesterCase263(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultTesterCase263
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
// inst(22:20)=11x & inst(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(22:20)=11x & op2(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTesterCase264
    : public Unary2RegisterSatImmedShiftedOpTesterRegsNotPc {
 public:
  Unary2RegisterSatImmedShiftedOpTesterCase264(const NamedClassDecoder& decoder)
    : Unary2RegisterSatImmedShiftedOpTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterSatImmedShiftedOpTesterCase264
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

// Neutral case:
// inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTesterCase265
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTesterCase265(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltBNoCondUpdatesTesterCase265
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00600000) != 0x00600000 /* op(22:21)=~11 */) return false;
  if ((inst.Bits() & 0x00000F00) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
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
class Binary3RegisterOpAltATesterCase266
    : public Binary3RegisterOpAltATesterRegsNotPc {
 public:
  Binary3RegisterOpAltATesterCase266(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltATesterCase266
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

// Neutral case:
// inst(25:20)=10xxxx
//    = BranchImmediate24 {'constraints': }
//
// Representaive case:
// op(25:20)=10xxxx
//    = BranchImmediate24 {constraints: }
class BranchImmediate24TesterCase267
    : public BranchImmediate24Tester {
 public:
  BranchImmediate24TesterCase267(const NamedClassDecoder& decoder)
    : BranchImmediate24Tester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BranchImmediate24TesterCase267
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
class BranchImmediate24TesterCase268
    : public BranchImmediate24Tester {
 public:
  BranchImmediate24TesterCase268(const NamedClassDecoder& decoder)
    : BranchImmediate24Tester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BranchImmediate24TesterCase268
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03000000) != 0x03000000 /* op(25:20)=~11xxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return BranchImmediate24Tester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(8)=0 & inst(7:4)=00x1
//    = MoveDoubleVfpRegisterOp {'constraints': }
//
// Representaive case:
// C(8)=0 & op(7:4)=00x1
//    = MoveDoubleVfpRegisterOp {constraints: }
class MoveDoubleVfpRegisterOpTesterCase269
    : public MoveDoubleVfpRegisterOpTester {
 public:
  MoveDoubleVfpRegisterOpTesterCase269(const NamedClassDecoder& decoder)
    : MoveDoubleVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveDoubleVfpRegisterOpTesterCase269
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

// Neutral case:
// inst(8)=1 & inst(7:4)=00x1
//    = MoveDoubleVfpRegisterOp {'constraints': }
//
// Representaive case:
// C(8)=1 & op(7:4)=00x1
//    = MoveDoubleVfpRegisterOp {constraints: }
class MoveDoubleVfpRegisterOpTesterCase270
    : public MoveDoubleVfpRegisterOpTester {
 public:
  MoveDoubleVfpRegisterOpTesterCase270(const NamedClassDecoder& decoder)
    : MoveDoubleVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveDoubleVfpRegisterOpTesterCase270
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

// Neutral case:
// inst(20)=0 & inst(8)=0 & inst(23:21)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = MoveVfpRegisterOp {'constraints': }
//
// Representaive case:
// L(20)=0 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = MoveVfpRegisterOp {constraints: }
class MoveVfpRegisterOpTesterCase271
    : public MoveVfpRegisterOpTester {
 public:
  MoveVfpRegisterOpTesterCase271(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveVfpRegisterOpTesterCase271
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
//    = VfpUsesRegOp {'constraints': }
//
// Representaive case:
// L(20)=0 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = VfpUsesRegOp {constraints: }
class VfpUsesRegOpTesterCase272
    : public VfpUsesRegOpTester {
 public:
  VfpUsesRegOpTesterCase272(const NamedClassDecoder& decoder)
    : VfpUsesRegOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VfpUsesRegOpTesterCase272
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
//    = MoveVfpRegisterOpWithTypeSel {'constraints': }
//
// Representaive case:
// L(20)=0 & C(8)=1 & A(23:21)=0xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = MoveVfpRegisterOpWithTypeSel {constraints: }
class MoveVfpRegisterOpWithTypeSelTesterCase273
    : public MoveVfpRegisterOpWithTypeSelTester {
 public:
  MoveVfpRegisterOpWithTypeSelTesterCase273(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpWithTypeSelTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveVfpRegisterOpWithTypeSelTesterCase273
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
//    = DuplicateToAdvSIMDRegisters {'constraints': }
//
// Representaive case:
// L(20)=0 & C(8)=1 & A(23:21)=1xx & B(6:5)=0x & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = DuplicateToAdvSIMDRegisters {constraints: }
class DuplicateToAdvSIMDRegistersTesterCase274
    : public DuplicateToAdvSIMDRegistersTester {
 public:
  DuplicateToAdvSIMDRegistersTesterCase274(const NamedClassDecoder& decoder)
    : DuplicateToAdvSIMDRegistersTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool DuplicateToAdvSIMDRegistersTesterCase274
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
//    = MoveVfpRegisterOp {'constraints': }
//
// Representaive case:
// L(20)=1 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = MoveVfpRegisterOp {constraints: }
class MoveVfpRegisterOpTesterCase275
    : public MoveVfpRegisterOpTester {
 public:
  MoveVfpRegisterOpTesterCase275(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveVfpRegisterOpTesterCase275
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
//    = VfpMrsOp {'constraints': }
//
// Representaive case:
// L(20)=1 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = VfpMrsOp {constraints: }
class VfpMrsOpTesterCase276
    : public VfpMrsOpTester {
 public:
  VfpMrsOpTesterCase276(const NamedClassDecoder& decoder)
    : VfpMrsOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VfpMrsOpTesterCase276
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
//    = MoveVfpRegisterOpWithTypeSel {'constraints': }
//
// Representaive case:
// L(20)=1 & C(8)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = MoveVfpRegisterOpWithTypeSel {constraints: }
class MoveVfpRegisterOpWithTypeSelTesterCase277
    : public MoveVfpRegisterOpWithTypeSelTester {
 public:
  MoveVfpRegisterOpWithTypeSelTesterCase277(const NamedClassDecoder& decoder)
    : MoveVfpRegisterOpWithTypeSelTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveVfpRegisterOpWithTypeSelTesterCase277
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

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000000 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = CondDecoder {'constraints': }
//
// Representaive case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000000 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = CondDecoder {constraints: }
class CondDecoderTesterCase278
    : public CondDecoderTester {
 public:
  CondDecoderTesterCase278(const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondDecoderTesterCase278
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000000 /* op2(7:0)=~00000000 */) return false;
  if ((inst.Bits() & 0x0000FF00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return CondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000001 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = CondDecoder {'constraints': }
//
// Representaive case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000001 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = CondDecoder {constraints: }
class CondDecoderTesterCase279
    : public CondDecoderTester {
 public:
  CondDecoderTesterCase279(const NamedClassDecoder& decoder)
    : CondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondDecoderTesterCase279
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000001 /* op2(7:0)=~00000001 */) return false;
  if ((inst.Bits() & 0x0000FF00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return CondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000010 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000010 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase280
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase280(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase280
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000002 /* op2(7:0)=~00000010 */) return false;
  if ((inst.Bits() & 0x0000FF00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000011 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000011 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase281
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase281(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase281
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000003 /* op2(7:0)=~00000011 */) return false;
  if ((inst.Bits() & 0x0000FF00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000100 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000100 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase282
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase282(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase282
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000FF) != 0x00000004 /* op2(7:0)=~00000100 */) return false;
  if ((inst.Bits() & 0x0000FF00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=1111xxxx & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=1111xxxx & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase283
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase283(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase283
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00000000 /* op1(19:16)=~0000 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x000000F0 /* op2(7:0)=~1111xxxx */) return false;
  if ((inst.Bits() & 0x0000FF00) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx11110000xxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0100 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = MoveImmediate12ToApsr {'constraints': }
//
// Representaive case:
// op(22)=0 & op1(19:16)=0100 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = MoveImmediate12ToApsr {constraints: }
class MoveImmediate12ToApsrTesterCase284
    : public MoveImmediate12ToApsrTester {
 public:
  MoveImmediate12ToApsrTesterCase284(const NamedClassDecoder& decoder)
    : MoveImmediate12ToApsrTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveImmediate12ToApsrTesterCase284
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x00040000 /* op1(19:16)=~0100 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveImmediate12ToApsrTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=1x00 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = MoveImmediate12ToApsr {'constraints': }
//
// Representaive case:
// op(22)=0 & op1(19:16)=1x00 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = MoveImmediate12ToApsr {constraints: }
class MoveImmediate12ToApsrTesterCase285
    : public MoveImmediate12ToApsrTester {
 public:
  MoveImmediate12ToApsrTesterCase285(const NamedClassDecoder& decoder)
    : MoveImmediate12ToApsrTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MoveImmediate12ToApsrTesterCase285
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x000B0000) != 0x00080000 /* op1(19:16)=~1x00 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return MoveImmediate12ToApsrTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(22)=0 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase286
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase286(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase286
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x00030000) != 0x00010000 /* op1(19:16)=~xx01 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=0 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(22)=0 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase287
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase287(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase287
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00000000 /* op(22)=~0 */) return false;
  if ((inst.Bits() & 0x00020000) != 0x00020000 /* op1(19:16)=~xx1x */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(22)=1 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(22)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase288
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase288(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase288
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00400000) != 0x00400000 /* op(22)=~1 */) return false;
  if ((inst.Bits() & 0x0000F000) != 0x0000F000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0x010
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// A(25)=0 & op1(24:20)=0x010
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase289
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase289(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase289
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

// Neutral case:
// inst(25)=0 & inst(24:20)=0x011
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// A(25)=0 & op1(24:20)=0x011
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase290
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase290(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase290
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

// Neutral case:
// inst(25)=0 & inst(24:20)=0x110
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// A(25)=0 & op1(24:20)=0x110
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase291
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase291(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase291
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

// Neutral case:
// inst(25)=0 & inst(24:20)=0x111
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// A(25)=0 & op1(24:20)=0x111
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase292
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase292(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase292
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

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=~1111 & inst(24:20)=~0x011
//    = Load2RegisterImm12Op {'constraints': ,
//     'safety': ["'NotRnIsPc'"]}
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x011
//    = Load2RegisterImm12Op {constraints: ,
//     safety: ['NotRnIsPc']}
class LoadStore2RegisterImm12OpTesterCase293
    : public LoadStore2RegisterImm12OpTesterNotRnIsPc {
 public:
  LoadStore2RegisterImm12OpTesterCase293(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTesterNotRnIsPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase293
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

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=1111 & inst(24:20)=~0x011 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = Load2RegisterImm12Op {'constraints': }
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x011 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = Load2RegisterImm12Op {constraints: }
class LoadStore2RegisterImm12OpTesterCase294
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase294(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase294
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25)=~0 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20)=~xx0x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00300000 /* op1_repeated(24:20)=0x011 */) return false;
  if ((inst.Bits() & 0x01200000) != 0x01000000 /* $pattern(31:0)=~xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x0 & inst(24:20)=~0x110
//    = Store2RegisterImm12Op {'constraints': }
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx1x0 & op1_repeated(24:20)=~0x110
//    = Store2RegisterImm12Op {constraints: }
class LoadStore2RegisterImm12OpTesterCase295
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase295(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase295
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

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x1 & inst(19:16)=~1111 & inst(24:20)=~0x111
//    = Load2RegisterImm12Op {'constraints': ,
//     'safety': ["'NotRnIsPc'"]}
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x111
//    = Load2RegisterImm12Op {constraints: ,
//     safety: ['NotRnIsPc']}
class LoadStore2RegisterImm12OpTesterCase296
    : public LoadStore2RegisterImm12OpTesterNotRnIsPc {
 public:
  LoadStore2RegisterImm12OpTesterCase296(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTesterNotRnIsPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase296
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

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(24:20)=~0x111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = Load2RegisterImm12Op {'constraints': }
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = Load2RegisterImm12Op {constraints: }
class LoadStore2RegisterImm12OpTesterCase297
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase297(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase297
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25)=~0 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00700000 /* op1_repeated(24:20)=0x111 */) return false;
  if ((inst.Bits() & 0x01200000) != 0x01000000 /* $pattern(31:0)=~xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0xx1x & inst(7:4)=1011
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=1011
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase298
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase298(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase298
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

// Neutral case:
// inst(25)=0 & inst(24:20)=0xx1x & inst(7:4)=11x1
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=11x1
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase299
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase299(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase299
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

// Neutral case:
// inst(25)=1 & inst(24:20)=10000
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'safety': ['inst(15:12)=1111 => UNPREDICTABLE']}
//
// Representaive case:
// op(25)=1 & op1(24:20)=10000
//    = Unary1RegisterImmediateOp {constraints: ,
//     safety: [Rd(15:12)=1111 => UNPREDICTABLE]}
class Unary1RegisterImmediateOpTesterCase300
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTesterCase300(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase300
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x02000000 /* op(25)=~1 */) return false;
  if ((inst.Bits() & 0x01F00000) != 0x01000000 /* op1(24:20)=~10000 */) return false;

  // Check other preconditions defined for the base decoder.
  return Unary1RegisterImmediateOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Unary1RegisterImmediateOpTesterCase300
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary1RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(25)=1 & inst(24:20)=10100
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'safety': ['inst(15:12)=1111 => UNPREDICTABLE']}
//
// Representaive case:
// op(25)=1 & op1(24:20)=10100
//    = Unary1RegisterImmediateOp {constraints: ,
//     safety: [Rd(15:12)=1111 => UNPREDICTABLE]}
class Unary1RegisterImmediateOpTesterCase301
    : public Unary1RegisterImmediateOpTester {
 public:
  Unary1RegisterImmediateOpTesterCase301(const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Unary1RegisterImmediateOpTesterCase301
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

bool Unary1RegisterImmediateOpTesterCase301
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Unary1RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  EXPECT_TRUE((inst.Bits() & 0x0000F000) != 0x0000F000 /* Rd(15:12)=1111 => UNPREDICTABLE */);
  return true;
}

// Neutral case:
// inst(25)=1 & inst(24:20)=0x010 & inst(4)=0
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// A(25)=1 & op1(24:20)=0x010 & B(4)=0
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase302
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase302(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase302
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

// Neutral case:
// inst(25)=1 & inst(24:20)=0x011 & inst(4)=0
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// A(25)=1 & op1(24:20)=0x011 & B(4)=0
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase303
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase303(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase303
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

// Neutral case:
// inst(25)=1 & inst(24:20)=0x110 & inst(4)=0
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// A(25)=1 & op1(24:20)=0x110 & B(4)=0
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase304
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase304(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase304
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

// Neutral case:
// inst(25)=1 & inst(24:20)=0x111 & inst(4)=0
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// A(25)=1 & op1(24:20)=0x111 & B(4)=0
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase305
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase305(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase305
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

// Neutral case:
// inst(25)=1 & inst(24:20)=xx0x0 & inst(4)=0 & inst(24:20)=~0x010
//    = Store3RegisterImm5Op {'constraints': }
//
// Representaive case:
// A(25)=1 & op1(24:20)=xx0x0 & B(4)=0 & op1_repeated(24:20)=~0x010
//    = Store3RegisterImm5Op {constraints: }
class LoadStore3RegisterImm5OpTesterCase306
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterCase306(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterCase306
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

// Neutral case:
// inst(25)=1 & inst(24:20)=xx0x1 & inst(4)=0 & inst(24:20)=~0x011
//    = Load3RegisterImm5Op {'constraints': }
//
// Representaive case:
// A(25)=1 & op1(24:20)=xx0x1 & B(4)=0 & op1_repeated(24:20)=~0x011
//    = Load3RegisterImm5Op {constraints: }
class LoadStore3RegisterImm5OpTesterCase307
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterCase307(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterCase307
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

// Neutral case:
// inst(25)=1 & inst(24:20)=xx1x0 & inst(4)=0 & inst(24:20)=~0x110
//    = Store3RegisterImm5Op {'constraints': }
//
// Representaive case:
// A(25)=1 & op1(24:20)=xx1x0 & B(4)=0 & op1_repeated(24:20)=~0x110
//    = Store3RegisterImm5Op {constraints: }
class LoadStore3RegisterImm5OpTesterCase308
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterCase308(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterCase308
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

// Neutral case:
// inst(25)=1 & inst(24:20)=xx1x1 & inst(4)=0 & inst(24:20)=~0x111
//    = Load3RegisterImm5Op {'constraints': }
//
// Representaive case:
// A(25)=1 & op1(24:20)=xx1x1 & B(4)=0 & op1_repeated(24:20)=~0x111
//    = Load3RegisterImm5Op {constraints: }
class LoadStore3RegisterImm5OpTesterCase309
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterCase309(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterCase309
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

// Neutral case:
// 
//    = Store2RegisterImm12Op {'constraints': & ~cccc010100101101tttt000000000100 }
//
// Representaive case:
// 
//    = Store2RegisterImm12Op {constraints: & ~cccc010100101101tttt000000000100 }
class LoadStore2RegisterImm12OpTesterCase310
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase310(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase310
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check pattern restrictions of row.
  if ((inst.Bits() & 0x0FFF0FFF) == 0x052D0004 /* constraint(31:0)=xxxx010100101101xxxx000000000100 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=00000x
//    = UndefinedCondDecoder {'constraints': }
//
// Representaive case:
// op1(25:20)=00000x
//    = UndefinedCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase311
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase311(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase311
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03E00000) != 0x00000000 /* op1(25:20)=~00000x */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25:20)=11xxxx
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// op1(25:20)=11xxxx
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase312
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase312(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase312
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x03000000) != 0x03000000 /* op1(25:20)=~11xxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(7:6)=x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
//    = CondVfpOp {'constraints': }
//
// Representaive case:
// opc3(7:6)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
//    = CondVfpOp {constraints: }
class CondVfpOpTesterCase313
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase313(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase313
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000040) != 0x00000000 /* opc3(7:6)=~x0 */) return false;
  if ((inst.Bits() & 0x000000A0) != 0x00000000 /* $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(27:20)=11000100
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Mcrr2_Rule_93_A2_P188'}
//
// Representative case:
// op1(27:20)=11000100
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Mcrr2_Rule_93_A2_P188}
class ForbiddenUncondDecoderTester_Case0
    : public UnsafeUncondDecoderTesterCase0 {
 public:
  ForbiddenUncondDecoderTester_Case0()
    : UnsafeUncondDecoderTesterCase0(
      state_.ForbiddenUncondDecoder_Mcrr2_Rule_93_A2_P188_instance_)
  {}
};

// Neutral case:
// inst(27:20)=11000101
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Mrrc2_Rule_101_A2_P204'}
//
// Representative case:
// op1(27:20)=11000101
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Mrrc2_Rule_101_A2_P204}
class ForbiddenUncondDecoderTester_Case1
    : public UnsafeUncondDecoderTesterCase1 {
 public:
  ForbiddenUncondDecoderTester_Case1()
    : UnsafeUncondDecoderTesterCase1(
      state_.ForbiddenUncondDecoder_Mrrc2_Rule_101_A2_P204_instance_)
  {}
};

// Neutral case:
// inst(26:20)=0010000 & inst(7:4)=0000 & inst(19:16)=xxx1 & inst(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Setend_Rule_157_P314'}
//
// Representative case:
// op1(26:20)=0010000 & op2(7:4)=0000 & Rn(19:16)=xxx1 & $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Setend_Rule_157_P314}
class ForbiddenUncondDecoderTester_Case2
    : public UnsafeUncondDecoderTesterCase2 {
 public:
  ForbiddenUncondDecoderTester_Case2()
    : UnsafeUncondDecoderTesterCase2(
      state_.ForbiddenUncondDecoder_Setend_Rule_157_P314_instance_)
  {}
};

// Neutral case:
// inst(26:20)=0010000 & inst(7:4)=xx0x & inst(19:16)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Cps_Rule_b6_1_1_A1_B6_3'}
//
// Representative case:
// op1(26:20)=0010000 & op2(7:4)=xx0x & Rn(19:16)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Cps_Rule_b6_1_1_A1_B6_3}
class ForbiddenUncondDecoderTester_Case3
    : public UnsafeUncondDecoderTesterCase3 {
 public:
  ForbiddenUncondDecoderTester_Case3()
    : UnsafeUncondDecoderTesterCase3(
      state_.ForbiddenUncondDecoder_Cps_Rule_b6_1_1_A1_B6_3_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010011
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010011
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case4
    : public UnsafeUncondDecoderTesterCase4 {
 public:
  UnpredictableUncondDecoderTester_Case4()
    : UnsafeUncondDecoderTesterCase4(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0000
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0000
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case5
    : public UnsafeUncondDecoderTesterCase5 {
 public:
  UnpredictableUncondDecoderTester_Case5()
    : UnsafeUncondDecoderTesterCase5(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0001 & inst(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Clrex_Rule_30_A1_P70'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0001 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Clrex_Rule_30_A1_P70}
class ForbiddenUncondDecoderTester_Case6
    : public UnsafeUncondDecoderTesterCase6 {
 public:
  ForbiddenUncondDecoderTester_Case6()
    : UnsafeUncondDecoderTesterCase6(
      state_.ForbiddenUncondDecoder_Clrex_Rule_30_A1_P70_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0100 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier {'constraints': ,
//     'rule': 'Dsb_Rule_42_A1_P92'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0100 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier {constraints: ,
//     rule: Dsb_Rule_42_A1_P92}
class DataBarrierTester_Case7
    : public DataBarrierTesterCase7 {
 public:
  DataBarrierTester_Case7()
    : DataBarrierTesterCase7(
      state_.DataBarrier_Dsb_Rule_42_A1_P92_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0101 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier {'constraints': ,
//     'rule': 'Dmb_Rule_41_A1_P90'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0101 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier {constraints: ,
//     rule: Dmb_Rule_41_A1_P90}
class DataBarrierTester_Case8
    : public DataBarrierTesterCase8 {
 public:
  DataBarrierTester_Case8()
    : DataBarrierTesterCase8(
      state_.DataBarrier_Dmb_Rule_41_A1_P90_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0110 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = InstructionBarrier {'constraints': ,
//     'rule': 'Isb_Rule_49_A1_P102'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0110 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = InstructionBarrier {constraints: ,
//     rule: Isb_Rule_49_A1_P102}
class InstructionBarrierTester_Case9
    : public InstructionBarrierTesterCase9 {
 public:
  InstructionBarrierTester_Case9()
    : InstructionBarrierTesterCase9(
      state_.InstructionBarrier_Isb_Rule_49_A1_P102_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0111
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0111
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case10
    : public UnsafeUncondDecoderTesterCase10 {
 public:
  UnpredictableUncondDecoderTester_Case10()
    : UnsafeUncondDecoderTesterCase10(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=001x
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=001x
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case11
    : public UnsafeUncondDecoderTesterCase11 {
 public:
  UnpredictableUncondDecoderTester_Case11()
    : UnsafeUncondDecoderTesterCase11(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=1xxx
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=1xxx
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case12
    : public UnsafeUncondDecoderTesterCase12 {
 public:
  UnpredictableUncondDecoderTester_Case12()
    : UnsafeUncondDecoderTesterCase12(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(25:20)=001001
//    = LoadRegisterList {'constraints': ,
//     'rule': 'Ldm_Ldmia_Ldmfd_Rule_53_A1_P110'}
//
// Representative case:
// op(25:20)=001001
//    = LoadRegisterList {constraints: ,
//     rule: Ldm_Ldmia_Ldmfd_Rule_53_A1_P110}
class LoadRegisterListTester_Case13
    : public LoadStoreRegisterListTesterCase13 {
 public:
  LoadRegisterListTester_Case13()
    : LoadStoreRegisterListTesterCase13(
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
class LoadRegisterListTester_Case14
    : public LoadStoreRegisterListTesterCase14 {
 public:
  LoadRegisterListTester_Case14()
    : LoadStoreRegisterListTesterCase14(
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
class LoadRegisterListTester_Case15
    : public LoadStoreRegisterListTesterCase15 {
 public:
  LoadRegisterListTester_Case15()
    : LoadStoreRegisterListTesterCase15(
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
class StoreRegisterListTester_Case16
    : public LoadStoreRegisterListTesterCase16 {
 public:
  StoreRegisterListTester_Case16()
    : LoadStoreRegisterListTesterCase16(
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
class StoreRegisterListTester_Case17
    : public LoadStoreRegisterListTesterCase17 {
 public:
  StoreRegisterListTester_Case17()
    : LoadStoreRegisterListTesterCase17(
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
class StoreRegisterListTester_Case18
    : public LoadStoreRegisterListTesterCase18 {
 public:
  StoreRegisterListTester_Case18()
    : LoadStoreRegisterListTesterCase18(
      state_.StoreRegisterList_Push_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(26:20)=100x001
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Unallocated_hints'}
//
// Representative case:
// op1(26:20)=100x001
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Unallocated_hints}
class ForbiddenUncondDecoderTester_Case19
    : public UnsafeUncondDecoderTesterCase19 {
 public:
  ForbiddenUncondDecoderTester_Case19()
    : UnsafeUncondDecoderTesterCase19(
      state_.ForbiddenUncondDecoder_Unallocated_hints_instance_)
  {}
};

// Neutral case:
// inst(26:20)=100x101 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {'constraints': ,
//     'rule': 'Pli_Rule_120_A1_P242'}
//
// Representative case:
// op1(26:20)=100x101 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: ,
//     rule: Pli_Rule_120_A1_P242}
class PreloadRegisterImm12OpTester_Case20
    : public PreloadRegisterImm12OpTesterCase20 {
 public:
  PreloadRegisterImm12OpTester_Case20()
    : PreloadRegisterImm12OpTesterCase20(
      state_.PreloadRegisterImm12Op_Pli_Rule_120_A1_P242_instance_)
  {}
};

// Neutral case:
// inst(26:20)=101x001 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {'constraints': & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     'rule': 'Pldw_Rule_117_A1_P236'}
//
// Representative case:
// op1(26:20)=101x001 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     rule: Pldw_Rule_117_A1_P236}
class PreloadRegisterImm12OpTester_Case21
    : public PreloadRegisterImm12OpTesterCase21 {
 public:
  PreloadRegisterImm12OpTester_Case21()
    : PreloadRegisterImm12OpTesterCase21(
      state_.PreloadRegisterImm12Op_Pldw_Rule_117_A1_P236_instance_)
  {}
};

// Neutral case:
// inst(26:20)=101x001 & inst(19:16)=1111
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=101x001 & Rn(19:16)=1111
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case22
    : public UnsafeUncondDecoderTesterCase22 {
 public:
  UnpredictableUncondDecoderTester_Case22()
    : UnsafeUncondDecoderTesterCase22(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(26:20)=101x101 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {'constraints': & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     'rule': 'Pld_Rule_117_A1_P236'}
//
// Representative case:
// op1(26:20)=101x101 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     rule: Pld_Rule_117_A1_P236}
class PreloadRegisterImm12OpTester_Case23
    : public PreloadRegisterImm12OpTesterCase23 {
 public:
  PreloadRegisterImm12OpTester_Case23()
    : PreloadRegisterImm12OpTesterCase23(
      state_.PreloadRegisterImm12Op_Pld_Rule_117_A1_P236_instance_)
  {}
};

// Neutral case:
// inst(26:20)=101x101 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {'constraints': ,
//     'rule': 'Pld_Rule_118_A1_P238'}
//
// Representative case:
// op1(26:20)=101x101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: ,
//     rule: Pld_Rule_118_A1_P238}
class PreloadRegisterImm12OpTester_Case24
    : public PreloadRegisterImm12OpTesterCase24 {
 public:
  PreloadRegisterImm12OpTester_Case24()
    : PreloadRegisterImm12OpTesterCase24(
      state_.PreloadRegisterImm12Op_Pld_Rule_118_A1_P238_instance_)
  {}
};

// Neutral case:
// inst(26:20)=110x001 & inst(7:4)=xxx0
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Unallocated_hints'}
//
// Representative case:
// op1(26:20)=110x001 & op2(7:4)=xxx0
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Unallocated_hints}
class ForbiddenUncondDecoderTester_Case25
    : public UnsafeUncondDecoderTesterCase25 {
 public:
  ForbiddenUncondDecoderTester_Case25()
    : UnsafeUncondDecoderTesterCase25(
      state_.ForbiddenUncondDecoder_Unallocated_hints_instance_)
  {}
};

// Neutral case:
// inst(26:20)=110x101 & inst(7:4)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOp {'constraints': ,
//     'rule': 'Pli_Rule_121_A1_P244'}
//
// Representative case:
// op1(26:20)=110x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOp {constraints: ,
//     rule: Pli_Rule_121_A1_P244}
class PreloadRegisterPairOpTester_Case26
    : public PreloadRegisterPairOpTesterCase26 {
 public:
  PreloadRegisterPairOpTester_Case26()
    : PreloadRegisterPairOpTesterCase26(
      state_.PreloadRegisterPairOp_Pli_Rule_121_A1_P244_instance_)
  {}
};

// Neutral case:
// inst(26:20)=111x001 & inst(7:4)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc {'constraints': ,
//     'rule': 'Pldw_Rule_119_A1_P240'}
//
// Representative case:
// op1(26:20)=111x001 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc {constraints: ,
//     rule: Pldw_Rule_119_A1_P240}
class PreloadRegisterPairOpWAndRnNotPcTester_Case27
    : public PreloadRegisterPairOpWAndRnNotPcTesterCase27 {
 public:
  PreloadRegisterPairOpWAndRnNotPcTester_Case27()
    : PreloadRegisterPairOpWAndRnNotPcTesterCase27(
      state_.PreloadRegisterPairOpWAndRnNotPc_Pldw_Rule_119_A1_P240_instance_)
  {}
};

// Neutral case:
// inst(26:20)=111x101 & inst(7:4)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc {'constraints': ,
//     'rule': 'Pld_Rule_119_A1_P240'}
//
// Representative case:
// op1(26:20)=111x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc {constraints: ,
//     rule: Pld_Rule_119_A1_P240}
class PreloadRegisterPairOpWAndRnNotPcTester_Case28
    : public PreloadRegisterPairOpWAndRnNotPcTesterCase28 {
 public:
  PreloadRegisterPairOpWAndRnNotPcTester_Case28()
    : PreloadRegisterPairOpWAndRnNotPcTesterCase28(
      state_.PreloadRegisterPairOpWAndRnNotPc_Pld_Rule_119_A1_P240_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1011x11
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1011x11
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case29
    : public UnsafeUncondDecoderTesterCase29 {
 public:
  UnpredictableUncondDecoderTester_Case29()
    : UnsafeUncondDecoderTesterCase29(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(24:20)=00100 & inst(19:16)=1111
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'rule': 'Adr_Rule_10_A2_P32'}
//
// Representative case:
// op(24:20)=00100 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: Adr_Rule_10_A2_P32}
class Unary1RegisterImmediateOpTester_Case30
    : public Unary1RegisterImmediateOpTesterCase30 {
 public:
  Unary1RegisterImmediateOpTester_Case30()
    : Unary1RegisterImmediateOpTesterCase30(
      state_.Unary1RegisterImmediateOp_Adr_Rule_10_A2_P32_instance_)
  {}
};

// Neutral case:
// inst(24:20)=00101 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Subs_Pc_Lr_and_related_instructions_Rule_A1a'}
//
// Representative case:
// op(24:20)=00101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Subs_Pc_Lr_and_related_instructions_Rule_A1a}
class ForbiddenCondDecoderTester_Case31
    : public UnsafeCondDecoderTesterCase31 {
 public:
  ForbiddenCondDecoderTester_Case31()
    : UnsafeCondDecoderTesterCase31(
      state_.ForbiddenCondDecoder_Subs_Pc_Lr_and_related_instructions_Rule_A1a_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01000 & inst(19:16)=1111
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'rule': 'Adr_Rule_10_A1_P32'}
//
// Representative case:
// op(24:20)=01000 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: Adr_Rule_10_A1_P32}
class Unary1RegisterImmediateOpTester_Case32
    : public Unary1RegisterImmediateOpTesterCase32 {
 public:
  Unary1RegisterImmediateOpTester_Case32()
    : Unary1RegisterImmediateOpTesterCase32(
      state_.Unary1RegisterImmediateOp_Adr_Rule_10_A1_P32_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01001 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Subs_Pc_Lr_and_related_instructions_Rule_A1b'}
//
// Representative case:
// op(24:20)=01001 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Subs_Pc_Lr_and_related_instructions_Rule_A1b}
class ForbiddenCondDecoderTester_Case33
    : public UnsafeCondDecoderTesterCase33 {
 public:
  ForbiddenCondDecoderTester_Case33()
    : UnsafeCondDecoderTesterCase33(
      state_.ForbiddenCondDecoder_Subs_Pc_Lr_and_related_instructions_Rule_A1b_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': ,
//     'rule': 'Tst_Rule_231_A1_P456'}
//
// Representative case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: ,
//     rule: Tst_Rule_231_A1_P456}
class Binary2RegisterImmedShiftedTestTester_Case34
    : public Binary2RegisterImmedShiftedTestTesterCase34 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case34()
    : Binary2RegisterImmedShiftedTestTesterCase34(
      state_.Binary2RegisterImmedShiftedTest_Tst_Rule_231_A1_P456_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = MaskedBinaryRegisterImmediateTest {'constraints': ,
//     'rule': 'Tst_Rule_230_A1_P454'}
//
// Representative case:
// op(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = MaskedBinaryRegisterImmediateTest {constraints: ,
//     rule: Tst_Rule_230_A1_P454}
class MaskedBinaryRegisterImmediateTestTester_Case35
    : public BinaryRegisterImmediateTestTesterCase35 {
 public:
  MaskedBinaryRegisterImmediateTestTester_Case35()
    : BinaryRegisterImmediateTestTesterCase35(
      state_.MaskedBinaryRegisterImmediateTest_Tst_Rule_230_A1_P454_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'rule': 'Tst_Rule_232_A1_P458',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: Tst_Rule_232_A1_P458,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTester_Case36
    : public Binary3RegisterShiftedTestTesterCase36 {
 public:
  Binary3RegisterShiftedTestTester_Case36()
    : Binary3RegisterShiftedTestTesterCase36(
      state_.Binary3RegisterShiftedTest_Tst_Rule_232_A1_P458_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': ,
//     'rule': 'Teq_Rule_228_A1_P450'}
//
// Representative case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: ,
//     rule: Teq_Rule_228_A1_P450}
class Binary2RegisterImmedShiftedTestTester_Case37
    : public Binary2RegisterImmedShiftedTestTesterCase37 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case37()
    : Binary2RegisterImmedShiftedTestTesterCase37(
      state_.Binary2RegisterImmedShiftedTest_Teq_Rule_228_A1_P450_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {'constraints': ,
//     'rule': 'Teq_Rule_227_A1_P448'}
//
// Representative case:
// op(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {constraints: ,
//     rule: Teq_Rule_227_A1_P448}
class BinaryRegisterImmediateTestTester_Case38
    : public BinaryRegisterImmediateTestTesterCase38 {
 public:
  BinaryRegisterImmediateTestTester_Case38()
    : BinaryRegisterImmediateTestTesterCase38(
      state_.BinaryRegisterImmediateTest_Teq_Rule_227_A1_P448_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'rule': 'Teq_Rule_229_A1_P452',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: Teq_Rule_229_A1_P452,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTester_Case39
    : public Binary3RegisterShiftedTestTesterCase39 {
 public:
  Binary3RegisterShiftedTestTester_Case39()
    : Binary3RegisterShiftedTestTesterCase39(
      state_.Binary3RegisterShiftedTest_Teq_Rule_229_A1_P452_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': ,
//     'rule': 'Cmp_Rule_36_A1_P82'}
//
// Representative case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: ,
//     rule: Cmp_Rule_36_A1_P82}
class Binary2RegisterImmedShiftedTestTester_Case40
    : public Binary2RegisterImmedShiftedTestTesterCase40 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case40()
    : Binary2RegisterImmedShiftedTestTesterCase40(
      state_.Binary2RegisterImmedShiftedTest_Cmp_Rule_36_A1_P82_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {'constraints': ,
//     'rule': 'Cmp_Rule_35_A1_P80'}
//
// Representative case:
// op(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {constraints: ,
//     rule: Cmp_Rule_35_A1_P80}
class BinaryRegisterImmediateTestTester_Case41
    : public BinaryRegisterImmediateTestTesterCase41 {
 public:
  BinaryRegisterImmediateTestTester_Case41()
    : BinaryRegisterImmediateTestTesterCase41(
      state_.BinaryRegisterImmediateTest_Cmp_Rule_35_A1_P80_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'rule': 'Cmp_Rule_37_A1_P84',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: Cmp_Rule_37_A1_P84,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTester_Case42
    : public Binary3RegisterShiftedTestTesterCase42 {
 public:
  Binary3RegisterShiftedTestTester_Case42()
    : Binary3RegisterShiftedTestTesterCase42(
      state_.Binary3RegisterShiftedTest_Cmp_Rule_37_A1_P84_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {'constraints': ,
//     'rule': 'Cmn_Rule_33_A1_P76'}
//
// Representative case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest {constraints: ,
//     rule: Cmn_Rule_33_A1_P76}
class Binary2RegisterImmedShiftedTestTester_Case43
    : public Binary2RegisterImmedShiftedTestTesterCase43 {
 public:
  Binary2RegisterImmedShiftedTestTester_Case43()
    : Binary2RegisterImmedShiftedTestTesterCase43(
      state_.Binary2RegisterImmedShiftedTest_Cmn_Rule_33_A1_P76_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {'constraints': ,
//     'rule': 'Cmn_Rule_32_A1_P74'}
//
// Representative case:
// op(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest {constraints: ,
//     rule: Cmn_Rule_32_A1_P74}
class BinaryRegisterImmediateTestTester_Case44
    : public BinaryRegisterImmediateTestTesterCase44 {
 public:
  BinaryRegisterImmediateTestTester_Case44()
    : BinaryRegisterImmediateTestTesterCase44(
      state_.BinaryRegisterImmediateTest_Cmn_Rule_32_A1_P74_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {'constraints': ,
//     'rule': 'Cmn_Rule_34_A1_P78',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest {constraints: ,
//     rule: Cmn_Rule_34_A1_P78,
//     safety: ['RegsNotPc']}
class Binary3RegisterShiftedTestTester_Case45
    : public Binary3RegisterShiftedTestTesterCase45 {
 public:
  Binary3RegisterShiftedTestTester_Case45()
    : Binary3RegisterShiftedTestTesterCase45(
      state_.Binary3RegisterShiftedTest_Cmn_Rule_34_A1_P78_instance_)
  {}
};

// Neutral case:
// inst(24:20)=11000 & inst(7:5)=000 & inst(15:12)=~1111
//    = Binary4RegisterDualOp {'constraints': ,
//     'rule': 'Usada8_Rule_254_A1_P502',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     rule: Usada8_Rule_254_A1_P502,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case46
    : public Binary4RegisterDualOpTesterCase46 {
 public:
  Binary4RegisterDualOpTester_Case46()
    : Binary4RegisterDualOpTesterCase46(
      state_.Binary4RegisterDualOp_Usada8_Rule_254_A1_P502_instance_)
  {}
};

// Neutral case:
// inst(24:20)=11000 & inst(7:5)=000 & inst(15:12)=1111
//    = Binary3RegisterOpAltA {'constraints': ,
//     'rule': 'Usad8_Rule_253_A1_P500',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: Usad8_Rule_253_A1_P500,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_Case47
    : public Binary3RegisterOpAltATesterCase47 {
 public:
  Binary3RegisterOpAltATester_Case47()
    : Binary3RegisterOpAltATesterCase47(
      state_.Binary3RegisterOpAltA_Usad8_Rule_253_A1_P500_instance_)
  {}
};

// Neutral case:
// inst(24:20)=11111 & inst(7:5)=111
//    = Roadblock {'constraints': ,
//     'rule': 'Udf_Rule_A1'}
//
// Representative case:
// op1(24:20)=11111 & op2(7:5)=111
//    = Roadblock {constraints: ,
//     rule: Udf_Rule_A1}
class RoadblockTester_Case48
    : public RoadblockTesterCase48 {
 public:
  RoadblockTester_Case48()
    : RoadblockTesterCase48(
      state_.Roadblock_Udf_Rule_A1_instance_)
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
class StoreRegisterListTester_Case49
    : public LoadStoreRegisterListTesterCase49 {
 public:
  StoreRegisterListTester_Case49()
    : LoadStoreRegisterListTesterCase49(
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
class LoadRegisterListTester_Case50
    : public LoadStoreRegisterListTesterCase50 {
 public:
  LoadRegisterListTester_Case50()
    : LoadStoreRegisterListTesterCase50(
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
class StoreRegisterListTester_Case51
    : public LoadStoreRegisterListTesterCase51 {
 public:
  StoreRegisterListTester_Case51()
    : LoadStoreRegisterListTesterCase51(
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
class LoadRegisterListTester_Case52
    : public LoadStoreRegisterListTesterCase52 {
 public:
  LoadRegisterListTester_Case52()
    : LoadStoreRegisterListTesterCase52(
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
class StoreRegisterListTester_Case53
    : public LoadStoreRegisterListTesterCase53 {
 public:
  StoreRegisterListTester_Case53()
    : LoadStoreRegisterListTesterCase53(
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
class LoadRegisterListTester_Case54
    : public LoadStoreRegisterListTesterCase54 {
 public:
  LoadRegisterListTester_Case54()
    : LoadStoreRegisterListTesterCase54(
      state_.LoadRegisterList_Ldmib_Ldmed_Rule_56_A1_P116_instance_)
  {}
};

// Neutral case:
// inst(26:20)=100xx11
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=100xx11
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case55
    : public UnsafeUncondDecoderTesterCase55 {
 public:
  UnpredictableUncondDecoderTester_Case55()
    : UnsafeUncondDecoderTesterCase55(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(27:20)=100xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxx0000101000000000
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Rfe_Rule_B6_1_10_A1_B6_16'}
//
// Representative case:
// op1(27:20)=100xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000101000000000
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Rfe_Rule_B6_1_10_A1_B6_16}
class ForbiddenUncondDecoderTester_Case56
    : public UnsafeUncondDecoderTesterCase56 {
 public:
  ForbiddenUncondDecoderTester_Case56()
    : UnsafeUncondDecoderTesterCase56(
      state_.ForbiddenUncondDecoder_Rfe_Rule_B6_1_10_A1_B6_16_instance_)
  {}
};

// Neutral case:
// inst(27:20)=100xx1x0 & inst(31:0)=xxxxxxxxxxxx110100000101000xxxxx
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Srs_Rule_B6_1_10_A1_B6_20'}
//
// Representative case:
// op1(27:20)=100xx1x0 & $pattern(31:0)=xxxxxxxxxxxx110100000101000xxxxx
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Srs_Rule_B6_1_10_A1_B6_20}
class ForbiddenUncondDecoderTester_Case57
    : public UnsafeUncondDecoderTesterCase57 {
 public:
  ForbiddenUncondDecoderTester_Case57()
    : UnsafeUncondDecoderTesterCase57(
      state_.ForbiddenUncondDecoder_Srs_Rule_B6_1_10_A1_B6_20_instance_)
  {}
};

// Neutral case:
// inst(27:20)=1110xxx0 & inst(4)=1
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Mcr2_Rule_92_A2_P186'}
//
// Representative case:
// op1(27:20)=1110xxx0 & op(4)=1
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Mcr2_Rule_92_A2_P186}
class ForbiddenUncondDecoderTester_Case58
    : public UnsafeUncondDecoderTesterCase58 {
 public:
  ForbiddenUncondDecoderTester_Case58()
    : UnsafeUncondDecoderTesterCase58(
      state_.ForbiddenUncondDecoder_Mcr2_Rule_92_A2_P186_instance_)
  {}
};

// Neutral case:
// inst(27:20)=1110xxx1 & inst(4)=1
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Mrc2_Rule_100_A2_P202'}
//
// Representative case:
// op1(27:20)=1110xxx1 & op(4)=1
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Mrc2_Rule_100_A2_P202}
class ForbiddenUncondDecoderTester_Case59
    : public UnsafeUncondDecoderTesterCase59 {
 public:
  ForbiddenUncondDecoderTester_Case59()
    : UnsafeUncondDecoderTesterCase59(
      state_.ForbiddenUncondDecoder_Mrc2_Rule_100_A2_P202_instance_)
  {}
};

// Neutral case:
// inst(19:16)=0000 & inst(7:6)=01
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vmov_Rule_327_A2_P642'}
//
// Representative case:
// opc2(19:16)=0000 & opc3(7:6)=01
//    = CondVfpOp {constraints: ,
//     rule: Vmov_Rule_327_A2_P642}
class CondVfpOpTester_Case60
    : public CondVfpOpTesterCase60 {
 public:
  CondVfpOpTester_Case60()
    : CondVfpOpTesterCase60(
      state_.CondVfpOp_Vmov_Rule_327_A2_P642_instance_)
  {}
};

// Neutral case:
// inst(19:16)=0000 & inst(7:6)=11
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vabs_Rule_269_A2_P532'}
//
// Representative case:
// opc2(19:16)=0000 & opc3(7:6)=11
//    = CondVfpOp {constraints: ,
//     rule: Vabs_Rule_269_A2_P532}
class CondVfpOpTester_Case61
    : public CondVfpOpTesterCase61 {
 public:
  CondVfpOpTester_Case61()
    : CondVfpOpTesterCase61(
      state_.CondVfpOp_Vabs_Rule_269_A2_P532_instance_)
  {}
};

// Neutral case:
// inst(19:16)=0001 & inst(7:6)=01
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vneg_Rule_342_A2_P672'}
//
// Representative case:
// opc2(19:16)=0001 & opc3(7:6)=01
//    = CondVfpOp {constraints: ,
//     rule: Vneg_Rule_342_A2_P672}
class CondVfpOpTester_Case62
    : public CondVfpOpTesterCase62 {
 public:
  CondVfpOpTester_Case62()
    : CondVfpOpTesterCase62(
      state_.CondVfpOp_Vneg_Rule_342_A2_P672_instance_)
  {}
};

// Neutral case:
// inst(19:16)=0001 & inst(7:6)=11
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vsqrt_Rule_388_A1_P762'}
//
// Representative case:
// opc2(19:16)=0001 & opc3(7:6)=11
//    = CondVfpOp {constraints: ,
//     rule: Vsqrt_Rule_388_A1_P762}
class CondVfpOpTester_Case63
    : public CondVfpOpTesterCase63 {
 public:
  CondVfpOpTester_Case63()
    : CondVfpOpTesterCase63(
      state_.CondVfpOp_Vsqrt_Rule_388_A1_P762_instance_)
  {}
};

// Neutral case:
// inst(19:16)=0100 & inst(7:6)=x1
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vcmp_Vcmpe_Rule_A1'}
//
// Representative case:
// opc2(19:16)=0100 & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: Vcmp_Vcmpe_Rule_A1}
class CondVfpOpTester_Case64
    : public CondVfpOpTesterCase64 {
 public:
  CondVfpOpTester_Case64()
    : CondVfpOpTesterCase64(
      state_.CondVfpOp_Vcmp_Vcmpe_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(19:16)=0101 & inst(7:6)=x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vcmp_Vcmpe_Rule_A2'}
//
// Representative case:
// opc2(19:16)=0101 & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
//    = CondVfpOp {constraints: ,
//     rule: Vcmp_Vcmpe_Rule_A2}
class CondVfpOpTester_Case65
    : public CondVfpOpTesterCase65 {
 public:
  CondVfpOpTester_Case65()
    : CondVfpOpTesterCase65(
      state_.CondVfpOp_Vcmp_Vcmpe_Rule_A2_instance_)
  {}
};

// Neutral case:
// inst(19:16)=0111 & inst(7:6)=11
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vcvt_Rule_298_A1_P584'}
//
// Representative case:
// opc2(19:16)=0111 & opc3(7:6)=11
//    = CondVfpOp {constraints: ,
//     rule: Vcvt_Rule_298_A1_P584}
class CondVfpOpTester_Case66
    : public CondVfpOpTesterCase66 {
 public:
  CondVfpOpTester_Case66()
    : CondVfpOpTesterCase66(
      state_.CondVfpOp_Vcvt_Rule_298_A1_P584_instance_)
  {}
};

// Neutral case:
// inst(19:16)=1000 & inst(7:6)=x1
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vcvt_Vcvtr_Rule_295_A1_P578'}
//
// Representative case:
// opc2(19:16)=1000 & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: Vcvt_Vcvtr_Rule_295_A1_P578}
class CondVfpOpTester_Case67
    : public CondVfpOpTesterCase67 {
 public:
  CondVfpOpTester_Case67()
    : CondVfpOpTesterCase67(
      state_.CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578_instance_)
  {}
};

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
class Binary4RegisterDualResultTester_Case68
    : public Binary4RegisterDualResultTesterCase68 {
 public:
  Binary4RegisterDualResultTester_Case68()
    : Binary4RegisterDualResultTesterCase68(
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
class UndefinedCondDecoderTester_Case69
    : public UnsafeCondDecoderTesterCase69 {
 public:
  UndefinedCondDecoderTester_Case69()
    : UnsafeCondDecoderTesterCase69(
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
class Binary4RegisterDualOpTester_Case70
    : public Binary4RegisterDualOpTesterCase70 {
 public:
  Binary4RegisterDualOpTester_Case70()
    : Binary4RegisterDualOpTesterCase70(
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
class UndefinedCondDecoderTester_Case71
    : public UnsafeCondDecoderTesterCase71 {
 public:
  UndefinedCondDecoderTester_Case71()
    : UnsafeCondDecoderTesterCase71(
      state_.UndefinedCondDecoder_Undefined_A5_2_5_0111_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp {'constraints': ,
//     'rule': 'Strex_Rule_202_A1_P400'}
//
// Representative case:
// op(23:20)=1000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp {constraints: ,
//     rule: Strex_Rule_202_A1_P400}
class StoreExclusive3RegisterOpTester_Case72
    : public StoreExclusive3RegisterOpTesterCase72 {
 public:
  StoreExclusive3RegisterOpTester_Case72()
    : StoreExclusive3RegisterOpTesterCase72(
      state_.StoreExclusive3RegisterOp_Strex_Rule_202_A1_P400_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterOp {'constraints': ,
//     'rule': 'Ldrex_Rule_69_A1_P142'}
//
// Representative case:
// op(23:20)=1001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterOp {constraints: ,
//     rule: Ldrex_Rule_69_A1_P142}
class LoadExclusive2RegisterOpTester_Case73
    : public LoadExclusive2RegisterOpTesterCase73 {
 public:
  LoadExclusive2RegisterOpTester_Case73()
    : LoadExclusive2RegisterOpTesterCase73(
      state_.LoadExclusive2RegisterOp_Ldrex_Rule_69_A1_P142_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterDoubleOp {'constraints': ,
//     'rule': 'Strexd_Rule_204_A1_P404'}
//
// Representative case:
// op(23:20)=1010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterDoubleOp {constraints: ,
//     rule: Strexd_Rule_204_A1_P404}
class StoreExclusive3RegisterDoubleOpTester_Case74
    : public StoreExclusive3RegisterDoubleOpTesterCase74 {
 public:
  StoreExclusive3RegisterDoubleOpTester_Case74()
    : StoreExclusive3RegisterDoubleOpTesterCase74(
      state_.StoreExclusive3RegisterDoubleOp_Strexd_Rule_204_A1_P404_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterDoubleOp {'constraints': ,
//     'rule': 'Ldrexd_Rule_71_A1_P146'}
//
// Representative case:
// op(23:20)=1011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterDoubleOp {constraints: ,
//     rule: Ldrexd_Rule_71_A1_P146}
class LoadExclusive2RegisterDoubleOpTester_Case75
    : public LoadExclusive2RegisterDoubleOpTesterCase75 {
 public:
  LoadExclusive2RegisterDoubleOpTester_Case75()
    : LoadExclusive2RegisterDoubleOpTesterCase75(
      state_.LoadExclusive2RegisterDoubleOp_Ldrexd_Rule_71_A1_P146_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp {'constraints': ,
//     'rule': 'Strexb_Rule_203_A1_P402'}
//
// Representative case:
// op(23:20)=1100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp {constraints: ,
//     rule: Strexb_Rule_203_A1_P402}
class StoreExclusive3RegisterOpTester_Case76
    : public StoreExclusive3RegisterOpTesterCase76 {
 public:
  StoreExclusive3RegisterOpTester_Case76()
    : StoreExclusive3RegisterOpTesterCase76(
      state_.StoreExclusive3RegisterOp_Strexb_Rule_203_A1_P402_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1101 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterOp {'constraints': ,
//     'rule': 'Ldrexb_Rule_70_A1_P144'}
//
// Representative case:
// op(23:20)=1101 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterOp {constraints: ,
//     rule: Ldrexb_Rule_70_A1_P144}
class LoadExclusive2RegisterOpTester_Case77
    : public LoadExclusive2RegisterOpTesterCase77 {
 public:
  LoadExclusive2RegisterOpTester_Case77()
    : LoadExclusive2RegisterOpTesterCase77(
      state_.LoadExclusive2RegisterOp_Ldrexb_Rule_70_A1_P144_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1110 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp {'constraints': ,
//     'rule': 'Strexh_Rule_205_A1_P406'}
//
// Representative case:
// op(23:20)=1110 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp {constraints: ,
//     rule: Strexh_Rule_205_A1_P406}
class StoreExclusive3RegisterOpTester_Case78
    : public StoreExclusive3RegisterOpTesterCase78 {
 public:
  StoreExclusive3RegisterOpTester_Case78()
    : StoreExclusive3RegisterOpTesterCase78(
      state_.StoreExclusive3RegisterOp_Strexh_Rule_205_A1_P406_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterOp {'constraints': ,
//     'rule': 'Ldrexh_Rule_72_A1_P148'}
//
// Representative case:
// op(23:20)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterOp {constraints: ,
//     rule: Ldrexh_Rule_72_A1_P148}
class LoadExclusive2RegisterOpTester_Case79
    : public LoadExclusive2RegisterOpTesterCase79 {
 public:
  LoadExclusive2RegisterOpTester_Case79()
    : LoadExclusive2RegisterOpTesterCase79(
      state_.LoadExclusive2RegisterOp_Ldrexh_Rule_72_A1_P148_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x00
//    = StoreVectorRegisterList {'constraints': ,
//     'rule': 'Vstm_Rule_399_A1_A2_P784'}
//
// Representative case:
// opcode(24:20)=01x00
//    = StoreVectorRegisterList {constraints: ,
//     rule: Vstm_Rule_399_A1_A2_P784}
class StoreVectorRegisterListTester_Case80
    : public StoreVectorRegisterListTesterCase80 {
 public:
  StoreVectorRegisterListTester_Case80()
    : StoreVectorRegisterListTesterCase80(
      state_.StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x01
//    = LoadVectorRegisterList {'constraints': ,
//     'rule': 'Vldm_Rule_319_A1_A2_P626'}
//
// Representative case:
// opcode(24:20)=01x01
//    = LoadVectorRegisterList {constraints: ,
//     rule: Vldm_Rule_319_A1_A2_P626}
class LoadVectorRegisterListTester_Case81
    : public LoadStoreVectorRegisterListTesterCase81 {
 public:
  LoadVectorRegisterListTester_Case81()
    : LoadStoreVectorRegisterListTesterCase81(
      state_.LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x10
//    = StoreVectorRegisterList {'constraints': ,
//     'rule': 'Vstm_Rule_399_A1_A2_P784'}
//
// Representative case:
// opcode(24:20)=01x10
//    = StoreVectorRegisterList {constraints: ,
//     rule: Vstm_Rule_399_A1_A2_P784}
class StoreVectorRegisterListTester_Case82
    : public StoreVectorRegisterListTesterCase82 {
 public:
  StoreVectorRegisterListTester_Case82()
    : StoreVectorRegisterListTesterCase82(
      state_.StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=~1101
//    = LoadVectorRegisterList {'constraints': ,
//     'rule': 'Vldm_Rule_319_A1_A2_P626',
//     'safety': ["'NotRnIsSp'"]}
//
// Representative case:
// opcode(24:20)=01x11 & Rn(19:16)=~1101
//    = LoadVectorRegisterList {constraints: ,
//     rule: Vldm_Rule_319_A1_A2_P626,
//     safety: ['NotRnIsSp']}
class LoadVectorRegisterListTester_Case83
    : public LoadStoreVectorRegisterListTesterCase83 {
 public:
  LoadVectorRegisterListTester_Case83()
    : LoadStoreVectorRegisterListTesterCase83(
      state_.LoadVectorRegisterList_Vldm_Rule_319_A1_A2_P626_instance_)
  {}
};

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=1101
//    = LoadVectorRegisterList {'constraints': ,
//     'rule': 'Vpop_Rule_354_A1_A2_P694'}
//
// Representative case:
// opcode(24:20)=01x11 & Rn(19:16)=1101
//    = LoadVectorRegisterList {constraints: ,
//     rule: Vpop_Rule_354_A1_A2_P694}
class LoadVectorRegisterListTester_Case84
    : public LoadStoreVectorRegisterListTesterCase84 {
 public:
  LoadVectorRegisterListTester_Case84()
    : LoadStoreVectorRegisterListTesterCase84(
      state_.LoadVectorRegisterList_Vpop_Rule_354_A1_A2_P694_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=~1101
//    = StoreVectorRegisterList {'constraints': ,
//     'rule': 'Vstm_Rule_399_A1_A2_P784',
//     'safety': ["'NotRnIsSp'"]}
//
// Representative case:
// opcode(24:20)=10x10 & Rn(19:16)=~1101
//    = StoreVectorRegisterList {constraints: ,
//     rule: Vstm_Rule_399_A1_A2_P784,
//     safety: ['NotRnIsSp']}
class StoreVectorRegisterListTester_Case85
    : public StoreVectorRegisterListTesterCase85 {
 public:
  StoreVectorRegisterListTester_Case85()
    : StoreVectorRegisterListTesterCase85(
      state_.StoreVectorRegisterList_Vstm_Rule_399_A1_A2_P784_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=1101
//    = StoreVectorRegisterList {'constraints': ,
//     'rule': 'Vpush_355_A1_A2_P696'}
//
// Representative case:
// opcode(24:20)=10x10 & Rn(19:16)=1101
//    = StoreVectorRegisterList {constraints: ,
//     rule: Vpush_355_A1_A2_P696}
class StoreVectorRegisterListTester_Case86
    : public StoreVectorRegisterListTesterCase86 {
 public:
  StoreVectorRegisterListTester_Case86()
    : StoreVectorRegisterListTesterCase86(
      state_.StoreVectorRegisterList_Vpush_355_A1_A2_P696_instance_)
  {}
};

// Neutral case:
// inst(24:20)=10x11
//    = LoadVectorRegisterList {'constraints': ,
//     'rule': 'Vldm_Rule_318_A1_A2_P626'}
//
// Representative case:
// opcode(24:20)=10x11
//    = LoadVectorRegisterList {constraints: ,
//     rule: Vldm_Rule_318_A1_A2_P626}
class LoadVectorRegisterListTester_Case87
    : public LoadStoreVectorRegisterListTesterCase87 {
 public:
  LoadVectorRegisterListTester_Case87()
    : LoadStoreVectorRegisterListTesterCase87(
      state_.LoadVectorRegisterList_Vldm_Rule_318_A1_A2_P626_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'And_Rule_7_A1_P36',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: And_Rule_7_A1_P36,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_Case88
    : public Binary3RegisterImmedShiftedOpTesterCase88 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case88()
    : Binary3RegisterImmedShiftedOpTesterCase88(
      state_.Binary3RegisterImmedShiftedOp_And_Rule_7_A1_P36_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0000x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'And_Rule_11_A1_P34',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0000x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: And_Rule_11_A1_P34,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_Case89
    : public Binary2RegisterImmediateOpTesterCase89 {
 public:
  Binary2RegisterImmediateOpTester_Case89()
    : Binary2RegisterImmediateOpTesterCase89(
      state_.Binary2RegisterImmediateOp_And_Rule_11_A1_P34_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0000x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'And_Rule_13_A1_P38',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0000x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: And_Rule_13_A1_P38,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case90
    : public Binary4RegisterShiftedOpTesterCase90 {
 public:
  Binary4RegisterShiftedOpTester_Case90()
    : Binary4RegisterShiftedOpTesterCase90(
      state_.Binary4RegisterShiftedOp_And_Rule_13_A1_P38_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0001x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Eor_Rule_45_A1_P96',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0001x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Eor_Rule_45_A1_P96,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_Case91
    : public Binary3RegisterImmedShiftedOpTesterCase91 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case91()
    : Binary3RegisterImmedShiftedOpTesterCase91(
      state_.Binary3RegisterImmedShiftedOp_Eor_Rule_45_A1_P96_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0001x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Eor_Rule_44_A1_P94',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0001x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Eor_Rule_44_A1_P94,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_Case92
    : public Binary2RegisterImmediateOpTesterCase92 {
 public:
  Binary2RegisterImmediateOpTester_Case92()
    : Binary2RegisterImmediateOpTesterCase92(
      state_.Binary2RegisterImmediateOp_Eor_Rule_44_A1_P94_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0001x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Eor_Rule_46_A1_P98',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0001x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Eor_Rule_46_A1_P98,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case93
    : public Binary4RegisterShiftedOpTesterCase93 {
 public:
  Binary4RegisterShiftedOpTester_Case93()
    : Binary4RegisterShiftedOpTesterCase93(
      state_.Binary4RegisterShiftedOp_Eor_Rule_46_A1_P98_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0010x & inst(19:16)=~1111
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Sub_Rule_212_A1_P420',
//     'safety': ["'NeitherRdIsPcAndSNorRnIsPcAndNotS'"]}
//
// Representative case:
// op(24:20)=0010x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Sub_Rule_212_A1_P420,
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
class Binary2RegisterImmediateOpTester_Case94
    : public Binary2RegisterImmediateOpTesterCase94 {
 public:
  Binary2RegisterImmediateOpTester_Case94()
    : Binary2RegisterImmediateOpTesterCase94(
      state_.Binary2RegisterImmediateOp_Sub_Rule_212_A1_P420_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0010x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Sub_Rule_213_A1_P422',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0010x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Sub_Rule_213_A1_P422,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_Case95
    : public Binary3RegisterImmedShiftedOpTesterCase95 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case95()
    : Binary3RegisterImmedShiftedOpTesterCase95(
      state_.Binary3RegisterImmedShiftedOp_Sub_Rule_213_A1_P422_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0010x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Sub_Rule_214_A1_P424',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0010x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Sub_Rule_214_A1_P424,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case96
    : public Binary4RegisterShiftedOpTesterCase96 {
 public:
  Binary4RegisterShiftedOpTester_Case96()
    : Binary4RegisterShiftedOpTesterCase96(
      state_.Binary4RegisterShiftedOp_Sub_Rule_214_A1_P424_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0011x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Rsb_Rule_143_P286',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0011x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Rsb_Rule_143_P286,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_Case97
    : public Binary3RegisterImmedShiftedOpTesterCase97 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case97()
    : Binary3RegisterImmedShiftedOpTesterCase97(
      state_.Binary3RegisterImmedShiftedOp_Rsb_Rule_143_P286_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0011x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Rsb_Rule_142_A1_P284',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0011x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Rsb_Rule_142_A1_P284,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_Case98
    : public Binary2RegisterImmediateOpTesterCase98 {
 public:
  Binary2RegisterImmediateOpTester_Case98()
    : Binary2RegisterImmediateOpTesterCase98(
      state_.Binary2RegisterImmediateOp_Rsb_Rule_142_A1_P284_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0011x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Rsb_Rule_144_A1_P288',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0011x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Rsb_Rule_144_A1_P288,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case99
    : public Binary4RegisterShiftedOpTesterCase99 {
 public:
  Binary4RegisterShiftedOpTester_Case99()
    : Binary4RegisterShiftedOpTesterCase99(
      state_.Binary4RegisterShiftedOp_Rsb_Rule_144_A1_P288_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0100x & inst(19:16)=~1111
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Add_Rule_5_A1_P22',
//     'safety': ["'NeitherRdIsPcAndSNorRnIsPcAndNotS'"]}
//
// Representative case:
// op(24:20)=0100x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Add_Rule_5_A1_P22,
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
class Binary2RegisterImmediateOpTester_Case100
    : public Binary2RegisterImmediateOpTesterCase100 {
 public:
  Binary2RegisterImmediateOpTester_Case100()
    : Binary2RegisterImmediateOpTesterCase100(
      state_.Binary2RegisterImmediateOp_Add_Rule_5_A1_P22_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0100x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Add_Rule_6_A1_P24',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0100x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Add_Rule_6_A1_P24,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_Case101
    : public Binary3RegisterImmedShiftedOpTesterCase101 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case101()
    : Binary3RegisterImmedShiftedOpTesterCase101(
      state_.Binary3RegisterImmedShiftedOp_Add_Rule_6_A1_P24_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0100x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Add_Rule_7_A1_P26',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Add_Rule_7_A1_P26,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case102
    : public Binary4RegisterShiftedOpTesterCase102 {
 public:
  Binary4RegisterShiftedOpTester_Case102()
    : Binary4RegisterShiftedOpTesterCase102(
      state_.Binary4RegisterShiftedOp_Add_Rule_7_A1_P26_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Adc_Rule_2_A1_P16',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Adc_Rule_2_A1_P16,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_Case103
    : public Binary3RegisterImmedShiftedOpTesterCase103 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case103()
    : Binary3RegisterImmedShiftedOpTesterCase103(
      state_.Binary3RegisterImmedShiftedOp_Adc_Rule_2_A1_P16_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0101x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Adc_Rule_6_A1_P14',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0101x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Adc_Rule_6_A1_P14,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_Case104
    : public Binary2RegisterImmediateOpTesterCase104 {
 public:
  Binary2RegisterImmediateOpTester_Case104()
    : Binary2RegisterImmediateOpTesterCase104(
      state_.Binary2RegisterImmediateOp_Adc_Rule_6_A1_P14_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0101x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Adc_Rule_3_A1_P18',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0101x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Adc_Rule_3_A1_P18,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case105
    : public Binary4RegisterShiftedOpTesterCase105 {
 public:
  Binary4RegisterShiftedOpTester_Case105()
    : Binary4RegisterShiftedOpTesterCase105(
      state_.Binary4RegisterShiftedOp_Adc_Rule_3_A1_P18_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0110x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Sbc_Rule_152_A1_P304',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0110x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Sbc_Rule_152_A1_P304,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_Case106
    : public Binary3RegisterImmedShiftedOpTesterCase106 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case106()
    : Binary3RegisterImmedShiftedOpTesterCase106(
      state_.Binary3RegisterImmedShiftedOp_Sbc_Rule_152_A1_P304_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0110x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Sbc_Rule_151_A1_P302',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0110x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Sbc_Rule_151_A1_P302,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_Case107
    : public Binary2RegisterImmediateOpTesterCase107 {
 public:
  Binary2RegisterImmediateOpTester_Case107()
    : Binary2RegisterImmediateOpTesterCase107(
      state_.Binary2RegisterImmediateOp_Sbc_Rule_151_A1_P302_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0110x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Sbc_Rule_153_A1_P306',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Sbc_Rule_153_A1_P306,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case108
    : public Binary4RegisterShiftedOpTesterCase108 {
 public:
  Binary4RegisterShiftedOpTester_Case108()
    : Binary4RegisterShiftedOpTesterCase108(
      state_.Binary4RegisterShiftedOp_Sbc_Rule_153_A1_P306_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Rsc_Rule_146_A1_P292',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Rsc_Rule_146_A1_P292,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_Case109
    : public Binary3RegisterImmedShiftedOpTesterCase109 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case109()
    : Binary3RegisterImmedShiftedOpTesterCase109(
      state_.Binary3RegisterImmedShiftedOp_Rsc_Rule_146_A1_P292_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0111x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Rsc_Rule_145_A1_P290',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0111x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Rsc_Rule_145_A1_P290,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_Case110
    : public Binary2RegisterImmediateOpTesterCase110 {
 public:
  Binary2RegisterImmediateOpTester_Case110()
    : Binary2RegisterImmediateOpTesterCase110(
      state_.Binary2RegisterImmediateOp_Rsc_Rule_145_A1_P290_instance_)
  {}
};

// Neutral case:
// inst(24:20)=0111x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Rsc_Rule_147_A1_P294',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0111x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Rsc_Rule_147_A1_P294,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case111
    : public Binary4RegisterShiftedOpTesterCase111 {
 public:
  Binary4RegisterShiftedOpTester_Case111()
    : Binary4RegisterShiftedOpTesterCase111(
      state_.Binary4RegisterShiftedOp_Rsc_Rule_147_A1_P294_instance_)
  {}
};

// Neutral case:
// inst(24:21)=1001 & inst(19:16)=1101 & inst(11:0)=000000000100
//    = Store2RegisterImm12OpRnNotRtOnWriteback {'constraints': ,
//     'rule': 'Push_Rule_123_A2_P248'}
//
// Representative case:
// Flags(24:21)=1001 & Rn(19:16)=1101 & Imm12(11:0)=000000000100
//    = Store2RegisterImm12OpRnNotRtOnWriteback {constraints: ,
//     rule: Push_Rule_123_A2_P248}
class Store2RegisterImm12OpRnNotRtOnWritebackTester_Case112
    : public LoadStore2RegisterImm12OpTesterCase112 {
 public:
  Store2RegisterImm12OpRnNotRtOnWritebackTester_Case112()
    : LoadStore2RegisterImm12OpTesterCase112(
      state_.Store2RegisterImm12OpRnNotRtOnWriteback_Push_Rule_123_A2_P248_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1100x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Orr_Rule_114_A1_P230',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1100x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Orr_Rule_114_A1_P230,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_Case113
    : public Binary3RegisterImmedShiftedOpTesterCase113 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case113()
    : Binary3RegisterImmedShiftedOpTesterCase113(
      state_.Binary3RegisterImmedShiftedOp_Orr_Rule_114_A1_P230_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1100x
//    = Binary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Orr_Rule_113_A1_P228',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1100x
//    = Binary2RegisterImmediateOp {constraints: ,
//     rule: Orr_Rule_113_A1_P228,
//     safety: ['NotRdIsPcAndS']}
class Binary2RegisterImmediateOpTester_Case114
    : public Binary2RegisterImmediateOpTesterCase114 {
 public:
  Binary2RegisterImmediateOpTester_Case114()
    : Binary2RegisterImmediateOpTesterCase114(
      state_.Binary2RegisterImmediateOp_Orr_Rule_113_A1_P228_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1100x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Orr_Rule_115_A1_P212',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1100x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Orr_Rule_115_A1_P212,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case115
    : public Binary4RegisterShiftedOpTesterCase115 {
 public:
  Binary4RegisterShiftedOpTester_Case115()
    : Binary4RegisterShiftedOpTesterCase115(
      state_.Binary4RegisterShiftedOp_Orr_Rule_115_A1_P212_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Lsl_Rule_88_A1_P178',
//     'safety': ["'NeitherImm5NotZeroNorNotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     rule: Lsl_Rule_88_A1_P178,
//     safety: ['NeitherImm5NotZeroNorNotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTester_Case116
    : public Unary2RegisterImmedShiftedOpTesterCase116 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case116()
    : Unary2RegisterImmedShiftedOpTesterCase116(
      state_.Unary2RegisterImmedShiftedOp_Lsl_Rule_88_A1_P178_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Ror_Rule_139_A1_P278',
//     'safety': ["'NeitherImm5NotZeroNorNotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     rule: Ror_Rule_139_A1_P278,
//     safety: ['NeitherImm5NotZeroNorNotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTester_Case117
    : public Unary2RegisterImmedShiftedOpTesterCase117 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case117()
    : Unary2RegisterImmedShiftedOpTesterCase117(
      state_.Unary2RegisterImmedShiftedOp_Ror_Rule_139_A1_P278_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {'constraints': ,
//     'rule': 'Mov_Rule_97_A1_P196',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {constraints: ,
//     rule: Mov_Rule_97_A1_P196,
//     safety: ['NotRdIsPcAndS']}
class Unary2RegisterOpTester_Case118
    : public Unary2RegisterOpTesterCase118 {
 public:
  Unary2RegisterOpTester_Case118()
    : Unary2RegisterOpTesterCase118(
      state_.Unary2RegisterOp_Mov_Rule_97_A1_P196_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {'constraints': ,
//     'rule': 'Rrx_Rule_141_A1_P282',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp {constraints: ,
//     rule: Rrx_Rule_141_A1_P282,
//     safety: ['NotRdIsPcAndS']}
class Unary2RegisterOpTester_Case119
    : public Unary2RegisterOpTesterCase119 {
 public:
  Unary2RegisterOpTester_Case119()
    : Unary2RegisterOpTesterCase119(
      state_.Unary2RegisterOp_Rrx_Rule_141_A1_P282_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'rule': 'Lsl_Rule_89_A1_P180',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     rule: Lsl_Rule_89_A1_P180,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTester_Case120
    : public Binary3RegisterOpTesterCase120 {
 public:
  Binary3RegisterOpTester_Case120()
    : Binary3RegisterOpTesterCase120(
      state_.Binary3RegisterOp_Lsl_Rule_89_A1_P180_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'rule': 'Lsr_Rule_91_A1_P184',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     rule: Lsr_Rule_91_A1_P184,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTester_Case121
    : public Binary3RegisterOpTesterCase121 {
 public:
  Binary3RegisterOpTester_Case121()
    : Binary3RegisterOpTesterCase121(
      state_.Binary3RegisterOp_Lsr_Rule_91_A1_P184_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'rule': 'Asr_Rule_15_A1_P42',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     rule: Asr_Rule_15_A1_P42,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTester_Case122
    : public Binary3RegisterOpTesterCase122 {
 public:
  Binary3RegisterOpTester_Case122()
    : Binary3RegisterOpTesterCase122(
      state_.Binary3RegisterOp_Asr_Rule_15_A1_P42_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract {'constraints': ,
//     'rule': 'Sbfx_Rule_154_A1_P308',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract {constraints: ,
//     rule: Sbfx_Rule_154_A1_P308,
//     safety: ['RegsNotPc']}
class Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case123
    : public Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase123 {
 public:
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case123()
    : Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase123(
      state_.Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_Sbfx_Rule_154_A1_P308_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {'constraints': ,
//     'rule': 'Ror_Rule_140_A1_P280',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp {constraints: ,
//     rule: Ror_Rule_140_A1_P280,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpTester_Case124
    : public Binary3RegisterOpTesterCase124 {
 public:
  Binary3RegisterOpTester_Case124()
    : Binary3RegisterOpTesterCase124(
      state_.Binary3RegisterOp_Ror_Rule_140_A1_P280_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Lsr_Rule_90_A1_P182',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1101x & op3(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     rule: Lsr_Rule_90_A1_P182,
//     safety: ['NotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTester_Case125
    : public Unary2RegisterImmedShiftedOpTesterCase125 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case125()
    : Unary2RegisterImmedShiftedOpTesterCase125(
      state_.Unary2RegisterImmedShiftedOp_Lsr_Rule_90_A1_P182_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Asr_Rule_14_A1_P40',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1101x & op3(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     rule: Asr_Rule_14_A1_P40,
//     safety: ['NotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTester_Case126
    : public Unary2RegisterImmedShiftedOpTesterCase126 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case126()
    : Unary2RegisterImmedShiftedOpTesterCase126(
      state_.Unary2RegisterImmedShiftedOp_Asr_Rule_14_A1_P40_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1101x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'rule': 'Mov_Rule_96_A1_P194',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1101x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: Mov_Rule_96_A1_P194,
//     safety: ['NotRdIsPcAndS']}
class Unary1RegisterImmediateOpTester_Case127
    : public Unary1RegisterImmediateOpTesterCase127 {
 public:
  Unary1RegisterImmediateOpTester_Case127()
    : Unary1RegisterImmediateOpTesterCase127(
      state_.Unary1RegisterImmediateOp_Mov_Rule_96_A1_P194_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1110x & inst(7:5)=x00 & inst(3:0)=~1111
//    = Binary2RegisterBitRangeMsbGeLsb {'constraints': ,
//     'rule': 'Bfi_Rule_18_A1_P48',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=~1111
//    = Binary2RegisterBitRangeMsbGeLsb {constraints: ,
//     rule: Bfi_Rule_18_A1_P48,
//     safety: ['RegsNotPc']}
class Binary2RegisterBitRangeMsbGeLsbTester_Case128
    : public Binary2RegisterBitRangeMsbGeLsbTesterCase128 {
 public:
  Binary2RegisterBitRangeMsbGeLsbTester_Case128()
    : Binary2RegisterBitRangeMsbGeLsbTesterCase128(
      state_.Binary2RegisterBitRangeMsbGeLsb_Bfi_Rule_18_A1_P48_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1110x & inst(7:5)=x00 & inst(3:0)=1111
//    = Unary1RegisterBitRangeMsbGeLsb {'constraints': ,
//     'rule': 'Bfc_17_A1_P46',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=1111
//    = Unary1RegisterBitRangeMsbGeLsb {constraints: ,
//     rule: Bfc_17_A1_P46,
//     safety: ['RegsNotPc']}
class Unary1RegisterBitRangeMsbGeLsbTester_Case129
    : public Unary1RegisterBitRangeMsbGeLsbTesterCase129 {
 public:
  Unary1RegisterBitRangeMsbGeLsbTester_Case129()
    : Unary1RegisterBitRangeMsbGeLsbTesterCase129(
      state_.Unary1RegisterBitRangeMsbGeLsb_Bfc_17_A1_P46_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1110x
//    = Binary3RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Bic_Rule_20_A1_P52',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1110x
//    = Binary3RegisterImmedShiftedOp {constraints: ,
//     rule: Bic_Rule_20_A1_P52,
//     safety: ['NotRdIsPcAndS']}
class Binary3RegisterImmedShiftedOpTester_Case130
    : public Binary3RegisterImmedShiftedOpTesterCase130 {
 public:
  Binary3RegisterImmedShiftedOpTester_Case130()
    : Binary3RegisterImmedShiftedOpTesterCase130(
      state_.Binary3RegisterImmedShiftedOp_Bic_Rule_20_A1_P52_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1110x
//    = MaskedBinary2RegisterImmediateOp {'constraints': ,
//     'rule': 'Bic_Rule_19_A1_P50',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1110x
//    = MaskedBinary2RegisterImmediateOp {constraints: ,
//     rule: Bic_Rule_19_A1_P50,
//     safety: ['NotRdIsPcAndS']}
class MaskedBinary2RegisterImmediateOpTester_Case131
    : public Binary2RegisterImmediateOpTesterCase131 {
 public:
  MaskedBinary2RegisterImmediateOpTester_Case131()
    : Binary2RegisterImmediateOpTesterCase131(
      state_.MaskedBinary2RegisterImmediateOp_Bic_Rule_19_A1_P50_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1110x
//    = Binary4RegisterShiftedOp {'constraints': ,
//     'rule': 'Bic_Rule_21_A1_P54',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1110x
//    = Binary4RegisterShiftedOp {constraints: ,
//     rule: Bic_Rule_21_A1_P54,
//     safety: ['RegsNotPc']}
class Binary4RegisterShiftedOpTester_Case132
    : public Binary4RegisterShiftedOpTesterCase132 {
 public:
  Binary4RegisterShiftedOpTester_Case132()
    : Binary4RegisterShiftedOpTesterCase132(
      state_.Binary4RegisterShiftedOp_Bic_Rule_21_A1_P54_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1111x & inst(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract {'constraints': ,
//     'rule': 'Ubfx_Rule_236_A1_P466',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1111x & op2(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract {constraints: ,
//     rule: Ubfx_Rule_236_A1_P466,
//     safety: ['RegsNotPc']}
class Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case133
    : public Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase133 {
 public:
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case133()
    : Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTesterCase133(
      state_.Binary2RegisterBitRangeNotRnIsPcBitfieldExtract_Ubfx_Rule_236_A1_P466_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {'constraints': ,
//     'rule': 'Mvn_Rule_107_A1_P216',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp {constraints: ,
//     rule: Mvn_Rule_107_A1_P216,
//     safety: ['NotRdIsPcAndS']}
class Unary2RegisterImmedShiftedOpTester_Case134
    : public Unary2RegisterImmedShiftedOpTesterCase134 {
 public:
  Unary2RegisterImmedShiftedOpTester_Case134()
    : Unary2RegisterImmedShiftedOpTesterCase134(
      state_.Unary2RegisterImmedShiftedOp_Mvn_Rule_107_A1_P216_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'rule': 'Mvn_Rule_106_A1_P214',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: Mvn_Rule_106_A1_P214,
//     safety: ['NotRdIsPcAndS']}
class Unary1RegisterImmediateOpTester_Case135
    : public Unary1RegisterImmediateOpTesterCase135 {
 public:
  Unary1RegisterImmediateOpTester_Case135()
    : Unary1RegisterImmediateOpTesterCase135(
      state_.Unary1RegisterImmediateOp_Mvn_Rule_106_A1_P214_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp {'constraints': ,
//     'rule': 'Mvn_Rule_108_A1_P218',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp {constraints: ,
//     rule: Mvn_Rule_108_A1_P218,
//     safety: ['RegsNotPc']}
class Unary3RegisterShiftedOpTester_Case136
    : public Unary3RegisterShiftedOpTesterCase136 {
 public:
  Unary3RegisterShiftedOpTester_Case136()
    : Unary3RegisterShiftedOpTesterCase136(
      state_.Unary3RegisterShiftedOp_Mvn_Rule_108_A1_P218_instance_)
  {}
};

// Neutral case:
// inst(26:20)=11xxx11 & inst(7:4)=xxx0
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=11xxx11 & op2(7:4)=xxx0
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case137
    : public UnsafeUncondDecoderTesterCase137 {
 public:
  UnpredictableUncondDecoderTester_Case137()
    : UnsafeUncondDecoderTesterCase137(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(27:20)=110xxxx0 & inst(27:20)=~11000x00
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Stc2_Rule_188_A2_P372'}
//
// Representative case:
// op1(27:20)=110xxxx0 & op1_repeated(27:20)=~11000x00
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Stc2_Rule_188_A2_P372}
class ForbiddenUncondDecoderTester_Case138
    : public UnsafeUncondDecoderTesterCase138 {
 public:
  ForbiddenUncondDecoderTester_Case138()
    : UnsafeUncondDecoderTesterCase138(
      state_.ForbiddenUncondDecoder_Stc2_Rule_188_A2_P372_instance_)
  {}
};

// Neutral case:
// inst(27:20)=110xxxx1 & inst(19:16)=~1111 & inst(27:20)=~11000x01
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Ldc2_Rule_51_A2_P106'}
//
// Representative case:
// op1(27:20)=110xxxx1 & Rn(19:16)=~1111 & op1_repeated(27:20)=~11000x01
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Ldc2_Rule_51_A2_P106}
class ForbiddenUncondDecoderTester_Case139
    : public UnsafeUncondDecoderTesterCase139 {
 public:
  ForbiddenUncondDecoderTester_Case139()
    : UnsafeUncondDecoderTesterCase139(
      state_.ForbiddenUncondDecoder_Ldc2_Rule_51_A2_P106_instance_)
  {}
};

// Neutral case:
// inst(27:20)=110xxxx1 & inst(19:16)=1111 & inst(27:20)=~11000x01
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Ldc2_Rule_52_A2_P108'}
//
// Representative case:
// op1(27:20)=110xxxx1 & Rn(19:16)=1111 & op1_repeated(27:20)=~11000x01
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Ldc2_Rule_52_A2_P108}
class ForbiddenUncondDecoderTester_Case140
    : public UnsafeUncondDecoderTesterCase140 {
 public:
  ForbiddenUncondDecoderTester_Case140()
    : UnsafeUncondDecoderTesterCase140(
      state_.ForbiddenUncondDecoder_Ldc2_Rule_52_A2_P108_instance_)
  {}
};

// Neutral case:
// inst(27:20)=1110xxxx & inst(4)=0
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Cdp2_Rule_28_A2_P68'}
//
// Representative case:
// op1(27:20)=1110xxxx & op(4)=0
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Cdp2_Rule_28_A2_P68}
class ForbiddenUncondDecoderTester_Case141
    : public UnsafeUncondDecoderTesterCase141 {
 public:
  ForbiddenUncondDecoderTester_Case141()
    : UnsafeUncondDecoderTesterCase141(
      state_.ForbiddenUncondDecoder_Cdp2_Rule_28_A2_P68_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx00 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = Unary1RegisterUse {'constraints': ,
//     'rule': 'Msr_Rule_104_A1_P210'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = Unary1RegisterUse {constraints: ,
//     rule: Msr_Rule_104_A1_P210}
class Unary1RegisterUseTester_Case142
    : public Unary1RegisterUseTesterCase142 {
 public:
  Unary1RegisterUseTester_Case142()
    : Unary1RegisterUseTesterCase142(
      state_.Unary1RegisterUse_Msr_Rule_104_A1_P210_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Msr_Rule_B6_1_7_P14'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Msr_Rule_B6_1_7_P14}
class ForbiddenCondDecoderTester_Case143
    : public UnsafeCondDecoderTesterCase143 {
 public:
  ForbiddenCondDecoderTester_Case143()
    : UnsafeCondDecoderTesterCase143(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_7_P14_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Msr_Rule_B6_1_7_P14'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Msr_Rule_B6_1_7_P14}
class ForbiddenCondDecoderTester_Case144
    : public UnsafeCondDecoderTesterCase144 {
 public:
  ForbiddenCondDecoderTester_Case144()
    : UnsafeCondDecoderTesterCase144(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_7_P14_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Msr_Rule_B6_1_7_P14'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Msr_Rule_B6_1_7_P14}
class ForbiddenCondDecoderTester_Case145
    : public UnsafeCondDecoderTesterCase145 {
 public:
  ForbiddenCondDecoderTester_Case145()
    : UnsafeCondDecoderTesterCase145(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_7_P14_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = Unary1RegisterSet {'constraints': ,
//     'rule': 'Mrs_Rule_102_A1_P206_Or_B6_10'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = Unary1RegisterSet {constraints: ,
//     rule: Mrs_Rule_102_A1_P206_Or_B6_10}
class Unary1RegisterSetTester_Case146
    : public Unary1RegisterSetTesterCase146 {
 public:
  Unary1RegisterSetTester_Case146()
    : Unary1RegisterSetTesterCase146(
      state_.Unary1RegisterSet_Mrs_Rule_102_A1_P206_Or_B6_10_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Msr_Rule_Banked_register_A1_B9_1990'}
//
// Representative case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Msr_Rule_Banked_register_A1_B9_1990}
class ForbiddenCondDecoderTester_Case147
    : public UnsafeCondDecoderTesterCase147 {
 public:
  ForbiddenCondDecoderTester_Case147()
    : UnsafeCondDecoderTesterCase147(
      state_.ForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1990_instance_)
  {}
};

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x1 & inst(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Msr_Rule_Banked_register_A1_B9_1992'}
//
// Representative case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Msr_Rule_Banked_register_A1_B9_1992}
class ForbiddenCondDecoderTester_Case148
    : public UnsafeCondDecoderTesterCase148 {
 public:
  ForbiddenCondDecoderTester_Case148()
    : UnsafeCondDecoderTesterCase148(
      state_.ForbiddenCondDecoder_Msr_Rule_Banked_register_A1_B9_1992_instance_)
  {}
};

// Neutral case:
// inst(6:4)=001 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = BranchToRegister {'constraints': ,
//     'rule': 'Bx_Rule_25_A1_P62'}
//
// Representative case:
// op2(6:4)=001 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = BranchToRegister {constraints: ,
//     rule: Bx_Rule_25_A1_P62}
class BranchToRegisterTester_Case149
    : public BranchToRegisterTesterCase149 {
 public:
  BranchToRegisterTester_Case149()
    : BranchToRegisterTesterCase149(
      state_.BranchToRegister_Bx_Rule_25_A1_P62_instance_)
  {}
};

// Neutral case:
// inst(6:4)=001 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPc {'constraints': ,
//     'rule': 'Clz_Rule_31_A1_P72'}
//
// Representative case:
// op2(6:4)=001 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPc {constraints: ,
//     rule: Clz_Rule_31_A1_P72}
class Unary2RegisterOpNotRmIsPcTester_Case150
    : public Unary2RegisterOpNotRmIsPcTesterCase150 {
 public:
  Unary2RegisterOpNotRmIsPcTester_Case150()
    : Unary2RegisterOpNotRmIsPcTesterCase150(
      state_.Unary2RegisterOpNotRmIsPc_Clz_Rule_31_A1_P72_instance_)
  {}
};

// Neutral case:
// inst(6:4)=010 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Bxj_Rule_26_A1_P64'}
//
// Representative case:
// op2(6:4)=010 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Bxj_Rule_26_A1_P64}
class ForbiddenCondDecoderTester_Case151
    : public UnsafeCondDecoderTesterCase151 {
 public:
  ForbiddenCondDecoderTester_Case151()
    : UnsafeCondDecoderTesterCase151(
      state_.ForbiddenCondDecoder_Bxj_Rule_26_A1_P64_instance_)
  {}
};

// Neutral case:
// inst(6:4)=011 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = BranchToRegister {'constraints': ,
//     'rule': 'Blx_Rule_24_A1_P60',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op2(6:4)=011 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = BranchToRegister {constraints: ,
//     rule: Blx_Rule_24_A1_P60,
//     safety: ['RegsNotPc']}
class BranchToRegisterTester_Case152
    : public BranchToRegisterTesterCase152 {
 public:
  BranchToRegisterTester_Case152()
    : BranchToRegisterTesterCase152(
      state_.BranchToRegister_Blx_Rule_24_A1_P60_instance_)
  {}
};

// Neutral case:
// inst(6:4)=110 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Eret_Rule_A1'}
//
// Representative case:
// op2(6:4)=110 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Eret_Rule_A1}
class ForbiddenCondDecoderTester_Case153
    : public UnsafeCondDecoderTesterCase153 {
 public:
  ForbiddenCondDecoderTester_Case153()
    : UnsafeCondDecoderTesterCase153(
      state_.ForbiddenCondDecoder_Eret_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(6:4)=111 & inst(22:21)=01
//    = BreakPointAndConstantPoolHead {'constraints': ,
//     'rule': 'Bkpt_Rule_22_A1_P56'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=01
//    = BreakPointAndConstantPoolHead {constraints: ,
//     rule: Bkpt_Rule_22_A1_P56}
class BreakPointAndConstantPoolHeadTester_Case154
    : public Immediate16UseTesterCase154 {
 public:
  BreakPointAndConstantPoolHeadTester_Case154()
    : Immediate16UseTesterCase154(
      state_.BreakPointAndConstantPoolHead_Bkpt_Rule_22_A1_P56_instance_)
  {}
};

// Neutral case:
// inst(6:4)=111 & inst(22:21)=10
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Hvc_Rule_A1'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=10
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Hvc_Rule_A1}
class ForbiddenCondDecoderTester_Case155
    : public UnsafeCondDecoderTesterCase155 {
 public:
  ForbiddenCondDecoderTester_Case155()
    : UnsafeCondDecoderTesterCase155(
      state_.ForbiddenCondDecoder_Hvc_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(6:4)=111 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Smc_Rule_B6_1_9_P18'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Smc_Rule_B6_1_9_P18}
class ForbiddenCondDecoderTester_Case156
    : public UnsafeCondDecoderTesterCase156 {
 public:
  ForbiddenCondDecoderTester_Case156()
    : UnsafeCondDecoderTesterCase156(
      state_.ForbiddenCondDecoder_Smc_Rule_B6_1_9_P18_instance_)
  {}
};

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=000100
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Mcrr_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=000100
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Mcrr_Rule_A1}
class ForbiddenCondDecoderTester_Case157
    : public UnsafeCondDecoderTesterCase157 {
 public:
  ForbiddenCondDecoderTester_Case157()
    : UnsafeCondDecoderTesterCase157(
      state_.ForbiddenCondDecoder_Mcrr_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=000101
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Mrrc_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=000101
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Mrrc_Rule_A1}
class ForbiddenCondDecoderTester_Case158
    : public UnsafeCondDecoderTesterCase158 {
 public:
  ForbiddenCondDecoderTester_Case158()
    : UnsafeCondDecoderTesterCase158(
      state_.ForbiddenCondDecoder_Mrrc_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxx0 & inst(4)=1
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Mcr_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=10xxx0 & op(4)=1
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Mcr_Rule_A1}
class ForbiddenCondDecoderTester_Case159
    : public UnsafeCondDecoderTesterCase159 {
 public:
  ForbiddenCondDecoderTester_Case159()
    : UnsafeCondDecoderTesterCase159(
      state_.ForbiddenCondDecoder_Mcr_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxx1 & inst(4)=1
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Mrc_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=10xxx1 & op(4)=1
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Mrc_Rule_A1}
class ForbiddenCondDecoderTester_Case160
    : public UnsafeCondDecoderTesterCase160 {
 public:
  ForbiddenCondDecoderTester_Case160()
    : UnsafeCondDecoderTesterCase160(
      state_.ForbiddenCondDecoder_Mrc_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx0 & inst(25:20)=~000x00
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Stc_Rule_A2'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx0 & op1_repeated(25:20)=~000x00
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Stc_Rule_A2}
class ForbiddenCondDecoderTester_Case161
    : public UnsafeCondDecoderTesterCase161 {
 public:
  ForbiddenCondDecoderTester_Case161()
    : UnsafeCondDecoderTesterCase161(
      state_.ForbiddenCondDecoder_Stc_Rule_A2_instance_)
  {}
};

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx1 & inst(19:16)=~1111 & inst(25:20)=~000x01
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Ldc_immediate_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=~1111 & op1_repeated(25:20)=~000x01
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Ldc_immediate_Rule_A1}
class ForbiddenCondDecoderTester_Case162
    : public UnsafeCondDecoderTesterCase162 {
 public:
  ForbiddenCondDecoderTester_Case162()
    : UnsafeCondDecoderTesterCase162(
      state_.ForbiddenCondDecoder_Ldc_immediate_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx1 & inst(19:16)=1111 & inst(25:20)=~000x01
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Ldc_literal_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=1111 & op1_repeated(25:20)=~000x01
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Ldc_literal_Rule_A1}
class ForbiddenCondDecoderTester_Case163
    : public UnsafeCondDecoderTesterCase163 {
 public:
  ForbiddenCondDecoderTester_Case163()
    : UnsafeCondDecoderTesterCase163(
      state_.ForbiddenCondDecoder_Ldc_literal_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxxx & inst(4)=0
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Cdp_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=10xxxx & op(4)=0
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Cdp_Rule_A1}
class ForbiddenCondDecoderTester_Case164
    : public UnsafeCondDecoderTesterCase164 {
 public:
  ForbiddenCondDecoderTester_Case164()
    : UnsafeCondDecoderTesterCase164(
      state_.ForbiddenCondDecoder_Cdp_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(19:16)=001x & inst(7:6)=x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vcvtb_Vcvtt_Rule_300_A1_P588'}
//
// Representative case:
// opc2(19:16)=001x & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
//    = CondVfpOp {constraints: ,
//     rule: Vcvtb_Vcvtt_Rule_300_A1_P588}
class CondVfpOpTester_Case165
    : public CondVfpOpTesterCase165 {
 public:
  CondVfpOpTester_Case165()
    : CondVfpOpTesterCase165(
      state_.CondVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588_instance_)
  {}
};

// Neutral case:
// inst(19:16)=101x & inst(7:6)=x1
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vcvt_Rule_297_A1_P582'}
//
// Representative case:
// opc2(19:16)=101x & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: Vcvt_Rule_297_A1_P582}
class CondVfpOpTester_Case166
    : public CondVfpOpTesterCase166 {
 public:
  CondVfpOpTester_Case166()
    : CondVfpOpTesterCase166(
      state_.CondVfpOp_Vcvt_Rule_297_A1_P582_instance_)
  {}
};

// Neutral case:
// inst(19:16)=110x & inst(7:6)=x1
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vcvt_Vcvtr_Rule_295_A1_P578'}
//
// Representative case:
// opc2(19:16)=110x & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: Vcvt_Vcvtr_Rule_295_A1_P578}
class CondVfpOpTester_Case167
    : public CondVfpOpTesterCase167 {
 public:
  CondVfpOpTester_Case167()
    : CondVfpOpTesterCase167(
      state_.CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578_instance_)
  {}
};

// Neutral case:
// inst(19:16)=111x & inst(7:6)=x1
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vcvt_Rule_297_A1_P582'}
//
// Representative case:
// opc2(19:16)=111x & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: Vcvt_Rule_297_A1_P582}
class CondVfpOpTester_Case168
    : public CondVfpOpTesterCase168 {
 public:
  CondVfpOpTester_Case168()
    : CondVfpOpTesterCase168(
      state_.CondVfpOp_Vcvt_Rule_297_A1_P582_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterImmedShiftedOpRegsNotPc {'constraints': ,
//     'rule': 'Sxtab16_Rule_221_A1_P436',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     rule: Sxtab16_Rule_221_A1_P436,
//     safety: ['RegsNotPc']}
class Binary3RegisterImmedShiftedOpRegsNotPcTester_Case169
    : public Binary3RegisterImmedShiftedOpTesterCase169 {
 public:
  Binary3RegisterImmedShiftedOpRegsNotPcTester_Case169()
    : Binary3RegisterImmedShiftedOpTesterCase169(
      state_.Binary3RegisterImmedShiftedOpRegsNotPc_Sxtab16_Rule_221_A1_P436_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterImmedShiftedOpRegsNotPc {'constraints': ,
//     'rule': 'Sxtb16_Rule_224_A1_P442',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     rule: Sxtb16_Rule_224_A1_P442,
//     safety: ['RegsNotPc']}
class Unary2RegisterImmedShiftedOpRegsNotPcTester_Case170
    : public Unary2RegisterImmedShiftedOpRegsNotPcTesterCase170 {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTester_Case170()
    : Unary2RegisterImmedShiftedOpRegsNotPcTesterCase170(
      state_.Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb16_Rule_224_A1_P442_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Sel_Rule_156_A1_P312',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Sel_Rule_156_A1_P312,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case171
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase171 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case171()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase171(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sel_Rule_156_A1_P312_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=00x & inst(15:12)=~1111
//    = Binary4RegisterDualOp {'constraints': ,
//     'rule': 'Smlad_Rule_167_P332',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     rule: Smlad_Rule_167_P332,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case172
    : public Binary4RegisterDualOpTesterCase172 {
 public:
  Binary4RegisterDualOpTester_Case172()
    : Binary4RegisterDualOpTesterCase172(
      state_.Binary4RegisterDualOp_Smlad_Rule_167_P332_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=00x & inst(15:12)=1111
//    = Binary3RegisterOpAltA {'constraints': ,
//     'rule': 'Smuad_Rule_177_P352',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: Smuad_Rule_177_P352,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_Case173
    : public Binary3RegisterOpAltATesterCase173 {
 public:
  Binary3RegisterOpAltATester_Case173()
    : Binary3RegisterOpAltATesterCase173(
      state_.Binary3RegisterOpAltA_Smuad_Rule_177_P352_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=01x & inst(15:12)=~1111
//    = Binary4RegisterDualOp {'constraints': ,
//     'rule': 'Smlsd_Rule_172_P342',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     rule: Smlsd_Rule_172_P342,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case174
    : public Binary4RegisterDualOpTesterCase174 {
 public:
  Binary4RegisterDualOpTester_Case174()
    : Binary4RegisterDualOpTesterCase174(
      state_.Binary4RegisterDualOp_Smlsd_Rule_172_P342_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=01x & inst(15:12)=1111
//    = Binary3RegisterOpAltA {'constraints': ,
//     'rule': 'Smusd_Rule_181_P360',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: Smusd_Rule_181_P360,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_Case175
    : public Binary3RegisterOpAltATesterCase175 {
 public:
  Binary3RegisterOpAltATester_Case175()
    : Binary3RegisterOpAltATesterCase175(
      state_.Binary3RegisterOpAltA_Smusd_Rule_181_P360_instance_)
  {}
};

// Neutral case:
// inst(22:20)=000 & inst(7:5)=xx0
//    = Binary3RegisterImmedShiftedOpRegsNotPc {'constraints': ,
//     'rule': 'Pkh_Rule_116_A1_P234',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=xx0
//    = Binary3RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     rule: Pkh_Rule_116_A1_P234,
//     safety: ['RegsNotPc']}
class Binary3RegisterImmedShiftedOpRegsNotPcTester_Case176
    : public Binary3RegisterImmedShiftedOpTesterCase176 {
 public:
  Binary3RegisterImmedShiftedOpRegsNotPcTester_Case176()
    : Binary3RegisterImmedShiftedOpTesterCase176(
      state_.Binary3RegisterImmedShiftedOpRegsNotPc_Pkh_Rule_116_A1_P234_instance_)
  {}
};

// Neutral case:
// inst(22:20)=001 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {'constraints': ,
//     'rule': 'Sdiv_Rule_A1'}
//
// Representative case:
// op1(22:20)=001 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: Sdiv_Rule_A1}
class Binary3RegisterOpAltATester_Case177
    : public Binary3RegisterOpAltATesterCase177 {
 public:
  Binary3RegisterOpAltATester_Case177()
    : Binary3RegisterOpAltATesterCase177(
      state_.Binary3RegisterOpAltA_Sdiv_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(22:20)=010 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Unary2RegisterSatImmedShiftedOp {'constraints': ,
//     'rule': 'Ssat16_Rule_184_A1_P364',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=010 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     rule: Ssat16_Rule_184_A1_P364,
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTester_Case178
    : public Unary2RegisterSatImmedShiftedOpTesterCase178 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_Case178()
    : Unary2RegisterSatImmedShiftedOpTesterCase178(
      state_.Unary2RegisterSatImmedShiftedOp_Ssat16_Rule_184_A1_P364_instance_)
  {}
};

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Sxtab_Rule_220_A1_P434',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Sxtab_Rule_220_A1_P434,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case179
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase179 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case179()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase179(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sxtab_Rule_220_A1_P434_instance_)
  {}
};

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterImmedShiftedOpRegsNotPc {'constraints': ,
//     'rule': 'Sxtb_Rule_223_A1_P440',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterImmedShiftedOpRegsNotPc {constraints: ,
//     rule: Sxtb_Rule_223_A1_P440,
//     safety: ['RegsNotPc']}
class Unary2RegisterImmedShiftedOpRegsNotPcTester_Case180
    : public Unary2RegisterImmedShiftedOpRegsNotPcTesterCase180 {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPcTester_Case180()
    : Unary2RegisterImmedShiftedOpRegsNotPcTesterCase180(
      state_.Unary2RegisterImmedShiftedOpRegsNotPc_Sxtb_Rule_223_A1_P440_instance_)
  {}
};

// Neutral case:
// inst(22:20)=011 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {'constraints': ,
//     'rule': 'Udiv_Rule_A1'}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: Udiv_Rule_A1}
class Binary3RegisterOpAltATester_Case181
    : public Binary3RegisterOpAltATesterCase181 {
 public:
  Binary3RegisterOpAltATester_Case181()
    : Binary3RegisterOpAltATesterCase181(
      state_.Binary3RegisterOpAltA_Udiv_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(22:20)=011 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'rule': 'Rev_Rule_135_A1_P272',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: Rev_Rule_135_A1_P272,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case182
    : public Unary2RegisterOpNotRmIsPcTesterCase182 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case182()
    : Unary2RegisterOpNotRmIsPcTesterCase182(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev_Rule_135_A1_P272_instance_)
  {}
};

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Sxtah_Rule_222_A1_P438',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Sxtah_Rule_222_A1_P438,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case183
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase183 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case183()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase183(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sxtah_Rule_222_A1_P438_instance_)
  {}
};

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'rule': 'Sxth_Rule_225_A1_P444',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: Sxth_Rule_225_A1_P444,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case184
    : public Unary2RegisterOpNotRmIsPcTesterCase184 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case184()
    : Unary2RegisterOpNotRmIsPcTesterCase184(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Sxth_Rule_225_A1_P444_instance_)
  {}
};

// Neutral case:
// inst(22:20)=011 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'rule': 'Rev16_Rule_136_A1_P274',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: Rev16_Rule_136_A1_P274,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case185
    : public Unary2RegisterOpNotRmIsPcTesterCase185 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case185()
    : Unary2RegisterOpNotRmIsPcTesterCase185(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Rev16_Rule_136_A1_P274_instance_)
  {}
};

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uxtab16_Rule_262_A1_P516',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uxtab16_Rule_262_A1_P516,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case186
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase186 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case186()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase186(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uxtab16_Rule_262_A1_P516_instance_)
  {}
};

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'rule': 'Uxtb16_Rule_264_A1_P522',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: Uxtb16_Rule_264_A1_P522,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case187
    : public Unary2RegisterOpNotRmIsPcTesterCase187 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case187()
    : Unary2RegisterOpNotRmIsPcTesterCase187(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb16_Rule_264_A1_P522_instance_)
  {}
};

// Neutral case:
// inst(22:20)=100 & inst(7:5)=00x
//    = Binary4RegisterDualResult {'constraints': ,
//     'rule': 'Smlald_Rule_170_P336',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=00x
//    = Binary4RegisterDualResult {constraints: ,
//     rule: Smlald_Rule_170_P336,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_Case188
    : public Binary4RegisterDualResultTesterCase188 {
 public:
  Binary4RegisterDualResultTester_Case188()
    : Binary4RegisterDualResultTesterCase188(
      state_.Binary4RegisterDualResult_Smlald_Rule_170_P336_instance_)
  {}
};

// Neutral case:
// inst(22:20)=100 & inst(7:5)=01x
//    = Binary4RegisterDualResult {'constraints': ,
//     'rule': 'Smlsld_Rule_173_P344',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=01x
//    = Binary4RegisterDualResult {constraints: ,
//     rule: Smlsld_Rule_173_P344,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualResultTester_Case189
    : public Binary4RegisterDualResultTesterCase189 {
 public:
  Binary4RegisterDualResultTester_Case189()
    : Binary4RegisterDualResultTesterCase189(
      state_.Binary4RegisterDualResult_Smlsld_Rule_173_P344_instance_)
  {}
};

// Neutral case:
// inst(22:20)=101 & inst(7:5)=00x & inst(15:12)=~1111
//    = Binary4RegisterDualOp {'constraints': ,
//     'rule': 'Smmla_Rule_174_P346',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=~1111
//    = Binary4RegisterDualOp {constraints: ,
//     rule: Smmla_Rule_174_P346,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case190
    : public Binary4RegisterDualOpTesterCase190 {
 public:
  Binary4RegisterDualOpTester_Case190()
    : Binary4RegisterDualOpTesterCase190(
      state_.Binary4RegisterDualOp_Smmla_Rule_174_P346_instance_)
  {}
};

// Neutral case:
// inst(22:20)=101 & inst(7:5)=00x & inst(15:12)=1111
//    = Binary3RegisterOpAltA {'constraints': ,
//     'rule': 'Smmul_Rule_176_P350',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=1111
//    = Binary3RegisterOpAltA {constraints: ,
//     rule: Smmul_Rule_176_P350,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltATester_Case191
    : public Binary3RegisterOpAltATesterCase191 {
 public:
  Binary3RegisterOpAltATester_Case191()
    : Binary3RegisterOpAltATesterCase191(
      state_.Binary3RegisterOpAltA_Smmul_Rule_176_P350_instance_)
  {}
};

// Neutral case:
// inst(22:20)=101 & inst(7:5)=11x
//    = Binary4RegisterDualOp {'constraints': ,
//     'rule': 'Smmls_Rule_175_P348',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=101 & op2(7:5)=11x
//    = Binary4RegisterDualOp {constraints: ,
//     rule: Smmls_Rule_175_P348,
//     safety: ['RegsNotPc']}
class Binary4RegisterDualOpTester_Case192
    : public Binary4RegisterDualOpTesterCase192 {
 public:
  Binary4RegisterDualOpTester_Case192()
    : Binary4RegisterDualOpTesterCase192(
      state_.Binary4RegisterDualOp_Smmls_Rule_175_P348_instance_)
  {}
};

// Neutral case:
// inst(22:20)=110 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Unary2RegisterSatImmedShiftedOp {'constraints': ,
//     'rule': 'Usat16_Rule_256_A1_P506',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=110 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     rule: Usat16_Rule_256_A1_P506,
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTester_Case193
    : public Unary2RegisterSatImmedShiftedOpTesterCase193 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_Case193()
    : Unary2RegisterSatImmedShiftedOpTesterCase193(
      state_.Unary2RegisterSatImmedShiftedOp_Usat16_Rule_256_A1_P506_instance_)
  {}
};

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uxtab_Rule_260_A1_P514',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uxtab_Rule_260_A1_P514,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case194
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase194 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case194()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase194(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uxtab_Rule_260_A1_P514_instance_)
  {}
};

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'rule': 'Uxtb_Rule_263_A1_P520',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: Uxtb_Rule_263_A1_P520,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case195
    : public Unary2RegisterOpNotRmIsPcTesterCase195 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case195()
    : Unary2RegisterOpNotRmIsPcTesterCase195(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxtb_Rule_263_A1_P520_instance_)
  {}
};

// Neutral case:
// inst(22:20)=111 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'rule': 'Rbit_Rule_134_A1_P270',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: Rbit_Rule_134_A1_P270,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case196
    : public Unary2RegisterOpNotRmIsPcTesterCase196 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case196()
    : Unary2RegisterOpNotRmIsPcTesterCase196(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Rbit_Rule_134_A1_P270_instance_)
  {}
};

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Uxtah_Rule_262_A1_P518',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Uxtah_Rule_262_A1_P518,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case197
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase197 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case197()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase197(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uxtah_Rule_262_A1_P518_instance_)
  {}
};

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'rule': 'Uxth_Rule_265_A1_P524',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: Uxth_Rule_265_A1_P524,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case198
    : public Unary2RegisterOpNotRmIsPcTesterCase198 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case198()
    : Unary2RegisterOpNotRmIsPcTesterCase198(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Uxth_Rule_265_A1_P524_instance_)
  {}
};

// Neutral case:
// inst(22:20)=111 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {'constraints': ,
//     'rule': 'Revsh_Rule_137_A1_P276',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates {constraints: ,
//     rule: Revsh_Rule_137_A1_P276,
//     safety: ['RegsNotPc']}
class Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case199
    : public Unary2RegisterOpNotRmIsPcTesterCase199 {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case199()
    : Unary2RegisterOpNotRmIsPcTesterCase199(
      state_.Unary2RegisterOpNotRmIsPcNoCondUpdates_Revsh_Rule_137_A1_P276_instance_)
  {}
};

// Neutral case:
// inst(23:20)=0x00 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Deprecated {'constraints': ,
//     'rule': 'Swp_Swpb_Rule_A1'}
//
// Representative case:
// op(23:20)=0x00 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Deprecated {constraints: ,
//     rule: Swp_Swpb_Rule_A1}
class DeprecatedTester_Case200
    : public UnsafeCondDecoderTesterCase200 {
 public:
  DeprecatedTester_Case200()
    : UnsafeCondDecoderTesterCase200(
      state_.Deprecated_Swp_Swpb_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(23:20)=0x00
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vmla_vmls_Rule_423_A2_P636'}
//
// Representative case:
// opc1(23:20)=0x00
//    = CondVfpOp {constraints: ,
//     rule: Vmla_vmls_Rule_423_A2_P636}
class CondVfpOpTester_Case201
    : public CondVfpOpTesterCase201 {
 public:
  CondVfpOpTester_Case201()
    : CondVfpOpTesterCase201(
      state_.CondVfpOp_Vmla_vmls_Rule_423_A2_P636_instance_)
  {}
};

// Neutral case:
// inst(23:20)=0x01
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vnmla_vnmls_Rule_343_A1_P674'}
//
// Representative case:
// opc1(23:20)=0x01
//    = CondVfpOp {constraints: ,
//     rule: Vnmla_vnmls_Rule_343_A1_P674}
class CondVfpOpTester_Case202
    : public CondVfpOpTesterCase202 {
 public:
  CondVfpOpTester_Case202()
    : CondVfpOpTesterCase202(
      state_.CondVfpOp_Vnmla_vnmls_Rule_343_A1_P674_instance_)
  {}
};

// Neutral case:
// inst(23:20)=0x10 & inst(7:6)=x0
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vmul_Rule_338_A2_P664'}
//
// Representative case:
// opc1(23:20)=0x10 & opc3(7:6)=x0
//    = CondVfpOp {constraints: ,
//     rule: Vmul_Rule_338_A2_P664}
class CondVfpOpTester_Case203
    : public CondVfpOpTesterCase203 {
 public:
  CondVfpOpTester_Case203()
    : CondVfpOpTesterCase203(
      state_.CondVfpOp_Vmul_Rule_338_A2_P664_instance_)
  {}
};

// Neutral case:
// inst(23:20)=0x10 & inst(7:6)=x1
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vnmul_Rule_343_A2_P674'}
//
// Representative case:
// opc1(23:20)=0x10 & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: Vnmul_Rule_343_A2_P674}
class CondVfpOpTester_Case204
    : public CondVfpOpTesterCase204 {
 public:
  CondVfpOpTester_Case204()
    : CondVfpOpTesterCase204(
      state_.CondVfpOp_Vnmul_Rule_343_A2_P674_instance_)
  {}
};

// Neutral case:
// inst(23:20)=0x11 & inst(7:6)=x0
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vadd_Rule_271_A2_P536'}
//
// Representative case:
// opc1(23:20)=0x11 & opc3(7:6)=x0
//    = CondVfpOp {constraints: ,
//     rule: Vadd_Rule_271_A2_P536}
class CondVfpOpTester_Case205
    : public CondVfpOpTesterCase205 {
 public:
  CondVfpOpTester_Case205()
    : CondVfpOpTesterCase205(
      state_.CondVfpOp_Vadd_Rule_271_A2_P536_instance_)
  {}
};

// Neutral case:
// inst(23:20)=0x11 & inst(7:6)=x1
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vsub_Rule_402_A2_P790'}
//
// Representative case:
// opc1(23:20)=0x11 & opc3(7:6)=x1
//    = CondVfpOp {constraints: ,
//     rule: Vsub_Rule_402_A2_P790}
class CondVfpOpTester_Case206
    : public CondVfpOpTesterCase206 {
 public:
  CondVfpOpTester_Case206()
    : CondVfpOpTesterCase206(
      state_.CondVfpOp_Vsub_Rule_402_A2_P790_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1x00 & inst(7:6)=x0
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vdiv_Rule_301_A1_P590'}
//
// Representative case:
// opc1(23:20)=1x00 & opc3(7:6)=x0
//    = CondVfpOp {constraints: ,
//     rule: Vdiv_Rule_301_A1_P590}
class CondVfpOpTester_Case207
    : public CondVfpOpTesterCase207 {
 public:
  CondVfpOpTester_Case207()
    : CondVfpOpTesterCase207(
      state_.CondVfpOp_Vdiv_Rule_301_A1_P590_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1x01
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vfnma_vfnms_Rule_A1'}
//
// Representative case:
// opc1(23:20)=1x01
//    = CondVfpOp {constraints: ,
//     rule: Vfnma_vfnms_Rule_A1}
class CondVfpOpTester_Case208
    : public CondVfpOpTesterCase208 {
 public:
  CondVfpOpTester_Case208()
    : CondVfpOpTesterCase208(
      state_.CondVfpOp_Vfnma_vfnms_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(23:20)=1x10
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vfma_vfms_Rule_A1'}
//
// Representative case:
// opc1(23:20)=1x10
//    = CondVfpOp {constraints: ,
//     rule: Vfma_vfms_Rule_A1}
class CondVfpOpTester_Case209
    : public CondVfpOpTesterCase209 {
 public:
  CondVfpOpTester_Case209()
    : CondVfpOpTesterCase209(
      state_.CondVfpOp_Vfma_vfms_Rule_A1_instance_)
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
class Binary3RegisterOpAltATester_Case210
    : public Binary3RegisterOpAltATesterCase210 {
 public:
  Binary3RegisterOpAltATester_Case210()
    : Binary3RegisterOpAltATesterCase210(
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
class Binary4RegisterDualOpTester_Case211
    : public Binary4RegisterDualOpTesterCase211 {
 public:
  Binary4RegisterDualOpTester_Case211()
    : Binary4RegisterDualOpTesterCase211(
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
class Binary4RegisterDualResultTester_Case212
    : public Binary4RegisterDualResultTesterCase212 {
 public:
  Binary4RegisterDualResultTester_Case212()
    : Binary4RegisterDualResultTesterCase212(
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
class Binary4RegisterDualResultTester_Case213
    : public Binary4RegisterDualResultTesterCase213 {
 public:
  Binary4RegisterDualResultTester_Case213()
    : Binary4RegisterDualResultTesterCase213(
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
class Binary4RegisterDualResultTester_Case214
    : public Binary4RegisterDualResultTesterCase214 {
 public:
  Binary4RegisterDualResultTester_Case214()
    : Binary4RegisterDualResultTesterCase214(
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
class Binary4RegisterDualResultTester_Case215
    : public Binary4RegisterDualResultTesterCase215 {
 public:
  Binary4RegisterDualResultTester_Case215()
    : Binary4RegisterDualResultTesterCase215(
      state_.Binary4RegisterDualResult_Smlal_Rule_168_A1_P334_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1xx00
//    = StoreVectorRegister {'constraints': ,
//     'rule': 'Vstr_Rule_400_A1_A2_P786'}
//
// Representative case:
// opcode(24:20)=1xx00
//    = StoreVectorRegister {constraints: ,
//     rule: Vstr_Rule_400_A1_A2_P786}
class StoreVectorRegisterTester_Case216
    : public StoreVectorRegisterTesterCase216 {
 public:
  StoreVectorRegisterTester_Case216()
    : StoreVectorRegisterTesterCase216(
      state_.StoreVectorRegister_Vstr_Rule_400_A1_A2_P786_instance_)
  {}
};

// Neutral case:
// inst(24:20)=1xx01
//    = LoadVectorRegister {'constraints': ,
//     'rule': 'Vldr_Rule_320_A1_A2_P628'}
//
// Representative case:
// opcode(24:20)=1xx01
//    = LoadVectorRegister {constraints: ,
//     rule: Vldr_Rule_320_A1_A2_P628}
class LoadVectorRegisterTester_Case217
    : public LoadStoreVectorOpTesterCase217 {
 public:
  LoadVectorRegisterTester_Case217()
    : LoadStoreVectorOpTesterCase217(
      state_.LoadVectorRegister_Vldr_Rule_320_A1_A2_P628_instance_)
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
class ForbiddenCondDecoderTester_Case218
    : public UnsafeCondDecoderTesterCase218 {
 public:
  ForbiddenCondDecoderTester_Case218()
    : UnsafeCondDecoderTesterCase218(
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
class ForbiddenCondDecoderTester_Case219
    : public UnsafeCondDecoderTesterCase219 {
 public:
  ForbiddenCondDecoderTester_Case219()
    : UnsafeCondDecoderTesterCase219(
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
class ForbiddenCondDecoderTester_Case220
    : public UnsafeCondDecoderTesterCase220 {
 public:
  ForbiddenCondDecoderTester_Case220()
    : UnsafeCondDecoderTesterCase220(
      state_.ForbiddenCondDecoder_Ldm_Rule_2_B6_A1_P5_instance_)
  {}
};

// Neutral case:
// inst(27:20)=101xxxxx
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Blx_Rule_23_A2_P58'}
//
// Representative case:
// op1(27:20)=101xxxxx
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Blx_Rule_23_A2_P58}
class ForbiddenUncondDecoderTester_Case221
    : public UnsafeUncondDecoderTesterCase221 {
 public:
  ForbiddenUncondDecoderTester_Case221()
    : UnsafeUncondDecoderTesterCase221(
      state_.ForbiddenUncondDecoder_Blx_Rule_23_A2_P58_instance_)
  {}
};

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Store3RegisterOp {'constraints': ,
//     'rule': 'Strh_Rule_208_A1_P412'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Store3RegisterOp {constraints: ,
//     rule: Strh_Rule_208_A1_P412}
class Store3RegisterOpTester_Case222
    : public LoadStore3RegisterOpTesterCase222 {
 public:
  Store3RegisterOpTester_Case222()
    : LoadStore3RegisterOpTesterCase222(
      state_.Store3RegisterOp_Strh_Rule_208_A1_P412_instance_)
  {}
};

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp {'constraints': ,
//     'rule': 'Ldrh_Rule_76_A1_P156'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp {constraints: ,
//     rule: Ldrh_Rule_76_A1_P156}
class Load3RegisterOpTester_Case223
    : public LoadStore3RegisterOpTesterCase223 {
 public:
  Load3RegisterOpTester_Case223()
    : LoadStore3RegisterOpTesterCase223(
      state_.Load3RegisterOp_Ldrh_Rule_76_A1_P156_instance_)
  {}
};

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x0
//    = Store2RegisterImm8Op {'constraints': ,
//     'rule': 'Strh_Rule_207_A1_P410'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx1x0
//    = Store2RegisterImm8Op {constraints: ,
//     rule: Strh_Rule_207_A1_P410}
class Store2RegisterImm8OpTester_Case224
    : public LoadStore2RegisterImm8OpTesterCase224 {
 public:
  Store2RegisterImm8OpTester_Case224()
    : LoadStore2RegisterImm8OpTesterCase224(
      state_.Store2RegisterImm8Op_Strh_Rule_207_A1_P410_instance_)
  {}
};

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = Load2RegisterImm8Op {'constraints': ,
//     'rule': 'Ldrh_Rule_74_A1_P152'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op {constraints: ,
//     rule: Ldrh_Rule_74_A1_P152}
class Load2RegisterImm8OpTester_Case225
    : public LoadStore2RegisterImm8OpTesterCase225 {
 public:
  Load2RegisterImm8OpTester_Case225()
    : LoadStore2RegisterImm8OpTesterCase225(
      state_.Load2RegisterImm8Op_Ldrh_Rule_74_A1_P152_instance_)
  {}
};

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=1111
//    = Load2RegisterImm8Op {'constraints': ,
//     'rule': 'Ldrh_Rule_75_A1_P154'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op {constraints: ,
//     rule: Ldrh_Rule_75_A1_P154}
class Load2RegisterImm8OpTester_Case226
    : public LoadStore2RegisterImm8OpTesterCase226 {
 public:
  Load2RegisterImm8OpTester_Case226()
    : LoadStore2RegisterImm8OpTesterCase226(
      state_.Load2RegisterImm8Op_Ldrh_Rule_75_A1_P154_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterDoubleOp {'constraints': ,
//     'rule': 'Ldrd_Rule_68_A1_P140'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterDoubleOp {constraints: ,
//     rule: Ldrd_Rule_68_A1_P140}
class Load3RegisterDoubleOpTester_Case227
    : public LoadStore3RegisterDoubleOpTesterCase227 {
 public:
  Load3RegisterDoubleOpTester_Case227()
    : LoadStore3RegisterDoubleOpTesterCase227(
      state_.Load3RegisterDoubleOp_Ldrd_Rule_68_A1_P140_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp {'constraints': ,
//     'rule': 'Ldrsb_Rule_80_A1_P164'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp {constraints: ,
//     rule: Ldrsb_Rule_80_A1_P164}
class Load3RegisterOpTester_Case228
    : public LoadStore3RegisterOpTesterCase228 {
 public:
  Load3RegisterOpTester_Case228()
    : LoadStore3RegisterOpTesterCase228(
      state_.Load3RegisterOp_Ldrsb_Rule_80_A1_P164_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=~1111
//    = Load2RegisterImm8DoubleOp {'constraints': ,
//     'rule': 'Ldrd_Rule_66_A1_P136'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=~1111
//    = Load2RegisterImm8DoubleOp {constraints: ,
//     rule: Ldrd_Rule_66_A1_P136}
class Load2RegisterImm8DoubleOpTester_Case229
    : public LoadStore2RegisterImm8DoubleOpTesterCase229 {
 public:
  Load2RegisterImm8DoubleOpTester_Case229()
    : LoadStore2RegisterImm8DoubleOpTesterCase229(
      state_.Load2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=1111
//    = Load2RegisterImm8DoubleOp {'constraints': ,
//     'rule': 'Ldrd_Rule_67_A1_P138'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=1111
//    = Load2RegisterImm8DoubleOp {constraints: ,
//     rule: Ldrd_Rule_67_A1_P138}
class Load2RegisterImm8DoubleOpTester_Case230
    : public LoadStore2RegisterImm8DoubleOpTesterCase230 {
 public:
  Load2RegisterImm8DoubleOpTester_Case230()
    : LoadStore2RegisterImm8DoubleOpTesterCase230(
      state_.Load2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = Load2RegisterImm8Op {'constraints': ,
//     'rule': 'Ldrsb_Rule_78_A1_P160'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op {constraints: ,
//     rule: Ldrsb_Rule_78_A1_P160}
class Load2RegisterImm8OpTester_Case231
    : public LoadStore2RegisterImm8OpTesterCase231 {
 public:
  Load2RegisterImm8OpTester_Case231()
    : LoadStore2RegisterImm8OpTesterCase231(
      state_.Load2RegisterImm8Op_Ldrsb_Rule_78_A1_P160_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=1111
//    = Load2RegisterImm8Op {'constraints': ,
//     'rule': 'ldrsb_Rule_79_A1_162'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op {constraints: ,
//     rule: ldrsb_Rule_79_A1_162}
class Load2RegisterImm8OpTester_Case232
    : public LoadStore2RegisterImm8OpTesterCase232 {
 public:
  Load2RegisterImm8OpTester_Case232()
    : LoadStore2RegisterImm8OpTesterCase232(
      state_.Load2RegisterImm8Op_ldrsb_Rule_79_A1_162_instance_)
  {}
};

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Store3RegisterDoubleOp {'constraints': ,
//     'rule': 'Strd_Rule_201_A1_P398'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Store3RegisterDoubleOp {constraints: ,
//     rule: Strd_Rule_201_A1_P398}
class Store3RegisterDoubleOpTester_Case233
    : public LoadStore3RegisterDoubleOpTesterCase233 {
 public:
  Store3RegisterDoubleOpTester_Case233()
    : LoadStore3RegisterDoubleOpTesterCase233(
      state_.Store3RegisterDoubleOp_Strd_Rule_201_A1_P398_instance_)
  {}
};

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp {'constraints': ,
//     'rule': 'Ldrsh_Rule_84_A1_P172'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp {constraints: ,
//     rule: Ldrsh_Rule_84_A1_P172}
class Load3RegisterOpTester_Case234
    : public LoadStore3RegisterOpTesterCase234 {
 public:
  Load3RegisterOpTester_Case234()
    : LoadStore3RegisterOpTesterCase234(
      state_.Load3RegisterOp_Ldrsh_Rule_84_A1_P172_instance_)
  {}
};

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x0
//    = Store2RegisterImm8DoubleOp {'constraints': ,
//     'rule': 'Strd_Rule_200_A1_P396'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx1x0
//    = Store2RegisterImm8DoubleOp {constraints: ,
//     rule: Strd_Rule_200_A1_P396}
class Store2RegisterImm8DoubleOpTester_Case235
    : public LoadStore2RegisterImm8DoubleOpTesterCase235 {
 public:
  Store2RegisterImm8DoubleOpTester_Case235()
    : LoadStore2RegisterImm8DoubleOpTesterCase235(
      state_.Store2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396_instance_)
  {}
};

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = Load2RegisterImm8Op {'constraints': ,
//     'rule': 'Ldrsh_Rule_82_A1_P168'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op {constraints: ,
//     rule: Ldrsh_Rule_82_A1_P168}
class Load2RegisterImm8OpTester_Case236
    : public LoadStore2RegisterImm8OpTesterCase236 {
 public:
  Load2RegisterImm8OpTester_Case236()
    : LoadStore2RegisterImm8OpTesterCase236(
      state_.Load2RegisterImm8Op_Ldrsh_Rule_82_A1_P168_instance_)
  {}
};

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=1111
//    = Load2RegisterImm8Op {'constraints': ,
//     'rule': 'Ldrsh_Rule_83_A1_P170'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op {constraints: ,
//     rule: Ldrsh_Rule_83_A1_P170}
class Load2RegisterImm8OpTester_Case237
    : public LoadStore2RegisterImm8OpTesterCase237 {
 public:
  Load2RegisterImm8OpTester_Case237()
    : LoadStore2RegisterImm8OpTesterCase237(
      state_.Load2RegisterImm8Op_Ldrsh_Rule_83_A1_P170_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Sadd16_Rule_148_A1_P296',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Sadd16_Rule_148_A1_P296,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case238
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase238 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case238()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase238(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sadd16_Rule_148_A1_P296_instance_)
  {}
};

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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case239
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase238 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case239()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase238(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uadd16_Rule_233_A1_P460_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Sasx_Rule_150_A1_P300',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Sasx_Rule_150_A1_P300,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case240
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase239 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case240()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase239(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sasx_Rule_150_A1_P300_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case241
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase239 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case241()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase239(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uasx_Rule_235_A1_P464_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Ssax_Rule_185_A1_P366',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Ssax_Rule_185_A1_P366,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case242
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase240 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case242()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase240(
      state_.Binary3RegisterOpAltBNoCondUpdates_Ssax_Rule_185_A1_P366_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case243
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase240 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case243()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase240(
      state_.Binary3RegisterOpAltBNoCondUpdates_Usax_Rule_257_A1_P508_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Ssub16_Rule_186_A1_P368',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Ssub16_Rule_186_A1_P368,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case244
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase241 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case244()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase241(
      state_.Binary3RegisterOpAltBNoCondUpdates_Ssub16_Rule_186_A1_P368_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case245
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase241 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case245()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase241(
      state_.Binary3RegisterOpAltBNoCondUpdates_Usub16_Rule_258_A1_P510_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Sadd8_Rule_149_A1_P298',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Sadd8_Rule_149_A1_P298,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case246
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase242 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case246()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase242(
      state_.Binary3RegisterOpAltBNoCondUpdates_Sadd8_Rule_149_A1_P298_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case247
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase242 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case247()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase242(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uadd8_Rule_234_A1_P462_instance_)
  {}
};

// Neutral case:
// inst(21:20)=01 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Ssub8_Rule_187_A1_P370',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Ssub8_Rule_187_A1_P370,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case248
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase243 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case248()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase243(
      state_.Binary3RegisterOpAltBNoCondUpdates_Ssub8_Rule_187_A1_P370_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case249
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase243 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case249()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase243(
      state_.Binary3RegisterOpAltBNoCondUpdates_Usub8_Rule_259_A1_P512_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Qadd16_Rule_125_A1_P252',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Qadd16_Rule_125_A1_P252,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case250
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase244 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case250()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase244(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qadd16_Rule_125_A1_P252_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case251
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase244 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case251()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase244(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uqadd16_Rule_247_A1_P488_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Qasx_Rule_127_A1_P256',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Qasx_Rule_127_A1_P256,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case252
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase245 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case252()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase245(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qasx_Rule_127_A1_P256_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case253
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase245 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case253()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase245(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uqasx_Rule_249_A1_P492_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Qsax_Rule_130_A1_P262',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Qsax_Rule_130_A1_P262,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case254
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase246 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case254()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase246(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qsax_Rule_130_A1_P262_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case255
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase246 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case255()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase246(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uqsax_Rule_250_A1_P494_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Qsub16_Rule_132_A1_P266',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Qsub16_Rule_132_A1_P266,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case256
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase247 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case256()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase247(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qsub16_Rule_132_A1_P266_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case257
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase247 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case257()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase247(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uqsub16_Rule_251_A1_P496_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Qadd8_Rule_126_A1_P254',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Qadd8_Rule_126_A1_P254,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case258
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase248 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case258()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase248(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qadd8_Rule_126_A1_P254_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case259
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase248 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case259()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase248(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uqadd8_Rule_248_A1_P490_instance_)
  {}
};

// Neutral case:
// inst(21:20)=10 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Qsub8_Rule_133_A1_P268',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Qsub8_Rule_133_A1_P268,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case260
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase249 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case260()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase249(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qsub8_Rule_133_A1_P268_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case261
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase249 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case261()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase249(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uqsub8_Rule_252_A1_P498_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Shadd16_Rule_159_A1_P318',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Shadd16_Rule_159_A1_P318,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case262
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase250 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case262()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase250(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shadd16_Rule_159_A1_P318_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case263
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase250 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case263()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase250(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uhadd16_Rule_238_A1_P470_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Shasx_Rule_161_A1_P322',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Shasx_Rule_161_A1_P322,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case264
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase251 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case264()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase251(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shasx_Rule_161_A1_P322_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case265
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase251 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case265()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase251(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uhasx_Rule_240_A1_P474_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Shsax_Rule_162_A1_P324',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Shsax_Rule_162_A1_P324,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case266
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase252 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case266()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase252(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shsax_Rule_162_A1_P324_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case267
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase252 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case267()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase252(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uhsax_Rule_241_A1_P476_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Shsub16_Rule_163_A1_P326',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Shsub16_Rule_163_A1_P326,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case268
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase253 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case268()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase253(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shsub16_Rule_163_A1_P326_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case269
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase253 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case269()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase253(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uhsub16_Rule_242_A1_P478_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Shadd8_Rule_160_A1_P320',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Shadd8_Rule_160_A1_P320,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case270
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase254 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case270()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase254(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shadd8_Rule_160_A1_P320_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case271
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase254 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case271()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase254(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uhadd8_Rule_239_A1_P472_instance_)
  {}
};

// Neutral case:
// inst(21:20)=11 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Shsub8_Rule_164_A1_P328',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Shsub8_Rule_164_A1_P328,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case272
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase255 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case272()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase255(
      state_.Binary3RegisterOpAltBNoCondUpdates_Shsub8_Rule_164_A1_P328_instance_)
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
class Binary3RegisterOpAltBNoCondUpdatesTester_Case273
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase255 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case273()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase255(
      state_.Binary3RegisterOpAltBNoCondUpdates_Uhsub8_Rule_243_A1_P480_instance_)
  {}
};

// Neutral case:
// inst(22:21)=00 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Qadd_Rule_124_A1_P250',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=00 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Qadd_Rule_124_A1_P250,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case274
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase256 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case274()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase256(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qadd_Rule_124_A1_P250_instance_)
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
class Binary4RegisterDualOpTester_Case275
    : public Binary4RegisterDualOpTesterCase257 {
 public:
  Binary4RegisterDualOpTester_Case275()
    : Binary4RegisterDualOpTesterCase257(
      state_.Binary4RegisterDualOp_Smlaxx_Rule_166_A1_P330_instance_)
  {}
};

// Neutral case:
// inst(22:20)=01x & inst(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp {'constraints': ,
//     'rule': 'Ssat_Rule_183_A1_P362',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=01x & op2(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     rule: Ssat_Rule_183_A1_P362,
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTester_Case276
    : public Unary2RegisterSatImmedShiftedOpTesterCase258 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_Case276()
    : Unary2RegisterSatImmedShiftedOpTesterCase258(
      state_.Unary2RegisterSatImmedShiftedOp_Ssat_Rule_183_A1_P362_instance_)
  {}
};

// Neutral case:
// inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Qsub_Rule_131_A1_P264',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Qsub_Rule_131_A1_P264,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case277
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase259 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case277()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase259(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qsub_Rule_131_A1_P264_instance_)
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
class Binary4RegisterDualOpTester_Case278
    : public Binary4RegisterDualOpTesterCase260 {
 public:
  Binary4RegisterDualOpTester_Case278()
    : Binary4RegisterDualOpTesterCase260(
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
class Binary3RegisterOpAltATester_Case279
    : public Binary3RegisterOpAltATesterCase261 {
 public:
  Binary3RegisterOpAltATester_Case279()
    : Binary3RegisterOpAltATesterCase261(
      state_.Binary3RegisterOpAltA_Smulwx_Rule_180_A1_P358_instance_)
  {}
};

// Neutral case:
// inst(22:21)=10 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Qdadd_Rule_128_A1_P258',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=10 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Qdadd_Rule_128_A1_P258,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case280
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase262 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case280()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase262(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qdadd_Rule_128_A1_P258_instance_)
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
class Binary4RegisterDualResultTester_Case281
    : public Binary4RegisterDualResultTesterCase263 {
 public:
  Binary4RegisterDualResultTester_Case281()
    : Binary4RegisterDualResultTesterCase263(
      state_.Binary4RegisterDualResult_Smlalxx_Rule_169_A1_P336_instance_)
  {}
};

// Neutral case:
// inst(22:20)=11x & inst(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp {'constraints': ,
//     'rule': 'Usat_Rule_255_A1_P504',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=11x & op2(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp {constraints: ,
//     rule: Usat_Rule_255_A1_P504,
//     safety: ['RegsNotPc']}
class Unary2RegisterSatImmedShiftedOpTester_Case282
    : public Unary2RegisterSatImmedShiftedOpTesterCase264 {
 public:
  Unary2RegisterSatImmedShiftedOpTester_Case282()
    : Unary2RegisterSatImmedShiftedOpTesterCase264(
      state_.Unary2RegisterSatImmedShiftedOp_Usat_Rule_255_A1_P504_instance_)
  {}
};

// Neutral case:
// inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {'constraints': ,
//     'rule': 'Qdsub_Rule_129_A1_P260',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates {constraints: ,
//     rule: Qdsub_Rule_129_A1_P260,
//     safety: ['RegsNotPc']}
class Binary3RegisterOpAltBNoCondUpdatesTester_Case283
    : public Binary3RegisterOpAltBNoCondUpdatesTesterCase265 {
 public:
  Binary3RegisterOpAltBNoCondUpdatesTester_Case283()
    : Binary3RegisterOpAltBNoCondUpdatesTesterCase265(
      state_.Binary3RegisterOpAltBNoCondUpdates_Qdsub_Rule_129_A1_P260_instance_)
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
class Binary3RegisterOpAltATester_Case284
    : public Binary3RegisterOpAltATesterCase266 {
 public:
  Binary3RegisterOpAltATester_Case284()
    : Binary3RegisterOpAltATesterCase266(
      state_.Binary3RegisterOpAltA_Smulxx_Rule_178_P354_instance_)
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
class BranchImmediate24Tester_Case285
    : public BranchImmediate24TesterCase267 {
 public:
  BranchImmediate24Tester_Case285()
    : BranchImmediate24TesterCase267(
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
class BranchImmediate24Tester_Case286
    : public BranchImmediate24TesterCase268 {
 public:
  BranchImmediate24Tester_Case286()
    : BranchImmediate24TesterCase268(
      state_.BranchImmediate24_Bl_Blx_Rule_23_A1_P58_instance_)
  {}
};

// Neutral case:
// inst(8)=0 & inst(7:4)=00x1
//    = MoveDoubleVfpRegisterOp {'constraints': ,
//     'rule': 'Vmov_two_S_Rule_A1'}
//
// Representative case:
// C(8)=0 & op(7:4)=00x1
//    = MoveDoubleVfpRegisterOp {constraints: ,
//     rule: Vmov_two_S_Rule_A1}
class MoveDoubleVfpRegisterOpTester_Case287
    : public MoveDoubleVfpRegisterOpTesterCase269 {
 public:
  MoveDoubleVfpRegisterOpTester_Case287()
    : MoveDoubleVfpRegisterOpTesterCase269(
      state_.MoveDoubleVfpRegisterOp_Vmov_two_S_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(8)=1 & inst(7:4)=00x1
//    = MoveDoubleVfpRegisterOp {'constraints': ,
//     'rule': 'Vmov_one_D_Rule_A1'}
//
// Representative case:
// C(8)=1 & op(7:4)=00x1
//    = MoveDoubleVfpRegisterOp {constraints: ,
//     rule: Vmov_one_D_Rule_A1}
class MoveDoubleVfpRegisterOpTester_Case288
    : public MoveDoubleVfpRegisterOpTesterCase270 {
 public:
  MoveDoubleVfpRegisterOpTester_Case288()
    : MoveDoubleVfpRegisterOpTesterCase270(
      state_.MoveDoubleVfpRegisterOp_Vmov_one_D_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(20)=0 & inst(8)=0 & inst(23:21)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = MoveVfpRegisterOp {'constraints': ,
//     'rule': 'Vmov_Rule_330_A1_P648'}
//
// Representative case:
// L(20)=0 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = MoveVfpRegisterOp {constraints: ,
//     rule: Vmov_Rule_330_A1_P648}
class MoveVfpRegisterOpTester_Case289
    : public MoveVfpRegisterOpTesterCase271 {
 public:
  MoveVfpRegisterOpTester_Case289()
    : MoveVfpRegisterOpTesterCase271(
      state_.MoveVfpRegisterOp_Vmov_Rule_330_A1_P648_instance_)
  {}
};

// Neutral case:
// inst(20)=0 & inst(8)=0 & inst(23:21)=111 & inst(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = VfpUsesRegOp {'constraints': ,
//     'rule': 'Vmsr_Rule_336_A1_P660'}
//
// Representative case:
// L(20)=0 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = VfpUsesRegOp {constraints: ,
//     rule: Vmsr_Rule_336_A1_P660}
class VfpUsesRegOpTester_Case290
    : public VfpUsesRegOpTesterCase272 {
 public:
  VfpUsesRegOpTester_Case290()
    : VfpUsesRegOpTesterCase272(
      state_.VfpUsesRegOp_Vmsr_Rule_336_A1_P660_instance_)
  {}
};

// Neutral case:
// inst(20)=0 & inst(8)=1 & inst(23:21)=0xx & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = MoveVfpRegisterOpWithTypeSel {'constraints': ,
//     'rule': 'Vmov_Rule_328_A1_P644'}
//
// Representative case:
// L(20)=0 & C(8)=1 & A(23:21)=0xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = MoveVfpRegisterOpWithTypeSel {constraints: ,
//     rule: Vmov_Rule_328_A1_P644}
class MoveVfpRegisterOpWithTypeSelTester_Case291
    : public MoveVfpRegisterOpWithTypeSelTesterCase273 {
 public:
  MoveVfpRegisterOpWithTypeSelTester_Case291()
    : MoveVfpRegisterOpWithTypeSelTesterCase273(
      state_.MoveVfpRegisterOpWithTypeSel_Vmov_Rule_328_A1_P644_instance_)
  {}
};

// Neutral case:
// inst(20)=0 & inst(8)=1 & inst(23:21)=1xx & inst(6:5)=0x & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = DuplicateToAdvSIMDRegisters {'constraints': ,
//     'rule': 'Vdup_Rule_303_A1_P594'}
//
// Representative case:
// L(20)=0 & C(8)=1 & A(23:21)=1xx & B(6:5)=0x & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = DuplicateToAdvSIMDRegisters {constraints: ,
//     rule: Vdup_Rule_303_A1_P594}
class DuplicateToAdvSIMDRegistersTester_Case292
    : public DuplicateToAdvSIMDRegistersTesterCase274 {
 public:
  DuplicateToAdvSIMDRegistersTester_Case292()
    : DuplicateToAdvSIMDRegistersTesterCase274(
      state_.DuplicateToAdvSIMDRegisters_Vdup_Rule_303_A1_P594_instance_)
  {}
};

// Neutral case:
// inst(20)=1 & inst(8)=0 & inst(23:21)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = MoveVfpRegisterOp {'constraints': ,
//     'rule': 'Vmov_Rule_330_A1_P648'}
//
// Representative case:
// L(20)=1 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = MoveVfpRegisterOp {constraints: ,
//     rule: Vmov_Rule_330_A1_P648}
class MoveVfpRegisterOpTester_Case293
    : public MoveVfpRegisterOpTesterCase275 {
 public:
  MoveVfpRegisterOpTester_Case293()
    : MoveVfpRegisterOpTesterCase275(
      state_.MoveVfpRegisterOp_Vmov_Rule_330_A1_P648_instance_)
  {}
};

// Neutral case:
// inst(20)=1 & inst(8)=0 & inst(23:21)=111 & inst(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = VfpMrsOp {'constraints': ,
//     'rule': 'Vmrs_Rule_335_A1_P658'}
//
// Representative case:
// L(20)=1 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = VfpMrsOp {constraints: ,
//     rule: Vmrs_Rule_335_A1_P658}
class VfpMrsOpTester_Case294
    : public VfpMrsOpTesterCase276 {
 public:
  VfpMrsOpTester_Case294()
    : VfpMrsOpTesterCase276(
      state_.VfpMrsOp_Vmrs_Rule_335_A1_P658_instance_)
  {}
};

// Neutral case:
// inst(20)=1 & inst(8)=1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = MoveVfpRegisterOpWithTypeSel {'constraints': ,
//     'rule': 'Vmov_Rule_329_A1_P646'}
//
// Representative case:
// L(20)=1 & C(8)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = MoveVfpRegisterOpWithTypeSel {constraints: ,
//     rule: Vmov_Rule_329_A1_P646}
class MoveVfpRegisterOpWithTypeSelTester_Case295
    : public MoveVfpRegisterOpWithTypeSelTesterCase277 {
 public:
  MoveVfpRegisterOpWithTypeSelTester_Case295()
    : MoveVfpRegisterOpWithTypeSelTesterCase277(
      state_.MoveVfpRegisterOpWithTypeSel_Vmov_Rule_329_A1_P646_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000000 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = CondDecoder {'constraints': ,
//     'rule': 'Nop_Rule_110_A1_P222'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000000 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = CondDecoder {constraints: ,
//     rule: Nop_Rule_110_A1_P222}
class CondDecoderTester_Case296
    : public CondDecoderTesterCase278 {
 public:
  CondDecoderTester_Case296()
    : CondDecoderTesterCase278(
      state_.CondDecoder_Nop_Rule_110_A1_P222_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000001 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = CondDecoder {'constraints': ,
//     'rule': 'Yield_Rule_413_A1_P812'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000001 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = CondDecoder {constraints: ,
//     rule: Yield_Rule_413_A1_P812}
class CondDecoderTester_Case297
    : public CondDecoderTesterCase279 {
 public:
  CondDecoderTester_Case297()
    : CondDecoderTesterCase279(
      state_.CondDecoder_Yield_Rule_413_A1_P812_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000010 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Wfe_Rule_411_A1_P808'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000010 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Wfe_Rule_411_A1_P808}
class ForbiddenCondDecoderTester_Case298
    : public UnsafeCondDecoderTesterCase280 {
 public:
  ForbiddenCondDecoderTester_Case298()
    : UnsafeCondDecoderTesterCase280(
      state_.ForbiddenCondDecoder_Wfe_Rule_411_A1_P808_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000011 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Wfi_Rule_412_A1_P810'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000011 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Wfi_Rule_412_A1_P810}
class ForbiddenCondDecoderTester_Case299
    : public UnsafeCondDecoderTesterCase281 {
 public:
  ForbiddenCondDecoderTester_Case299()
    : UnsafeCondDecoderTesterCase281(
      state_.ForbiddenCondDecoder_Wfi_Rule_412_A1_P810_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000100 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Sev_Rule_158_A1_P316'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000100 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Sev_Rule_158_A1_P316}
class ForbiddenCondDecoderTester_Case300
    : public UnsafeCondDecoderTesterCase282 {
 public:
  ForbiddenCondDecoderTester_Case300()
    : UnsafeCondDecoderTesterCase282(
      state_.ForbiddenCondDecoder_Sev_Rule_158_A1_P316_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=1111xxxx & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Dbg_Rule_40_A1_P88'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=1111xxxx & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Dbg_Rule_40_A1_P88}
class ForbiddenCondDecoderTester_Case301
    : public UnsafeCondDecoderTesterCase283 {
 public:
  ForbiddenCondDecoderTester_Case301()
    : UnsafeCondDecoderTesterCase283(
      state_.ForbiddenCondDecoder_Dbg_Rule_40_A1_P88_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=0100 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = MoveImmediate12ToApsr {'constraints': ,
//     'rule': 'Msr_Rule_103_A1_P208'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0100 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = MoveImmediate12ToApsr {constraints: ,
//     rule: Msr_Rule_103_A1_P208}
class MoveImmediate12ToApsrTester_Case302
    : public MoveImmediate12ToApsrTesterCase284 {
 public:
  MoveImmediate12ToApsrTester_Case302()
    : MoveImmediate12ToApsrTesterCase284(
      state_.MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=1x00 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = MoveImmediate12ToApsr {'constraints': ,
//     'rule': 'Msr_Rule_103_A1_P208'}
//
// Representative case:
// op(22)=0 & op1(19:16)=1x00 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = MoveImmediate12ToApsr {constraints: ,
//     rule: Msr_Rule_103_A1_P208}
class MoveImmediate12ToApsrTester_Case303
    : public MoveImmediate12ToApsrTesterCase285 {
 public:
  MoveImmediate12ToApsrTester_Case303()
    : MoveImmediate12ToApsrTesterCase285(
      state_.MoveImmediate12ToApsr_Msr_Rule_103_A1_P208_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Msr_Rule_B6_1_6_A1_PB6_12'}
//
// Representative case:
// op(22)=0 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Msr_Rule_B6_1_6_A1_PB6_12}
class ForbiddenCondDecoderTester_Case304
    : public UnsafeCondDecoderTesterCase286 {
 public:
  ForbiddenCondDecoderTester_Case304()
    : UnsafeCondDecoderTesterCase286(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12_instance_)
  {}
};

// Neutral case:
// inst(22)=0 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Msr_Rule_B6_1_6_A1_PB6_12'}
//
// Representative case:
// op(22)=0 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Msr_Rule_B6_1_6_A1_PB6_12}
class ForbiddenCondDecoderTester_Case305
    : public UnsafeCondDecoderTesterCase287 {
 public:
  ForbiddenCondDecoderTester_Case305()
    : UnsafeCondDecoderTesterCase287(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12_instance_)
  {}
};

// Neutral case:
// inst(22)=1 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Msr_Rule_B6_1_6_A1_PB6_12'}
//
// Representative case:
// op(22)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Msr_Rule_B6_1_6_A1_PB6_12}
class ForbiddenCondDecoderTester_Case306
    : public UnsafeCondDecoderTesterCase288 {
 public:
  ForbiddenCondDecoderTester_Case306()
    : UnsafeCondDecoderTesterCase288(
      state_.ForbiddenCondDecoder_Msr_Rule_B6_1_6_A1_PB6_12_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=0x010
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Strt_Rule_A1'}
//
// Representative case:
// A(25)=0 & op1(24:20)=0x010
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Strt_Rule_A1}
class ForbiddenCondDecoderTester_Case307
    : public UnsafeCondDecoderTesterCase289 {
 public:
  ForbiddenCondDecoderTester_Case307()
    : UnsafeCondDecoderTesterCase289(
      state_.ForbiddenCondDecoder_Strt_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=0x011
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Ldrt_Rule_A1'}
//
// Representative case:
// A(25)=0 & op1(24:20)=0x011
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Ldrt_Rule_A1}
class ForbiddenCondDecoderTester_Case308
    : public UnsafeCondDecoderTesterCase290 {
 public:
  ForbiddenCondDecoderTester_Case308()
    : UnsafeCondDecoderTesterCase290(
      state_.ForbiddenCondDecoder_Ldrt_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=0x110
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Strtb_Rule_A1'}
//
// Representative case:
// A(25)=0 & op1(24:20)=0x110
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Strtb_Rule_A1}
class ForbiddenCondDecoderTester_Case309
    : public UnsafeCondDecoderTesterCase291 {
 public:
  ForbiddenCondDecoderTester_Case309()
    : UnsafeCondDecoderTesterCase291(
      state_.ForbiddenCondDecoder_Strtb_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=0x111
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Ldrtb_Rule_A1'}
//
// Representative case:
// A(25)=0 & op1(24:20)=0x111
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Ldrtb_Rule_A1}
class ForbiddenCondDecoderTester_Case310
    : public UnsafeCondDecoderTesterCase292 {
 public:
  ForbiddenCondDecoderTester_Case310()
    : UnsafeCondDecoderTesterCase292(
      state_.ForbiddenCondDecoder_Ldrtb_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=~1111 & inst(24:20)=~0x011
//    = Load2RegisterImm12Op {'constraints': ,
//     'rule': 'Ldr_Rule_58_A1_P120',
//     'safety': ["'NotRnIsPc'"]}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x011
//    = Load2RegisterImm12Op {constraints: ,
//     rule: Ldr_Rule_58_A1_P120,
//     safety: ['NotRnIsPc']}
class Load2RegisterImm12OpTester_Case311
    : public LoadStore2RegisterImm12OpTesterCase293 {
 public:
  Load2RegisterImm12OpTester_Case311()
    : LoadStore2RegisterImm12OpTesterCase293(
      state_.Load2RegisterImm12Op_Ldr_Rule_58_A1_P120_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=1111 & inst(24:20)=~0x011 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = Load2RegisterImm12Op {'constraints': ,
//     'rule': 'Ldr_Rule_59_A1_P122'}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x011 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = Load2RegisterImm12Op {constraints: ,
//     rule: Ldr_Rule_59_A1_P122}
class Load2RegisterImm12OpTester_Case312
    : public LoadStore2RegisterImm12OpTesterCase294 {
 public:
  Load2RegisterImm12OpTester_Case312()
    : LoadStore2RegisterImm12OpTesterCase294(
      state_.Load2RegisterImm12Op_Ldr_Rule_59_A1_P122_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x0 & inst(24:20)=~0x110
//    = Store2RegisterImm12Op {'constraints': ,
//     'rule': 'Strb_Rule_197_A1_P390'}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx1x0 & op1_repeated(24:20)=~0x110
//    = Store2RegisterImm12Op {constraints: ,
//     rule: Strb_Rule_197_A1_P390}
class Store2RegisterImm12OpTester_Case313
    : public LoadStore2RegisterImm12OpTesterCase295 {
 public:
  Store2RegisterImm12OpTester_Case313()
    : LoadStore2RegisterImm12OpTesterCase295(
      state_.Store2RegisterImm12Op_Strb_Rule_197_A1_P390_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x1 & inst(19:16)=~1111 & inst(24:20)=~0x111
//    = Load2RegisterImm12Op {'constraints': ,
//     'rule': 'Ldrb_Rule_62_A1_P128',
//     'safety': ["'NotRnIsPc'"]}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x111
//    = Load2RegisterImm12Op {constraints: ,
//     rule: Ldrb_Rule_62_A1_P128,
//     safety: ['NotRnIsPc']}
class Load2RegisterImm12OpTester_Case314
    : public LoadStore2RegisterImm12OpTesterCase296 {
 public:
  Load2RegisterImm12OpTester_Case314()
    : LoadStore2RegisterImm12OpTesterCase296(
      state_.Load2RegisterImm12Op_Ldrb_Rule_62_A1_P128_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(24:20)=~0x111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = Load2RegisterImm12Op {'constraints': ,
//     'rule': 'Ldrb_Rule_63_A1_P130'}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = Load2RegisterImm12Op {constraints: ,
//     rule: Ldrb_Rule_63_A1_P130}
class Load2RegisterImm12OpTester_Case315
    : public LoadStore2RegisterImm12OpTesterCase297 {
 public:
  Load2RegisterImm12OpTester_Case315()
    : LoadStore2RegisterImm12OpTesterCase297(
      state_.Load2RegisterImm12Op_Ldrb_Rule_63_A1_P130_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=0xx1x & inst(7:4)=1011
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'extra_load_store_instructions_unpriviledged'}
//
// Representative case:
// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=1011
//    = ForbiddenCondDecoder {constraints: ,
//     rule: extra_load_store_instructions_unpriviledged}
class ForbiddenCondDecoderTester_Case316
    : public UnsafeCondDecoderTesterCase298 {
 public:
  ForbiddenCondDecoderTester_Case316()
    : UnsafeCondDecoderTesterCase298(
      state_.ForbiddenCondDecoder_extra_load_store_instructions_unpriviledged_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=0xx1x & inst(7:4)=11x1
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'extra_load_store_instructions_unpriviledged'}
//
// Representative case:
// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=11x1
//    = ForbiddenCondDecoder {constraints: ,
//     rule: extra_load_store_instructions_unpriviledged}
class ForbiddenCondDecoderTester_Case317
    : public UnsafeCondDecoderTesterCase299 {
 public:
  ForbiddenCondDecoderTester_Case317()
    : UnsafeCondDecoderTesterCase299(
      state_.ForbiddenCondDecoder_extra_load_store_instructions_unpriviledged_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=10000
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'rule': 'Mov_Rule_96_A2_P194',
//     'safety': ['inst(15:12)=1111 => UNPREDICTABLE']}
//
// Representative case:
// op(25)=1 & op1(24:20)=10000
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: Mov_Rule_96_A2_P194,
//     safety: [Rd(15:12)=1111 => UNPREDICTABLE]}
class Unary1RegisterImmediateOpTester_Case318
    : public Unary1RegisterImmediateOpTesterCase300 {
 public:
  Unary1RegisterImmediateOpTester_Case318()
    : Unary1RegisterImmediateOpTesterCase300(
      state_.Unary1RegisterImmediateOp_Mov_Rule_96_A2_P194_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=10100
//    = Unary1RegisterImmediateOp {'constraints': ,
//     'rule': 'Mov_Rule_99_A1_P200',
//     'safety': ['inst(15:12)=1111 => UNPREDICTABLE']}
//
// Representative case:
// op(25)=1 & op1(24:20)=10100
//    = Unary1RegisterImmediateOp {constraints: ,
//     rule: Mov_Rule_99_A1_P200,
//     safety: [Rd(15:12)=1111 => UNPREDICTABLE]}
class Unary1RegisterImmediateOpTester_Case319
    : public Unary1RegisterImmediateOpTesterCase301 {
 public:
  Unary1RegisterImmediateOpTester_Case319()
    : Unary1RegisterImmediateOpTesterCase301(
      state_.Unary1RegisterImmediateOp_Mov_Rule_99_A1_P200_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=0x010 & inst(4)=0
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Strt_Rule_A2'}
//
// Representative case:
// A(25)=1 & op1(24:20)=0x010 & B(4)=0
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Strt_Rule_A2}
class ForbiddenCondDecoderTester_Case320
    : public UnsafeCondDecoderTesterCase302 {
 public:
  ForbiddenCondDecoderTester_Case320()
    : UnsafeCondDecoderTesterCase302(
      state_.ForbiddenCondDecoder_Strt_Rule_A2_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=0x011 & inst(4)=0
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Ldrt_Rule_A2'}
//
// Representative case:
// A(25)=1 & op1(24:20)=0x011 & B(4)=0
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Ldrt_Rule_A2}
class ForbiddenCondDecoderTester_Case321
    : public UnsafeCondDecoderTesterCase303 {
 public:
  ForbiddenCondDecoderTester_Case321()
    : UnsafeCondDecoderTesterCase303(
      state_.ForbiddenCondDecoder_Ldrt_Rule_A2_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=0x110 & inst(4)=0
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Strtb_Rule_A2'}
//
// Representative case:
// A(25)=1 & op1(24:20)=0x110 & B(4)=0
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Strtb_Rule_A2}
class ForbiddenCondDecoderTester_Case322
    : public UnsafeCondDecoderTesterCase304 {
 public:
  ForbiddenCondDecoderTester_Case322()
    : UnsafeCondDecoderTesterCase304(
      state_.ForbiddenCondDecoder_Strtb_Rule_A2_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=0x111 & inst(4)=0
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Ldrtb_Rule_A2'}
//
// Representative case:
// A(25)=1 & op1(24:20)=0x111 & B(4)=0
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Ldrtb_Rule_A2}
class ForbiddenCondDecoderTester_Case323
    : public UnsafeCondDecoderTesterCase305 {
 public:
  ForbiddenCondDecoderTester_Case323()
    : UnsafeCondDecoderTesterCase305(
      state_.ForbiddenCondDecoder_Ldrtb_Rule_A2_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=xx0x0 & inst(4)=0 & inst(24:20)=~0x010
//    = Store3RegisterImm5Op {'constraints': ,
//     'rule': 'Str_Rule_195_A1_P386'}
//
// Representative case:
// A(25)=1 & op1(24:20)=xx0x0 & B(4)=0 & op1_repeated(24:20)=~0x010
//    = Store3RegisterImm5Op {constraints: ,
//     rule: Str_Rule_195_A1_P386}
class Store3RegisterImm5OpTester_Case324
    : public LoadStore3RegisterImm5OpTesterCase306 {
 public:
  Store3RegisterImm5OpTester_Case324()
    : LoadStore3RegisterImm5OpTesterCase306(
      state_.Store3RegisterImm5Op_Str_Rule_195_A1_P386_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=xx0x1 & inst(4)=0 & inst(24:20)=~0x011
//    = Load3RegisterImm5Op {'constraints': ,
//     'rule': 'Ldr_Rule_60_A1_P124'}
//
// Representative case:
// A(25)=1 & op1(24:20)=xx0x1 & B(4)=0 & op1_repeated(24:20)=~0x011
//    = Load3RegisterImm5Op {constraints: ,
//     rule: Ldr_Rule_60_A1_P124}
class Load3RegisterImm5OpTester_Case325
    : public LoadStore3RegisterImm5OpTesterCase307 {
 public:
  Load3RegisterImm5OpTester_Case325()
    : LoadStore3RegisterImm5OpTesterCase307(
      state_.Load3RegisterImm5Op_Ldr_Rule_60_A1_P124_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=xx1x0 & inst(4)=0 & inst(24:20)=~0x110
//    = Store3RegisterImm5Op {'constraints': ,
//     'rule': 'Strb_Rule_198_A1_P392'}
//
// Representative case:
// A(25)=1 & op1(24:20)=xx1x0 & B(4)=0 & op1_repeated(24:20)=~0x110
//    = Store3RegisterImm5Op {constraints: ,
//     rule: Strb_Rule_198_A1_P392}
class Store3RegisterImm5OpTester_Case326
    : public LoadStore3RegisterImm5OpTesterCase308 {
 public:
  Store3RegisterImm5OpTester_Case326()
    : LoadStore3RegisterImm5OpTesterCase308(
      state_.Store3RegisterImm5Op_Strb_Rule_198_A1_P392_instance_)
  {}
};

// Neutral case:
// inst(25)=1 & inst(24:20)=xx1x1 & inst(4)=0 & inst(24:20)=~0x111
//    = Load3RegisterImm5Op {'constraints': ,
//     'rule': 'Ldrb_Rule_64_A1_P132'}
//
// Representative case:
// A(25)=1 & op1(24:20)=xx1x1 & B(4)=0 & op1_repeated(24:20)=~0x111
//    = Load3RegisterImm5Op {constraints: ,
//     rule: Ldrb_Rule_64_A1_P132}
class Load3RegisterImm5OpTester_Case327
    : public LoadStore3RegisterImm5OpTesterCase309 {
 public:
  Load3RegisterImm5OpTester_Case327()
    : LoadStore3RegisterImm5OpTesterCase309(
      state_.Load3RegisterImm5Op_Ldrb_Rule_64_A1_P132_instance_)
  {}
};

// Neutral case:
// 
//    = Store2RegisterImm12Op {'constraints': & ~cccc010100101101tttt000000000100 ,
//     'rule': 'Str_Rule_194_A1_P384'}
//
// Representative case:
// 
//    = Store2RegisterImm12Op {constraints: & ~cccc010100101101tttt000000000100 ,
//     rule: Str_Rule_194_A1_P384}
class Store2RegisterImm12OpTester_Case328
    : public LoadStore2RegisterImm12OpTesterCase310 {
 public:
  Store2RegisterImm12OpTester_Case328()
    : LoadStore2RegisterImm12OpTesterCase310(
      state_.Store2RegisterImm12Op_Str_Rule_194_A1_P384_instance_)
  {}
};

// Neutral case:
// inst(25:20)=00000x
//    = UndefinedCondDecoder {'constraints': ,
//     'rule': 'Undefined_A5_6'}
//
// Representative case:
// op1(25:20)=00000x
//    = UndefinedCondDecoder {constraints: ,
//     rule: Undefined_A5_6}
class UndefinedCondDecoderTester_Case329
    : public UnsafeCondDecoderTesterCase311 {
 public:
  UndefinedCondDecoderTester_Case329()
    : UnsafeCondDecoderTesterCase311(
      state_.UndefinedCondDecoder_Undefined_A5_6_instance_)
  {}
};

// Neutral case:
// inst(25:20)=11xxxx
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Svc_Rule_A1'}
//
// Representative case:
// op1(25:20)=11xxxx
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Svc_Rule_A1}
class ForbiddenCondDecoderTester_Case330
    : public UnsafeCondDecoderTesterCase312 {
 public:
  ForbiddenCondDecoderTester_Case330()
    : UnsafeCondDecoderTesterCase312(
      state_.ForbiddenCondDecoder_Svc_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(7:6)=x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
//    = CondVfpOp {'constraints': ,
//     'rule': 'Vmov_Rule_326_A2_P640'}
//
// Representative case:
// opc3(7:6)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
//    = CondVfpOp {constraints: ,
//     rule: Vmov_Rule_326_A2_P640}
class CondVfpOpTester_Case331
    : public CondVfpOpTesterCase313 {
 public:
  CondVfpOpTester_Case331()
    : CondVfpOpTesterCase313(
      state_.CondVfpOp_Vmov_Rule_326_A2_P640_instance_)
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
// inst(27:20)=11000100
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '111111000100ssssttttiiiiiiiiiiii',
//     'rule': 'Mcrr2_Rule_93_A2_P188'}
//
// Representative case:
// op1(27:20)=11000100
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 111111000100ssssttttiiiiiiiiiiii,
//     rule: Mcrr2_Rule_93_A2_P188}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case0_TestCase0) {
  ForbiddenUncondDecoderTester_Case0 baseline_tester;
  NamedForbidden_Mcrr2_Rule_93_A2_P188 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111111000100ssssttttiiiiiiiiiiii");
}

// Neutral case:
// inst(27:20)=11000101
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '111111000101ssssttttiiiiiiiiiiii',
//     'rule': 'Mrrc2_Rule_101_A2_P204'}
//
// Representative case:
// op1(27:20)=11000101
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 111111000101ssssttttiiiiiiiiiiii,
//     rule: Mrrc2_Rule_101_A2_P204}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case1_TestCase1) {
  ForbiddenUncondDecoderTester_Case1 baseline_tester;
  NamedForbidden_Mrrc2_Rule_101_A2_P204 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111111000101ssssttttiiiiiiiiiiii");
}

// Neutral case:
// inst(26:20)=0010000 & inst(7:4)=0000 & inst(19:16)=xxx1 & inst(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '1111000100000001000000i000000000',
//     'rule': 'Setend_Rule_157_P314'}
//
// Representative case:
// op1(26:20)=0010000 & op2(7:4)=0000 & Rn(19:16)=xxx1 & $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 1111000100000001000000i000000000,
//     rule: Setend_Rule_157_P314}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case2_TestCase2) {
  ForbiddenUncondDecoderTester_Case2 baseline_tester;
  NamedForbidden_Setend_Rule_157_P314 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111000100000001000000i000000000");
}

// Neutral case:
// inst(26:20)=0010000 & inst(7:4)=xx0x & inst(19:16)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '111100010000iii00000000iii0iiiii',
//     'rule': 'Cps_Rule_b6_1_1_A1_B6_3'}
//
// Representative case:
// op1(26:20)=0010000 & op2(7:4)=xx0x & Rn(19:16)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 111100010000iii00000000iii0iiiii,
//     rule: Cps_Rule_b6_1_1_A1_B6_3}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case3_TestCase3) {
  ForbiddenUncondDecoderTester_Case3 baseline_tester;
  NamedForbidden_Cps_Rule_b6_1_1_A1_B6_3 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100010000iii00000000iii0iiiii");
}

// Neutral case:
// inst(26:20)=1010011
//    = UnpredictableUncondDecoder => Unpredictable {'constraints': ,
//     'pattern': '111101010011xxxxxxxxxxxxxxxxxxxx',
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010011
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: 111101010011xxxxxxxxxxxxxxxxxxxx,
//     rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case4_TestCase4) {
  UnpredictableUncondDecoderTester_Case4 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010011xxxxxxxxxxxxxxxxxxxx");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0000
//    = UnpredictableUncondDecoder => Unpredictable {'constraints': ,
//     'pattern': '111101010111xxxxxxxxxxxx0000xxxx',
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0000
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: 111101010111xxxxxxxxxxxx0000xxxx,
//     rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case5_TestCase5) {
  UnpredictableUncondDecoderTester_Case5 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010111xxxxxxxxxxxx0000xxxx");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0001 & inst(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '11110101011111111111000000011111',
//     'rule': 'Clrex_Rule_30_A1_P70'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0001 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 11110101011111111111000000011111,
//     rule: Clrex_Rule_30_A1_P70}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case6_TestCase6) {
  ForbiddenUncondDecoderTester_Case6 baseline_tester;
  NamedForbidden_Clrex_Rule_30_A1_P70 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110101011111111111000000011111");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0100 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier => DataBarrier {'constraints': ,
//     'pattern': '1111010101111111111100000100xxxx',
//     'rule': 'Dsb_Rule_42_A1_P92'}
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0100 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier => DataBarrier {constraints: ,
//     pattern: 1111010101111111111100000100xxxx,
//     rule: Dsb_Rule_42_A1_P92}
TEST_F(Arm32DecoderStateTests,
       DataBarrierTester_Case7_TestCase7) {
  DataBarrierTester_Case7 tester;
  tester.Test("1111010101111111111100000100xxxx");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0101 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier => DataBarrier {'constraints': ,
//     'pattern': '1111010101111111111100000101xxxx',
//     'rule': 'Dmb_Rule_41_A1_P90'}
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0101 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier => DataBarrier {constraints: ,
//     pattern: 1111010101111111111100000101xxxx,
//     rule: Dmb_Rule_41_A1_P90}
TEST_F(Arm32DecoderStateTests,
       DataBarrierTester_Case8_TestCase8) {
  DataBarrierTester_Case8 tester;
  tester.Test("1111010101111111111100000101xxxx");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0110 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = InstructionBarrier => InstructionBarrier {'constraints': ,
//     'pattern': '1111010101111111111100000110xxxx',
//     'rule': 'Isb_Rule_49_A1_P102'}
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0110 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = InstructionBarrier => InstructionBarrier {constraints: ,
//     pattern: 1111010101111111111100000110xxxx,
//     rule: Isb_Rule_49_A1_P102}
TEST_F(Arm32DecoderStateTests,
       InstructionBarrierTester_Case9_TestCase9) {
  InstructionBarrierTester_Case9 tester;
  tester.Test("1111010101111111111100000110xxxx");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0111
//    = UnpredictableUncondDecoder => Unpredictable {'constraints': ,
//     'pattern': '111101010111xxxxxxxxxxxx0111xxxx',
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0111
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: 111101010111xxxxxxxxxxxx0111xxxx,
//     rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case10_TestCase10) {
  UnpredictableUncondDecoderTester_Case10 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010111xxxxxxxxxxxx0111xxxx");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=001x
//    = UnpredictableUncondDecoder => Unpredictable {'constraints': ,
//     'pattern': '111101010111xxxxxxxxxxxx001xxxxx',
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=001x
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: 111101010111xxxxxxxxxxxx001xxxxx,
//     rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case11_TestCase11) {
  UnpredictableUncondDecoderTester_Case11 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010111xxxxxxxxxxxx001xxxxx");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=1xxx
//    = UnpredictableUncondDecoder => Unpredictable {'constraints': ,
//     'pattern': '111101010111xxxxxxxxxxxx1xxxxxxx',
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=1xxx
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: 111101010111xxxxxxxxxxxx1xxxxxxx,
//     rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case12_TestCase12) {
  UnpredictableUncondDecoderTester_Case12 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010111xxxxxxxxxxxx1xxxxxxx");
}

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
       LoadRegisterListTester_Case13_TestCase13) {
  LoadRegisterListTester_Case13 baseline_tester;
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
       LoadRegisterListTester_Case14_TestCase14) {
  LoadRegisterListTester_Case14 baseline_tester;
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
       LoadRegisterListTester_Case15_TestCase15) {
  LoadRegisterListTester_Case15 baseline_tester;
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
       StoreRegisterListTester_Case16_TestCase16) {
  StoreRegisterListTester_Case16 tester;
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
       StoreRegisterListTester_Case17_TestCase17) {
  StoreRegisterListTester_Case17 tester;
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
       StoreRegisterListTester_Case18_TestCase18) {
  StoreRegisterListTester_Case18 tester;
  tester.Test("cccc10010010nnnnrrrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(26:20)=100x001
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '11110100x001xxxxxxxxxxxxxxxxxxxx',
//     'rule': 'Unallocated_hints'}
//
// Representative case:
// op1(26:20)=100x001
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 11110100x001xxxxxxxxxxxxxxxxxxxx,
//     rule: Unallocated_hints}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case19_TestCase19) {
  ForbiddenUncondDecoderTester_Case19 baseline_tester;
  NamedForbidden_Unallocated_hints actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110100x001xxxxxxxxxxxxxxxxxxxx");
}

// Neutral case:
// inst(26:20)=100x101 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op => DontCareInst {'constraints': ,
//     'pattern': '11110100u101nnnn1111iiiiiiiiiiii',
//     'rule': 'Pli_Rule_120_A1_P242'}
//
// Representative case:
// op1(26:20)=100x101 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op => DontCareInst {constraints: ,
//     pattern: 11110100u101nnnn1111iiiiiiiiiiii,
//     rule: Pli_Rule_120_A1_P242}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_Case20_TestCase20) {
  PreloadRegisterImm12OpTester_Case20 baseline_tester;
  NamedDontCareInst_Pli_Rule_120_A1_P242 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110100u101nnnn1111iiiiiiiiiiii");
}

// Neutral case:
// inst(26:20)=101x001 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op => DontCareInst {'constraints': & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     'pattern': '11110101u001nnnn1111iiiiiiiiiiii',
//     'rule': 'Pldw_Rule_117_A1_P236'}
//
// Representative case:
// op1(26:20)=101x001 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op => DontCareInst {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     pattern: 11110101u001nnnn1111iiiiiiiiiiii,
//     rule: Pldw_Rule_117_A1_P236}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_Case21_TestCase21) {
  PreloadRegisterImm12OpTester_Case21 baseline_tester;
  NamedDontCareInst_Pldw_Rule_117_A1_P236 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110101u001nnnn1111iiiiiiiiiiii");
}

// Neutral case:
// inst(26:20)=101x001 & inst(19:16)=1111
//    = UnpredictableUncondDecoder => Unpredictable {'constraints': ,
//     'pattern': '11110101x001xxxxxxxxxxxxxxxxxxxx',
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=101x001 & Rn(19:16)=1111
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: 11110101x001xxxxxxxxxxxxxxxxxxxx,
//     rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case22_TestCase22) {
  UnpredictableUncondDecoderTester_Case22 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110101x001xxxxxxxxxxxxxxxxxxxx");
}

// Neutral case:
// inst(26:20)=101x101 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op => DontCareInst {'constraints': & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     'pattern': '11110101u101nnnn1111iiiiiiiiiiii',
//     'rule': 'Pld_Rule_117_A1_P236'}
//
// Representative case:
// op1(26:20)=101x101 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op => DontCareInst {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     pattern: 11110101u101nnnn1111iiiiiiiiiiii,
//     rule: Pld_Rule_117_A1_P236}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_Case23_TestCase23) {
  PreloadRegisterImm12OpTester_Case23 baseline_tester;
  NamedDontCareInst_Pld_Rule_117_A1_P236 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110101u101nnnn1111iiiiiiiiiiii");
}

// Neutral case:
// inst(26:20)=101x101 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op => DontCareInst {'constraints': ,
//     'pattern': '11110101u10111111111iiiiiiiiiiii',
//     'rule': 'Pld_Rule_118_A1_P238'}
//
// Representative case:
// op1(26:20)=101x101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op => DontCareInst {constraints: ,
//     pattern: 11110101u10111111111iiiiiiiiiiii,
//     rule: Pld_Rule_118_A1_P238}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_Case24_TestCase24) {
  PreloadRegisterImm12OpTester_Case24 baseline_tester;
  NamedDontCareInst_Pld_Rule_118_A1_P238 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110101u10111111111iiiiiiiiiiii");
}

// Neutral case:
// inst(26:20)=110x001 & inst(7:4)=xxx0
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '11110110x001xxxxxxxxxxxxxxx0xxxx',
//     'rule': 'Unallocated_hints'}
//
// Representative case:
// op1(26:20)=110x001 & op2(7:4)=xxx0
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 11110110x001xxxxxxxxxxxxxxx0xxxx,
//     rule: Unallocated_hints}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case25_TestCase25) {
  ForbiddenUncondDecoderTester_Case25 baseline_tester;
  NamedForbidden_Unallocated_hints actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110110x001xxxxxxxxxxxxxxx0xxxx");
}

// Neutral case:
// inst(26:20)=110x101 & inst(7:4)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOp => PreloadRegisterPairOp {'constraints': ,
//     'pattern': '11110110u101nnnn1111iiiiitt0mmmm',
//     'rule': 'Pli_Rule_121_A1_P244'}
//
// Representaive case:
// op1(26:20)=110x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOp => PreloadRegisterPairOp {constraints: ,
//     pattern: 11110110u101nnnn1111iiiiitt0mmmm,
//     rule: Pli_Rule_121_A1_P244}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterPairOpTester_Case26_TestCase26) {
  PreloadRegisterPairOpTester_Case26 tester;
  tester.Test("11110110u101nnnn1111iiiiitt0mmmm");
}

// Neutral case:
// inst(26:20)=111x001 & inst(7:4)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc => PreloadRegisterPairOpWAndRnNotPc {'constraints': ,
//     'pattern': '11110111u001nnnn1111iiiiitt0mmmm',
//     'rule': 'Pldw_Rule_119_A1_P240'}
//
// Representaive case:
// op1(26:20)=111x001 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc => PreloadRegisterPairOpWAndRnNotPc {constraints: ,
//     pattern: 11110111u001nnnn1111iiiiitt0mmmm,
//     rule: Pldw_Rule_119_A1_P240}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterPairOpWAndRnNotPcTester_Case27_TestCase27) {
  PreloadRegisterPairOpWAndRnNotPcTester_Case27 tester;
  tester.Test("11110111u001nnnn1111iiiiitt0mmmm");
}

// Neutral case:
// inst(26:20)=111x101 & inst(7:4)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc => PreloadRegisterPairOpWAndRnNotPc {'constraints': ,
//     'pattern': '11110111u101nnnn1111iiiiitt0mmmm',
//     'rule': 'Pld_Rule_119_A1_P240'}
//
// Representaive case:
// op1(26:20)=111x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc => PreloadRegisterPairOpWAndRnNotPc {constraints: ,
//     pattern: 11110111u101nnnn1111iiiiitt0mmmm,
//     rule: Pld_Rule_119_A1_P240}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterPairOpWAndRnNotPcTester_Case28_TestCase28) {
  PreloadRegisterPairOpWAndRnNotPcTester_Case28 tester;
  tester.Test("11110111u101nnnn1111iiiiitt0mmmm");
}

// Neutral case:
// inst(26:20)=1011x11
//    = UnpredictableUncondDecoder => Unpredictable {'constraints': ,
//     'pattern': '111101011x11xxxxxxxxxxxxxxxxxxxx',
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1011x11
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: 111101011x11xxxxxxxxxxxxxxxxxxxx,
//     rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case29_TestCase29) {
  UnpredictableUncondDecoderTester_Case29 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101011x11xxxxxxxxxxxxxxxxxxxx");
}

// Neutral case:
// inst(24:20)=00100 & inst(19:16)=1111
//    = Unary1RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc001001001111ddddiiiiiiiiiiii',
//     'rule': 'Adr_Rule_10_A2_P32'}
//
// Representative case:
// op(24:20)=00100 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc001001001111ddddiiiiiiiiiiii,
//     rule: Adr_Rule_10_A2_P32}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Case30_TestCase30) {
  Unary1RegisterImmediateOpTester_Case30 baseline_tester;
  NamedDefs12To15_Adr_Rule_10_A2_P32 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001001001111ddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=00101 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc00100101nnnn1111iiiiiiiiiiii',
//     'rule': 'Subs_Pc_Lr_and_related_instructions_Rule_A1a'}
//
// Representative case:
// op(24:20)=00101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc00100101nnnn1111iiiiiiiiiiii,
//     rule: Subs_Pc_Lr_and_related_instructions_Rule_A1a}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case31_TestCase31) {
  ForbiddenCondDecoderTester_Case31 baseline_tester;
  NamedForbidden_Subs_Pc_Lr_and_related_instructions_Rule_A1a actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00100101nnnn1111iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=01000 & inst(19:16)=1111
//    = Unary1RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc001010001111ddddiiiiiiiiiiii',
//     'rule': 'Adr_Rule_10_A1_P32'}
//
// Representative case:
// op(24:20)=01000 & Rn(19:16)=1111
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc001010001111ddddiiiiiiiiiiii,
//     rule: Adr_Rule_10_A1_P32}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Case32_TestCase32) {
  Unary1RegisterImmediateOpTester_Case32 baseline_tester;
  NamedDefs12To15_Adr_Rule_10_A1_P32 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001010001111ddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=01001 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc00101001nnnn1111iiiiiiiiiiii',
//     'rule': 'Subs_Pc_Lr_and_related_instructions_Rule_A1b'}
//
// Representative case:
// op(24:20)=01001 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc00101001nnnn1111iiiiiiiiiiii,
//     rule: Subs_Pc_Lr_and_related_instructions_Rule_A1b}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case33_TestCase33) {
  ForbiddenCondDecoderTester_Case33 baseline_tester;
  NamedForbidden_Subs_Pc_Lr_and_related_instructions_Rule_A1b actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00101001nnnn1111iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest => DontCareInst {'constraints': ,
//     'pattern': 'cccc00010001nnnn0000iiiiitt0mmmm',
//     'rule': 'Tst_Rule_231_A1_P456'}
//
// Representative case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest => DontCareInst {constraints: ,
//     pattern: cccc00010001nnnn0000iiiiitt0mmmm,
//     rule: Tst_Rule_231_A1_P456}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case34_TestCase34) {
  Binary2RegisterImmedShiftedTestTester_Case34 baseline_tester;
  NamedDontCareInst_Tst_Rule_231_A1_P456 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010001nnnn0000iiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = MaskedBinaryRegisterImmediateTest => TestIfAddressMasked {'constraints': ,
//     'pattern': 'cccc00110001nnnn0000iiiiiiiiiiii',
//     'rule': 'Tst_Rule_230_A1_P454'}
//
// Representative case:
// op(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = MaskedBinaryRegisterImmediateTest => TestIfAddressMasked {constraints: ,
//     pattern: cccc00110001nnnn0000iiiiiiiiiiii,
//     rule: Tst_Rule_230_A1_P454}
TEST_F(Arm32DecoderStateTests,
       MaskedBinaryRegisterImmediateTestTester_Case35_TestCase35) {
  MaskedBinaryRegisterImmediateTestTester_Case35 baseline_tester;
  NamedTestIfAddressMasked_Tst_Rule_230_A1_P454 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110001nnnn0000iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=10001 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc00010001nnnn0000ssss0tt1mmmm',
//     'rule': 'Tst_Rule_232_A1_P458',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: cccc00010001nnnn0000ssss0tt1mmmm,
//     rule: Tst_Rule_232_A1_P458,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case36_TestCase36) {
  Binary3RegisterShiftedTestTester_Case36 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Tst_Rule_232_A1_P458 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010001nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest => DontCareInst {'constraints': ,
//     'pattern': 'cccc00010011nnnn0000iiiiitt0mmmm',
//     'rule': 'Teq_Rule_228_A1_P450'}
//
// Representative case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest => DontCareInst {constraints: ,
//     pattern: cccc00010011nnnn0000iiiiitt0mmmm,
//     rule: Teq_Rule_228_A1_P450}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case37_TestCase37) {
  Binary2RegisterImmedShiftedTestTester_Case37 baseline_tester;
  NamedDontCareInst_Teq_Rule_228_A1_P450 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010011nnnn0000iiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest => DontCareInst {'constraints': ,
//     'pattern': 'cccc00110011nnnn0000iiiiiiiiiiii',
//     'rule': 'Teq_Rule_227_A1_P448'}
//
// Representative case:
// op(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest => DontCareInst {constraints: ,
//     pattern: cccc00110011nnnn0000iiiiiiiiiiii,
//     rule: Teq_Rule_227_A1_P448}
TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_Case38_TestCase38) {
  BinaryRegisterImmediateTestTester_Case38 baseline_tester;
  NamedDontCareInst_Teq_Rule_227_A1_P448 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110011nnnn0000iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=10011 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc00010011nnnn0000ssss0tt1mmmm',
//     'rule': 'Teq_Rule_229_A1_P452',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: cccc00010011nnnn0000ssss0tt1mmmm,
//     rule: Teq_Rule_229_A1_P452,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case39_TestCase39) {
  Binary3RegisterShiftedTestTester_Case39 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Teq_Rule_229_A1_P452 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010011nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest => DontCareInst {'constraints': ,
//     'pattern': 'cccc00010101nnnn0000iiiiitt0mmmm',
//     'rule': 'Cmp_Rule_36_A1_P82'}
//
// Representative case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest => DontCareInst {constraints: ,
//     pattern: cccc00010101nnnn0000iiiiitt0mmmm,
//     rule: Cmp_Rule_36_A1_P82}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case40_TestCase40) {
  Binary2RegisterImmedShiftedTestTester_Case40 baseline_tester;
  NamedDontCareInst_Cmp_Rule_36_A1_P82 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010101nnnn0000iiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest => DontCareInst {'constraints': ,
//     'pattern': 'cccc00110101nnnn0000iiiiiiiiiiii',
//     'rule': 'Cmp_Rule_35_A1_P80'}
//
// Representative case:
// op(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest => DontCareInst {constraints: ,
//     pattern: cccc00110101nnnn0000iiiiiiiiiiii,
//     rule: Cmp_Rule_35_A1_P80}
TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_Case41_TestCase41) {
  BinaryRegisterImmediateTestTester_Case41 baseline_tester;
  NamedDontCareInst_Cmp_Rule_35_A1_P80 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110101nnnn0000iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=10101 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc00010101nnnn0000ssss0tt1mmmm',
//     'rule': 'Cmp_Rule_37_A1_P84',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: cccc00010101nnnn0000ssss0tt1mmmm,
//     rule: Cmp_Rule_37_A1_P84,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case42_TestCase42) {
  Binary3RegisterShiftedTestTester_Case42 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Cmp_Rule_37_A1_P84 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010101nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest => DontCareInst {'constraints': ,
//     'pattern': 'cccc00010111nnnn0000iiiiitt0mmmm',
//     'rule': 'Cmn_Rule_33_A1_P76'}
//
// Representative case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary2RegisterImmedShiftedTest => DontCareInst {constraints: ,
//     pattern: cccc00010111nnnn0000iiiiitt0mmmm,
//     rule: Cmn_Rule_33_A1_P76}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmedShiftedTestTester_Case43_TestCase43) {
  Binary2RegisterImmedShiftedTestTester_Case43 baseline_tester;
  NamedDontCareInst_Cmn_Rule_33_A1_P76 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010111nnnn0000iiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest => DontCareInst {'constraints': ,
//     'pattern': 'cccc00110111nnnn0000iiiiiiiiiiii',
//     'rule': 'Cmn_Rule_32_A1_P74'}
//
// Representative case:
// op(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = BinaryRegisterImmediateTest => DontCareInst {constraints: ,
//     pattern: cccc00110111nnnn0000iiiiiiiiiiii,
//     rule: Cmn_Rule_32_A1_P74}
TEST_F(Arm32DecoderStateTests,
       BinaryRegisterImmediateTestTester_Case44_TestCase44) {
  BinaryRegisterImmediateTestTester_Case44 baseline_tester;
  NamedDontCareInst_Cmn_Rule_32_A1_P74 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110111nnnn0000iiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=10111 & inst(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc00010111nnnn0000ssss0tt1mmmm',
//     'rule': 'Cmn_Rule_34_A1_P78',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = Binary3RegisterShiftedTest => DontCareInstRnRsRmNotPc {constraints: ,
//     pattern: cccc00010111nnnn0000ssss0tt1mmmm,
//     rule: Cmn_Rule_34_A1_P78,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterShiftedTestTester_Case45_TestCase45) {
  Binary3RegisterShiftedTestTester_Case45 baseline_tester;
  NamedDontCareInstRnRsRmNotPc_Cmn_Rule_34_A1_P78 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010111nnnn0000ssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=11000 & inst(7:5)=000 & inst(15:12)=~1111
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {'constraints': ,
//     'pattern': 'cccc01111000ddddaaaammmm0001nnnn',
//     'rule': 'Usada8_Rule_254_A1_P502',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=~1111
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: cccc01111000ddddaaaammmm0001nnnn,
//     rule: Usada8_Rule_254_A1_P502,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case46_TestCase46) {
  Binary4RegisterDualOpTester_Case46 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Usada8_Rule_254_A1_P502 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01111000ddddaaaammmm0001nnnn");
}

// Neutral case:
// inst(24:20)=11000 & inst(7:5)=000 & inst(15:12)=1111
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc01111000dddd1111mmmm0001nnnn',
//     'rule': 'Usad8_Rule_253_A1_P500',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=11000 & op2(7:5)=000 & Rd(15:12)=1111
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc01111000dddd1111mmmm0001nnnn,
//     rule: Usad8_Rule_253_A1_P500,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case47_TestCase47) {
  Binary3RegisterOpAltATester_Case47 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Usad8_Rule_253_A1_P500 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01111000dddd1111mmmm0001nnnn");
}

// Neutral case:
// inst(24:20)=11111 & inst(7:5)=111
//    = Roadblock => Roadblock {'constraints': ,
//     'pattern': 'cccc01111111iiiiiiiiiiii1111iiii',
//     'rule': 'Udf_Rule_A1'}
//
// Representaive case:
// op1(24:20)=11111 & op2(7:5)=111
//    = Roadblock => Roadblock {constraints: ,
//     pattern: cccc01111111iiiiiiiiiiii1111iiii,
//     rule: Udf_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       RoadblockTester_Case48_TestCase48) {
  RoadblockTester_Case48 tester;
  tester.Test("cccc01111111iiiiiiiiiiii1111iiii");
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
       StoreRegisterListTester_Case49_TestCase49) {
  StoreRegisterListTester_Case49 tester;
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
       LoadRegisterListTester_Case50_TestCase50) {
  LoadRegisterListTester_Case50 baseline_tester;
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
       StoreRegisterListTester_Case51_TestCase51) {
  StoreRegisterListTester_Case51 tester;
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
       LoadRegisterListTester_Case52_TestCase52) {
  LoadRegisterListTester_Case52 baseline_tester;
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
       StoreRegisterListTester_Case53_TestCase53) {
  StoreRegisterListTester_Case53 tester;
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
       LoadRegisterListTester_Case54_TestCase54) {
  LoadRegisterListTester_Case54 baseline_tester;
  NamedLoadMultiple_Ldmib_Ldmed_Rule_56_A1_P116 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100110w1nnnnrrrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(26:20)=100xx11
//    = UnpredictableUncondDecoder => Unpredictable {'constraints': ,
//     'pattern': '11110100xx11xxxxxxxxxxxxxxxxxxxx',
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=100xx11
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: 11110100xx11xxxxxxxxxxxxxxxxxxxx,
//     rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case55_TestCase55) {
  UnpredictableUncondDecoderTester_Case55 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110100xx11xxxxxxxxxxxxxxxxxxxx");
}

// Neutral case:
// inst(27:20)=100xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxx0000101000000000
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '1111100pu0w1nnnn0000101000000000',
//     'rule': 'Rfe_Rule_B6_1_10_A1_B6_16'}
//
// Representative case:
// op1(27:20)=100xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000101000000000
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 1111100pu0w1nnnn0000101000000000,
//     rule: Rfe_Rule_B6_1_10_A1_B6_16}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case56_TestCase56) {
  ForbiddenUncondDecoderTester_Case56 baseline_tester;
  NamedForbidden_Rfe_Rule_B6_1_10_A1_B6_16 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111100pu0w1nnnn0000101000000000");
}

// Neutral case:
// inst(27:20)=100xx1x0 & inst(31:0)=xxxxxxxxxxxx110100000101000xxxxx
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '1111100pu1w0110100000101000iiiii',
//     'rule': 'Srs_Rule_B6_1_10_A1_B6_20'}
//
// Representative case:
// op1(27:20)=100xx1x0 & $pattern(31:0)=xxxxxxxxxxxx110100000101000xxxxx
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 1111100pu1w0110100000101000iiiii,
//     rule: Srs_Rule_B6_1_10_A1_B6_20}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case57_TestCase57) {
  ForbiddenUncondDecoderTester_Case57 baseline_tester;
  NamedForbidden_Srs_Rule_B6_1_10_A1_B6_20 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111100pu1w0110100000101000iiiii");
}

// Neutral case:
// inst(27:20)=1110xxx0 & inst(4)=1
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '11111110iii0iiiittttiiiiiii1iiii',
//     'rule': 'Mcr2_Rule_92_A2_P186'}
//
// Representative case:
// op1(27:20)=1110xxx0 & op(4)=1
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 11111110iii0iiiittttiiiiiii1iiii,
//     rule: Mcr2_Rule_92_A2_P186}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case58_TestCase58) {
  ForbiddenUncondDecoderTester_Case58 baseline_tester;
  NamedForbidden_Mcr2_Rule_92_A2_P186 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11111110iii0iiiittttiiiiiii1iiii");
}

// Neutral case:
// inst(27:20)=1110xxx1 & inst(4)=1
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '11111110iii1iiiittttiiiiiii1iiii',
//     'rule': 'Mrc2_Rule_100_A2_P202'}
//
// Representative case:
// op1(27:20)=1110xxx1 & op(4)=1
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 11111110iii1iiiittttiiiiiii1iiii,
//     rule: Mrc2_Rule_100_A2_P202}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case59_TestCase59) {
  ForbiddenUncondDecoderTester_Case59 baseline_tester;
  NamedForbidden_Mrc2_Rule_100_A2_P202 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11111110iii1iiiittttiiiiiii1iiii");
}

// Neutral case:
// inst(19:16)=0000 & inst(7:6)=01
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d110000dddd101s01m0mmmm',
//     'rule': 'Vmov_Rule_327_A2_P642'}
//
// Representative case:
// opc2(19:16)=0000 & opc3(7:6)=01
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d110000dddd101s01m0mmmm,
//     rule: Vmov_Rule_327_A2_P642}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case60_TestCase60) {
  CondVfpOpTester_Case60 baseline_tester;
  NamedVfpOp_Vmov_Rule_327_A2_P642 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110000dddd101s01m0mmmm");
}

// Neutral case:
// inst(19:16)=0000 & inst(7:6)=11
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d110000dddd101s11m0mmmm',
//     'rule': 'Vabs_Rule_269_A2_P532'}
//
// Representative case:
// opc2(19:16)=0000 & opc3(7:6)=11
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d110000dddd101s11m0mmmm,
//     rule: Vabs_Rule_269_A2_P532}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case61_TestCase61) {
  CondVfpOpTester_Case61 baseline_tester;
  NamedVfpOp_Vabs_Rule_269_A2_P532 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110000dddd101s11m0mmmm");
}

// Neutral case:
// inst(19:16)=0001 & inst(7:6)=01
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d110001dddd101s01m0mmmm',
//     'rule': 'Vneg_Rule_342_A2_P672'}
//
// Representative case:
// opc2(19:16)=0001 & opc3(7:6)=01
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d110001dddd101s01m0mmmm,
//     rule: Vneg_Rule_342_A2_P672}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case62_TestCase62) {
  CondVfpOpTester_Case62 baseline_tester;
  NamedVfpOp_Vneg_Rule_342_A2_P672 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110001dddd101s01m0mmmm");
}

// Neutral case:
// inst(19:16)=0001 & inst(7:6)=11
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d110001dddd101s11m0mmmm',
//     'rule': 'Vsqrt_Rule_388_A1_P762'}
//
// Representative case:
// opc2(19:16)=0001 & opc3(7:6)=11
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d110001dddd101s11m0mmmm,
//     rule: Vsqrt_Rule_388_A1_P762}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case63_TestCase63) {
  CondVfpOpTester_Case63 baseline_tester;
  NamedVfpOp_Vsqrt_Rule_388_A1_P762 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110001dddd101s11m0mmmm");
}

// Neutral case:
// inst(19:16)=0100 & inst(7:6)=x1
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d110100dddd101se1m0mmmm',
//     'rule': 'Vcmp_Vcmpe_Rule_A1'}
//
// Representative case:
// opc2(19:16)=0100 & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d110100dddd101se1m0mmmm,
//     rule: Vcmp_Vcmpe_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case64_TestCase64) {
  CondVfpOpTester_Case64 baseline_tester;
  NamedVfpOp_Vcmp_Vcmpe_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110100dddd101se1m0mmmm");
}

// Neutral case:
// inst(19:16)=0101 & inst(7:6)=x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d110101dddd101se1000000',
//     'rule': 'Vcmp_Vcmpe_Rule_A2'}
//
// Representative case:
// opc2(19:16)=0101 & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d110101dddd101se1000000,
//     rule: Vcmp_Vcmpe_Rule_A2}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case65_TestCase65) {
  CondVfpOpTester_Case65 baseline_tester;
  NamedVfpOp_Vcmp_Vcmpe_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110101dddd101se1000000");
}

// Neutral case:
// inst(19:16)=0111 & inst(7:6)=11
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d110111dddd101s11m0mmmm',
//     'rule': 'Vcvt_Rule_298_A1_P584'}
//
// Representative case:
// opc2(19:16)=0111 & opc3(7:6)=11
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d110111dddd101s11m0mmmm,
//     rule: Vcvt_Rule_298_A1_P584}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case66_TestCase66) {
  CondVfpOpTester_Case66 baseline_tester;
  NamedVfpOp_Vcvt_Rule_298_A1_P584 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110111dddd101s11m0mmmm");
}

// Neutral case:
// inst(19:16)=1000 & inst(7:6)=x1
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d111000dddd101sp1m0mmmm',
//     'rule': 'Vcvt_Vcvtr_Rule_295_A1_P578'}
//
// Representative case:
// opc2(19:16)=1000 & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d111000dddd101sp1m0mmmm,
//     rule: Vcvt_Vcvtr_Rule_295_A1_P578}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case67_TestCase67) {
  CondVfpOpTester_Case67 baseline_tester;
  NamedVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d111000dddd101sp1m0mmmm");
}

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
       Binary4RegisterDualResultTester_Case68_TestCase68) {
  Binary4RegisterDualResultTester_Case68 baseline_tester;
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
       UndefinedCondDecoderTester_Case69_TestCase69) {
  UndefinedCondDecoderTester_Case69 baseline_tester;
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
       Binary4RegisterDualOpTester_Case70_TestCase70) {
  Binary4RegisterDualOpTester_Case70 baseline_tester;
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
       UndefinedCondDecoderTester_Case71_TestCase71) {
  UndefinedCondDecoderTester_Case71 baseline_tester;
  NamedUndefined_Undefined_A5_2_5_0111 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00000111xxxxxxxxxxxx1001xxxx");
}

// Neutral case:
// inst(23:20)=1000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp => StoreBasedMemoryRtBits0To3 {'constraints': ,
//     'pattern': 'cccc00011000nnnndddd11111001tttt',
//     'rule': 'Strex_Rule_202_A1_P400'}
//
// Representative case:
// op(23:20)=1000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp => StoreBasedMemoryRtBits0To3 {constraints: ,
//     pattern: cccc00011000nnnndddd11111001tttt,
//     rule: Strex_Rule_202_A1_P400}
TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterOpTester_Case72_TestCase72) {
  StoreExclusive3RegisterOpTester_Case72 baseline_tester;
  NamedStoreBasedMemoryRtBits0To3_Strex_Rule_202_A1_P400 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011000nnnndddd11111001tttt");
}

// Neutral case:
// inst(23:20)=1001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterOp => LoadBasedMemory {'constraints': ,
//     'pattern': 'cccc00011001nnnntttt111110011111',
//     'rule': 'Ldrex_Rule_69_A1_P142'}
//
// Representative case:
// op(23:20)=1001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterOp => LoadBasedMemory {constraints: ,
//     pattern: cccc00011001nnnntttt111110011111,
//     rule: Ldrex_Rule_69_A1_P142}
TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterOpTester_Case73_TestCase73) {
  LoadExclusive2RegisterOpTester_Case73 baseline_tester;
  NamedLoadBasedMemory_Ldrex_Rule_69_A1_P142 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011001nnnntttt111110011111");
}

// Neutral case:
// inst(23:20)=1010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterDoubleOp => StoreBasedMemoryDoubleRtBits0To3 {'constraints': ,
//     'pattern': 'cccc00011010nnnndddd11111001tttt',
//     'rule': 'Strexd_Rule_204_A1_P404'}
//
// Representative case:
// op(23:20)=1010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterDoubleOp => StoreBasedMemoryDoubleRtBits0To3 {constraints: ,
//     pattern: cccc00011010nnnndddd11111001tttt,
//     rule: Strexd_Rule_204_A1_P404}
TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterDoubleOpTester_Case74_TestCase74) {
  StoreExclusive3RegisterDoubleOpTester_Case74 baseline_tester;
  NamedStoreBasedMemoryDoubleRtBits0To3_Strexd_Rule_204_A1_P404 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011010nnnndddd11111001tttt");
}

// Neutral case:
// inst(23:20)=1011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterDoubleOp => LoadBasedMemoryDouble {'constraints': ,
//     'pattern': 'cccc00011011nnnntttt111110011111',
//     'rule': 'Ldrexd_Rule_71_A1_P146'}
//
// Representative case:
// op(23:20)=1011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterDoubleOp => LoadBasedMemoryDouble {constraints: ,
//     pattern: cccc00011011nnnntttt111110011111,
//     rule: Ldrexd_Rule_71_A1_P146}
TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterDoubleOpTester_Case75_TestCase75) {
  LoadExclusive2RegisterDoubleOpTester_Case75 baseline_tester;
  NamedLoadBasedMemoryDouble_Ldrexd_Rule_71_A1_P146 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011011nnnntttt111110011111");
}

// Neutral case:
// inst(23:20)=1100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp => StoreBasedMemoryRtBits0To3 {'constraints': ,
//     'pattern': 'cccc00011100nnnndddd11111001tttt',
//     'rule': 'Strexb_Rule_203_A1_P402'}
//
// Representative case:
// op(23:20)=1100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp => StoreBasedMemoryRtBits0To3 {constraints: ,
//     pattern: cccc00011100nnnndddd11111001tttt,
//     rule: Strexb_Rule_203_A1_P402}
TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterOpTester_Case76_TestCase76) {
  StoreExclusive3RegisterOpTester_Case76 baseline_tester;
  NamedStoreBasedMemoryRtBits0To3_Strexb_Rule_203_A1_P402 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011100nnnndddd11111001tttt");
}

// Neutral case:
// inst(23:20)=1101 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterOp => LoadBasedMemory {'constraints': ,
//     'pattern': 'cccc00011101nnnntttt111110011111',
//     'rule': 'Ldrexb_Rule_70_A1_P144'}
//
// Representative case:
// op(23:20)=1101 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterOp => LoadBasedMemory {constraints: ,
//     pattern: cccc00011101nnnntttt111110011111,
//     rule: Ldrexb_Rule_70_A1_P144}
TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterOpTester_Case77_TestCase77) {
  LoadExclusive2RegisterOpTester_Case77 baseline_tester;
  NamedLoadBasedMemory_Ldrexb_Rule_70_A1_P144 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011101nnnntttt111110011111");
}

// Neutral case:
// inst(23:20)=1110 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp => StoreBasedMemoryRtBits0To3 {'constraints': ,
//     'pattern': 'cccc00011110nnnndddd11111001tttt',
//     'rule': 'Strexh_Rule_205_A1_P406'}
//
// Representative case:
// op(23:20)=1110 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = StoreExclusive3RegisterOp => StoreBasedMemoryRtBits0To3 {constraints: ,
//     pattern: cccc00011110nnnndddd11111001tttt,
//     rule: Strexh_Rule_205_A1_P406}
TEST_F(Arm32DecoderStateTests,
       StoreExclusive3RegisterOpTester_Case78_TestCase78) {
  StoreExclusive3RegisterOpTester_Case78 baseline_tester;
  NamedStoreBasedMemoryRtBits0To3_Strexh_Rule_205_A1_P406 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011110nnnndddd11111001tttt");
}

// Neutral case:
// inst(23:20)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterOp => LoadBasedMemory {'constraints': ,
//     'pattern': 'cccc00011111nnnntttt111110011111',
//     'rule': 'Ldrexh_Rule_72_A1_P148'}
//
// Representative case:
// op(23:20)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxx1111
//    = LoadExclusive2RegisterOp => LoadBasedMemory {constraints: ,
//     pattern: cccc00011111nnnntttt111110011111,
//     rule: Ldrexh_Rule_72_A1_P148}
TEST_F(Arm32DecoderStateTests,
       LoadExclusive2RegisterOpTester_Case79_TestCase79) {
  LoadExclusive2RegisterOpTester_Case79 baseline_tester;
  NamedLoadBasedMemory_Ldrexh_Rule_72_A1_P148 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00011111nnnntttt111110011111");
}

// Neutral case:
// inst(24:20)=01x00
//    = StoreVectorRegisterList => StoreVectorRegisterList {'constraints': ,
//     'pattern': 'cccc11001d00nnnndddd101xiiiiiiii',
//     'rule': 'Vstm_Rule_399_A1_A2_P784'}
//
// Representaive case:
// opcode(24:20)=01x00
//    = StoreVectorRegisterList => StoreVectorRegisterList {constraints: ,
//     pattern: cccc11001d00nnnndddd101xiiiiiiii,
//     rule: Vstm_Rule_399_A1_A2_P784}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case80_TestCase80) {
  StoreVectorRegisterListTester_Case80 tester;
  tester.Test("cccc11001d00nnnndddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=01x01
//    = LoadVectorRegisterList => LoadVectorRegisterList {'constraints': ,
//     'pattern': 'cccc11001d01nnnndddd101xiiiiiiii',
//     'rule': 'Vldm_Rule_319_A1_A2_P626'}
//
// Representaive case:
// opcode(24:20)=01x01
//    = LoadVectorRegisterList => LoadVectorRegisterList {constraints: ,
//     pattern: cccc11001d01nnnndddd101xiiiiiiii,
//     rule: Vldm_Rule_319_A1_A2_P626}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case81_TestCase81) {
  LoadVectorRegisterListTester_Case81 tester;
  tester.Test("cccc11001d01nnnndddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=01x10
//    = StoreVectorRegisterList => StoreVectorRegisterList {'constraints': ,
//     'pattern': 'cccc11001d10nnnndddd101xiiiiiiii',
//     'rule': 'Vstm_Rule_399_A1_A2_P784'}
//
// Representaive case:
// opcode(24:20)=01x10
//    = StoreVectorRegisterList => StoreVectorRegisterList {constraints: ,
//     pattern: cccc11001d10nnnndddd101xiiiiiiii,
//     rule: Vstm_Rule_399_A1_A2_P784}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case82_TestCase82) {
  StoreVectorRegisterListTester_Case82 tester;
  tester.Test("cccc11001d10nnnndddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=~1101
//    = LoadVectorRegisterList => LoadVectorRegisterList {'constraints': ,
//     'pattern': 'cccc11001d11nnnndddd101xiiiiiiii',
//     'rule': 'Vldm_Rule_319_A1_A2_P626',
//     'safety': ["'NotRnIsSp'"]}
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=~1101
//    = LoadVectorRegisterList => LoadVectorRegisterList {constraints: ,
//     pattern: cccc11001d11nnnndddd101xiiiiiiii,
//     rule: Vldm_Rule_319_A1_A2_P626,
//     safety: ['NotRnIsSp']}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case83_TestCase83) {
  LoadVectorRegisterListTester_Case83 tester;
  tester.Test("cccc11001d11nnnndddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=01x11 & inst(19:16)=1101
//    = LoadVectorRegisterList => LoadVectorRegisterList {'constraints': ,
//     'pattern': 'cccc11001d111101dddd101xiiiiiiii',
//     'rule': 'Vpop_Rule_354_A1_A2_P694'}
//
// Representaive case:
// opcode(24:20)=01x11 & Rn(19:16)=1101
//    = LoadVectorRegisterList => LoadVectorRegisterList {constraints: ,
//     pattern: cccc11001d111101dddd101xiiiiiiii,
//     rule: Vpop_Rule_354_A1_A2_P694}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case84_TestCase84) {
  LoadVectorRegisterListTester_Case84 tester;
  tester.Test("cccc11001d111101dddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=~1101
//    = StoreVectorRegisterList => StoreVectorRegisterList {'constraints': ,
//     'pattern': 'cccc11010d10nnnndddd101xiiiiiiii',
//     'rule': 'Vstm_Rule_399_A1_A2_P784',
//     'safety': ["'NotRnIsSp'"]}
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=~1101
//    = StoreVectorRegisterList => StoreVectorRegisterList {constraints: ,
//     pattern: cccc11010d10nnnndddd101xiiiiiiii,
//     rule: Vstm_Rule_399_A1_A2_P784,
//     safety: ['NotRnIsSp']}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case85_TestCase85) {
  StoreVectorRegisterListTester_Case85 tester;
  tester.Test("cccc11010d10nnnndddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=10x10 & inst(19:16)=1101
//    = StoreVectorRegisterList => StoreVectorRegisterList {'constraints': ,
//     'pattern': 'cccc11010d101101dddd101xiiiiiiii',
//     'rule': 'Vpush_355_A1_A2_P696'}
//
// Representaive case:
// opcode(24:20)=10x10 & Rn(19:16)=1101
//    = StoreVectorRegisterList => StoreVectorRegisterList {constraints: ,
//     pattern: cccc11010d101101dddd101xiiiiiiii,
//     rule: Vpush_355_A1_A2_P696}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterListTester_Case86_TestCase86) {
  StoreVectorRegisterListTester_Case86 tester;
  tester.Test("cccc11010d101101dddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=10x11
//    = LoadVectorRegisterList => LoadVectorRegisterList {'constraints': ,
//     'pattern': 'cccc11010d11nnnndddd101xiiiiiiii',
//     'rule': 'Vldm_Rule_318_A1_A2_P626'}
//
// Representaive case:
// opcode(24:20)=10x11
//    = LoadVectorRegisterList => LoadVectorRegisterList {constraints: ,
//     pattern: cccc11010d11nnnndddd101xiiiiiiii,
//     rule: Vldm_Rule_318_A1_A2_P626}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterListTester_Case87_TestCase87) {
  LoadVectorRegisterListTester_Case87 tester;
  tester.Test("cccc11010d11nnnndddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0000000unnnnddddiiiiitt0mmmm',
//     'rule': 'And_Rule_7_A1_P36',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0000x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0000000unnnnddddiiiiitt0mmmm,
//     rule: And_Rule_7_A1_P36,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case88_TestCase88) {
  Binary3RegisterImmedShiftedOpTester_Case88 baseline_tester;
  NamedDefs12To15_And_Rule_7_A1_P36 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000unnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0000x
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010000snnnnddddiiiiiiiiiiii',
//     'rule': 'And_Rule_11_A1_P34',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0000x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010000snnnnddddiiiiiiiiiiii,
//     rule: And_Rule_11_A1_P34,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case89_TestCase89) {
  Binary2RegisterImmediateOpTester_Case89 baseline_tester;
  NamedDefs12To15_And_Rule_11_A1_P34 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010000snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0000x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000000snnnnddddssss0tt1mmmm',
//     'rule': 'And_Rule_13_A1_P38',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0000x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000000snnnnddddssss0tt1mmmm,
//     rule: And_Rule_13_A1_P38,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case90_TestCase90) {
  Binary4RegisterShiftedOpTester_Case90 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_And_Rule_13_A1_P38 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0001x
//    = Binary3RegisterImmedShiftedOp => Defs12To15CondsDontCare {'constraints': ,
//     'pattern': 'cccc0000001unnnnddddiiiiitt0mmmm',
//     'rule': 'Eor_Rule_45_A1_P96',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0001x
//    = Binary3RegisterImmedShiftedOp => Defs12To15CondsDontCare {constraints: ,
//     pattern: cccc0000001unnnnddddiiiiitt0mmmm,
//     rule: Eor_Rule_45_A1_P96,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case91_TestCase91) {
  Binary3RegisterImmedShiftedOpTester_Case91 baseline_tester;
  NamedDefs12To15CondsDontCare_Eor_Rule_45_A1_P96 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001unnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0001x
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010001snnnnddddiiiiiiiiiiii',
//     'rule': 'Eor_Rule_44_A1_P94',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0001x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010001snnnnddddiiiiiiiiiiii,
//     rule: Eor_Rule_44_A1_P94,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case92_TestCase92) {
  Binary2RegisterImmediateOpTester_Case92 baseline_tester;
  NamedDefs12To15_Eor_Rule_44_A1_P94 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010001snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0001x
//    = Binary4RegisterShiftedOp => Defs12To15CondsDontCareRdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000001snnnnddddssss0tt1mmmm',
//     'rule': 'Eor_Rule_46_A1_P98',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0001x
//    = Binary4RegisterShiftedOp => Defs12To15CondsDontCareRdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000001snnnnddddssss0tt1mmmm,
//     rule: Eor_Rule_46_A1_P98,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case93_TestCase93) {
  Binary4RegisterShiftedOpTester_Case93 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnRsRmNotPc_Eor_Rule_46_A1_P98 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0010x & inst(19:16)=~1111
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010010snnnnddddiiiiiiiiiiii',
//     'rule': 'Sub_Rule_212_A1_P420',
//     'safety': ["'NeitherRdIsPcAndSNorRnIsPcAndNotS'"]}
//
// Representative case:
// op(24:20)=0010x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010010snnnnddddiiiiiiiiiiii,
//     rule: Sub_Rule_212_A1_P420,
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case94_TestCase94) {
  Binary2RegisterImmediateOpTester_Case94 baseline_tester;
  NamedDefs12To15_Sub_Rule_212_A1_P420 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010010snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0010x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0000010unnnnddddiiiiitt0mmmm',
//     'rule': 'Sub_Rule_213_A1_P422',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0010x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0000010unnnnddddiiiiitt0mmmm,
//     rule: Sub_Rule_213_A1_P422,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case95_TestCase95) {
  Binary3RegisterImmedShiftedOpTester_Case95 baseline_tester;
  NamedDefs12To15_Sub_Rule_213_A1_P422 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000010unnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0010x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000010snnnnddddssss0tt1mmmm',
//     'rule': 'Sub_Rule_214_A1_P424',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0010x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000010snnnnddddssss0tt1mmmm,
//     rule: Sub_Rule_214_A1_P424,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case96_TestCase96) {
  Binary4RegisterShiftedOpTester_Case96 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Sub_Rule_214_A1_P424 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000010snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0011x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0000011unnnnddddiiiiitt0mmmm',
//     'rule': 'Rsb_Rule_143_P286',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0011x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0000011unnnnddddiiiiitt0mmmm,
//     rule: Rsb_Rule_143_P286,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case97_TestCase97) {
  Binary3RegisterImmedShiftedOpTester_Case97 baseline_tester;
  NamedDefs12To15_Rsb_Rule_143_P286 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000011unnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0011x
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010011snnnnddddiiiiiiiiiiii',
//     'rule': 'Rsb_Rule_142_A1_P284',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0011x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010011snnnnddddiiiiiiiiiiii,
//     rule: Rsb_Rule_142_A1_P284,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case98_TestCase98) {
  Binary2RegisterImmediateOpTester_Case98 baseline_tester;
  NamedDefs12To15_Rsb_Rule_142_A1_P284 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010011snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0011x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000011snnnnddddssss0tt1mmmm',
//     'rule': 'Rsb_Rule_144_A1_P288',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0011x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000011snnnnddddssss0tt1mmmm,
//     rule: Rsb_Rule_144_A1_P288,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case99_TestCase99) {
  Binary4RegisterShiftedOpTester_Case99 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Rsb_Rule_144_A1_P288 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000011snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0100x & inst(19:16)=~1111
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010100snnnnddddiiiiiiiiiiii',
//     'rule': 'Add_Rule_5_A1_P22',
//     'safety': ["'NeitherRdIsPcAndSNorRnIsPcAndNotS'"]}
//
// Representative case:
// op(24:20)=0100x & Rn(19:16)=~1111
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010100snnnnddddiiiiiiiiiiii,
//     rule: Add_Rule_5_A1_P22,
//     safety: ['NeitherRdIsPcAndSNorRnIsPcAndNotS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case100_TestCase100) {
  Binary2RegisterImmediateOpTester_Case100 baseline_tester;
  NamedDefs12To15_Add_Rule_5_A1_P22 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010100snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0100x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0000100unnnnddddiiiiitt0mmmm',
//     'rule': 'Add_Rule_6_A1_P24',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0100x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0000100unnnnddddiiiiitt0mmmm,
//     rule: Add_Rule_6_A1_P24,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case101_TestCase101) {
  Binary3RegisterImmedShiftedOpTester_Case101 baseline_tester;
  NamedDefs12To15_Add_Rule_6_A1_P24 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100unnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0100x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000100snnnnddddssss0tt1mmmm',
//     'rule': 'Add_Rule_7_A1_P26',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0100x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000100snnnnddddssss0tt1mmmm,
//     rule: Add_Rule_7_A1_P26,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case102_TestCase102) {
  Binary4RegisterShiftedOpTester_Case102 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Add_Rule_7_A1_P26 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0000101unnnnddddiiiiitt0mmmm',
//     'rule': 'Adc_Rule_2_A1_P16',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0101x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0000101unnnnddddiiiiitt0mmmm,
//     rule: Adc_Rule_2_A1_P16,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case103_TestCase103) {
  Binary3RegisterImmedShiftedOpTester_Case103 baseline_tester;
  NamedDefs12To15_Adc_Rule_2_A1_P16 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101unnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0101x
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010101snnnnddddiiiiiiiiiiii',
//     'rule': 'Adc_Rule_6_A1_P14',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0101x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010101snnnnddddiiiiiiiiiiii,
//     rule: Adc_Rule_6_A1_P14,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case104_TestCase104) {
  Binary2RegisterImmediateOpTester_Case104 baseline_tester;
  NamedDefs12To15_Adc_Rule_6_A1_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010101snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0101x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000101snnnnddddssss0tt1mmmm',
//     'rule': 'Adc_Rule_3_A1_P18',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0101x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000101snnnnddddssss0tt1mmmm,
//     rule: Adc_Rule_3_A1_P18,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case105_TestCase105) {
  Binary4RegisterShiftedOpTester_Case105 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Adc_Rule_3_A1_P18 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0110x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0000110unnnnddddiiiiitt0mmmm',
//     'rule': 'Sbc_Rule_152_A1_P304',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0110x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0000110unnnnddddiiiiitt0mmmm,
//     rule: Sbc_Rule_152_A1_P304,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case106_TestCase106) {
  Binary3RegisterImmedShiftedOpTester_Case106 baseline_tester;
  NamedDefs12To15_Sbc_Rule_152_A1_P304 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110unnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0110x
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010110snnnnddddiiiiiiiiiiii',
//     'rule': 'Sbc_Rule_151_A1_P302',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0110x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010110snnnnddddiiiiiiiiiiii,
//     rule: Sbc_Rule_151_A1_P302,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case107_TestCase107) {
  Binary2RegisterImmediateOpTester_Case107 baseline_tester;
  NamedDefs12To15_Sbc_Rule_151_A1_P302 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010110snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0110x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000110snnnnddddssss0tt1mmmm',
//     'rule': 'Sbc_Rule_153_A1_P306',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0110x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000110snnnnddddssss0tt1mmmm,
//     rule: Sbc_Rule_153_A1_P306,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case108_TestCase108) {
  Binary4RegisterShiftedOpTester_Case108 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Sbc_Rule_153_A1_P306 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0000111unnnnddddiiiiitt0mmmm',
//     'rule': 'Rsc_Rule_146_A1_P292',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=0111x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0000111unnnnddddiiiiitt0mmmm,
//     rule: Rsc_Rule_146_A1_P292,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case109_TestCase109) {
  Binary3RegisterImmedShiftedOpTester_Case109 baseline_tester;
  NamedDefs12To15_Rsc_Rule_146_A1_P292 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111unnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=0111x
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0010111snnnnddddiiiiiiiiiiii',
//     'rule': 'Rsc_Rule_145_A1_P290',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=0111x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0010111snnnnddddiiiiiiiiiiii,
//     rule: Rsc_Rule_145_A1_P290,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case110_TestCase110) {
  Binary2RegisterImmediateOpTester_Case110 baseline_tester;
  NamedDefs12To15_Rsc_Rule_145_A1_P290 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0010111snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=0111x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0000111snnnnddddssss0tt1mmmm',
//     'rule': 'Rsc_Rule_147_A1_P294',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=0111x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0000111snnnnddddssss0tt1mmmm,
//     rule: Rsc_Rule_147_A1_P294,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case111_TestCase111) {
  Binary4RegisterShiftedOpTester_Case111 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Rsc_Rule_147_A1_P294 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:21)=1001 & inst(19:16)=1101 & inst(11:0)=000000000100
//    = Store2RegisterImm12OpRnNotRtOnWriteback => Store2RegisterImm12OpRnNotRtOnWriteback {'constraints': ,
//     'pattern': 'cccc010100101101tttt000000000100',
//     'rule': 'Push_Rule_123_A2_P248'}
//
// Representaive case:
// Flags(24:21)=1001 & Rn(19:16)=1101 & Imm12(11:0)=000000000100
//    = Store2RegisterImm12OpRnNotRtOnWriteback => Store2RegisterImm12OpRnNotRtOnWriteback {constraints: ,
//     pattern: cccc010100101101tttt000000000100,
//     rule: Push_Rule_123_A2_P248}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm12OpRnNotRtOnWritebackTester_Case112_TestCase112) {
  Store2RegisterImm12OpRnNotRtOnWritebackTester_Case112 tester;
  tester.Test("cccc010100101101tttt000000000100");
}

// Neutral case:
// inst(24:20)=1100x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001100unnnnddddiiiiitt0mmmm',
//     'rule': 'Orr_Rule_114_A1_P230',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1100x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0001100unnnnddddiiiiitt0mmmm,
//     rule: Orr_Rule_114_A1_P230,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case113_TestCase113) {
  Binary3RegisterImmedShiftedOpTester_Case113 baseline_tester;
  NamedDefs12To15_Orr_Rule_114_A1_P230 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001100unnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=1100x
//    = Binary2RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0011100snnnnddddiiiiiiiiiiii',
//     'rule': 'Orr_Rule_113_A1_P228',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1100x
//    = Binary2RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0011100snnnnddddiiiiiiiiiiii,
//     rule: Orr_Rule_113_A1_P228,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterImmediateOpTester_Case114_TestCase114) {
  Binary2RegisterImmediateOpTester_Case114 baseline_tester;
  NamedDefs12To15_Orr_Rule_113_A1_P228 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011100snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=1100x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0001100snnnnddddssss0tt1mmmm',
//     'rule': 'Orr_Rule_115_A1_P212',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1100x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0001100snnnnddddssss0tt1mmmm,
//     rule: Orr_Rule_115_A1_P212,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case115_TestCase115) {
  Binary4RegisterShiftedOpTester_Case115 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Orr_Rule_115_A1_P212 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001100snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001101u0000ddddiiiii000mmmm',
//     'rule': 'Lsl_Rule_88_A1_P178',
//     'safety': ["'NeitherImm5NotZeroNorNotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0001101u0000ddddiiiii000mmmm,
//     rule: Lsl_Rule_88_A1_P178,
//     safety: ['NeitherImm5NotZeroNorNotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case116_TestCase116) {
  Unary2RegisterImmedShiftedOpTester_Case116 baseline_tester;
  NamedDefs12To15_Lsl_Rule_88_A1_P178 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii000mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=~00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001101u0000ddddiiiii110mmmm',
//     'rule': 'Ror_Rule_139_A1_P278',
//     'safety': ["'NeitherImm5NotZeroNorNotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0001101u0000ddddiiiii110mmmm,
//     rule: Ror_Rule_139_A1_P278,
//     safety: ['NeitherImm5NotZeroNorNotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case117_TestCase117) {
  Unary2RegisterImmedShiftedOpTester_Case117 baseline_tester;
  NamedDefs12To15_Ror_Rule_139_A1_P278 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii110mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001101u0000dddd00000000mmmm',
//     'rule': 'Mov_Rule_97_A1_P196',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp => Defs12To15 {constraints: ,
//     pattern: cccc0001101u0000dddd00000000mmmm,
//     rule: Mov_Rule_97_A1_P196,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpTester_Case118_TestCase118) {
  Unary2RegisterOpTester_Case118 baseline_tester;
  NamedDefs12To15_Mov_Rule_97_A1_P196 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000dddd00000000mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(11:7)=00000 & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001101u0000dddd00000110mmmm',
//     'rule': 'Rrx_Rule_141_A1_P282',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterOp => Defs12To15 {constraints: ,
//     pattern: cccc0001101u0000dddd00000110mmmm,
//     rule: Rrx_Rule_141_A1_P282,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpTester_Case119_TestCase119) {
  Unary2RegisterOpTester_Case119 baseline_tester;
  NamedDefs12To15_Rrx_Rule_141_A1_P282 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000dddd00000110mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=00 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0001101s0000ddddmmmm0001nnnn',
//     'rule': 'Lsl_Rule_89_A1_P180',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: cccc0001101s0000ddddmmmm0001nnnn,
//     rule: Lsl_Rule_89_A1_P180,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case120_TestCase120) {
  Binary3RegisterOpTester_Case120 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Lsl_Rule_89_A1_P180 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0001nnnn");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0001101s0000ddddmmmm0011nnnn',
//     'rule': 'Lsr_Rule_91_A1_P184',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: cccc0001101s0000ddddmmmm0011nnnn,
//     rule: Lsr_Rule_91_A1_P184,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case121_TestCase121) {
  Binary3RegisterOpTester_Case121 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Lsr_Rule_91_A1_P184 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0011nnnn");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0001101s0000ddddmmmm0101nnnn',
//     'rule': 'Asr_Rule_15_A1_P42',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: cccc0001101s0000ddddmmmm0101nnnn,
//     rule: Asr_Rule_15_A1_P42,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case122_TestCase122) {
  Binary3RegisterOpTester_Case122 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Asr_Rule_15_A1_P42 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0101nnnn");
}

// Neutral case:
// inst(24:20)=1101x & inst(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract => Defs12To15CondsDontCareRdRnNotPcBitfieldExtract {'constraints': ,
//     'pattern': 'cccc0111101wwwwwddddlllll101nnnn',
//     'rule': 'Sbfx_Rule_154_A1_P308',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract => Defs12To15CondsDontCareRdRnNotPcBitfieldExtract {constraints: ,
//     pattern: cccc0111101wwwwwddddlllll101nnnn,
//     rule: Sbfx_Rule_154_A1_P308,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case123_TestCase123) {
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case123 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPcBitfieldExtract_Sbfx_Rule_154_A1_P308 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0111101wwwwwddddlllll101nnnn");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=11 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0001101s0000ddddmmmm0111nnnn',
//     'rule': 'Ror_Rule_140_A1_P280',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1101x & op2(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Binary3RegisterOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: cccc0001101s0000ddddmmmm0111nnnn,
//     rule: Ror_Rule_140_A1_P280,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpTester_Case124_TestCase124) {
  Binary3RegisterOpTester_Case124 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Ror_Rule_140_A1_P280 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddmmmm0111nnnn");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=01 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001101u0000ddddiiiii010mmmm',
//     'rule': 'Lsr_Rule_90_A1_P182',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1101x & op3(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0001101u0000ddddiiiii010mmmm,
//     rule: Lsr_Rule_90_A1_P182,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case125_TestCase125) {
  Unary2RegisterImmedShiftedOpTester_Case125 baseline_tester;
  NamedDefs12To15_Lsr_Rule_90_A1_P182 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii010mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(6:5)=10 & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001101u0000ddddiiiii100mmmm',
//     'rule': 'Asr_Rule_14_A1_P40',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1101x & op3(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0001101u0000ddddiiiii100mmmm,
//     rule: Asr_Rule_14_A1_P40,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case126_TestCase126) {
  Unary2RegisterImmedShiftedOpTester_Case126 baseline_tester;
  NamedDefs12To15_Asr_Rule_14_A1_P40 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101u0000ddddiiiii100mmmm");
}

// Neutral case:
// inst(24:20)=1101x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0011101s0000ddddiiiiiiiiiiii',
//     'rule': 'Mov_Rule_96_A1_P194',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1101x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0011101s0000ddddiiiiiiiiiiii,
//     rule: Mov_Rule_96_A1_P194,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Case127_TestCase127) {
  Unary1RegisterImmediateOpTester_Case127 baseline_tester;
  NamedDefs12To15_Mov_Rule_96_A1_P194 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011101s0000ddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=1110x & inst(7:5)=x00 & inst(3:0)=~1111
//    = Binary2RegisterBitRangeMsbGeLsb => Defs12To15CondsDontCareMsbGeLsb {'constraints': ,
//     'pattern': 'cccc0111110mmmmmddddlllll001nnnn',
//     'rule': 'Bfi_Rule_18_A1_P48',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=~1111
//    = Binary2RegisterBitRangeMsbGeLsb => Defs12To15CondsDontCareMsbGeLsb {constraints: ,
//     pattern: cccc0111110mmmmmddddlllll001nnnn,
//     rule: Bfi_Rule_18_A1_P48,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterBitRangeMsbGeLsbTester_Case128_TestCase128) {
  Binary2RegisterBitRangeMsbGeLsbTester_Case128 baseline_tester;
  NamedDefs12To15CondsDontCareMsbGeLsb_Bfi_Rule_18_A1_P48 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0111110mmmmmddddlllll001nnnn");
}

// Neutral case:
// inst(24:20)=1110x & inst(7:5)=x00 & inst(3:0)=1111
//    = Unary1RegisterBitRangeMsbGeLsb => Unary1RegisterBitRangeMsbGeLsb {'constraints': ,
//     'pattern': 'cccc0111110mmmmmddddlllll0011111',
//     'rule': 'Bfc_17_A1_P46',
//     'safety': ["'RegsNotPc'"]}
//
// Representaive case:
// op1(24:20)=1110x & op2(7:5)=x00 & Rn(3:0)=1111
//    = Unary1RegisterBitRangeMsbGeLsb => Unary1RegisterBitRangeMsbGeLsb {constraints: ,
//     pattern: cccc0111110mmmmmddddlllll0011111,
//     rule: Bfc_17_A1_P46,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterBitRangeMsbGeLsbTester_Case129_TestCase129) {
  Unary1RegisterBitRangeMsbGeLsbTester_Case129 tester;
  tester.Test("cccc0111110mmmmmddddlllll0011111");
}

// Neutral case:
// inst(24:20)=1110x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001110unnnnddddiiiiitt0mmmm',
//     'rule': 'Bic_Rule_20_A1_P52',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1110x
//    = Binary3RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0001110unnnnddddiiiiitt0mmmm,
//     rule: Bic_Rule_20_A1_P52,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpTester_Case130_TestCase130) {
  Binary3RegisterImmedShiftedOpTester_Case130 baseline_tester;
  NamedDefs12To15_Bic_Rule_20_A1_P52 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001110unnnnddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=1110x
//    = MaskedBinary2RegisterImmediateOp => MaskAddress {'constraints': ,
//     'pattern': 'cccc0011110snnnnddddiiiiiiiiiiii',
//     'rule': 'Bic_Rule_19_A1_P50',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1110x
//    = MaskedBinary2RegisterImmediateOp => MaskAddress {constraints: ,
//     pattern: cccc0011110snnnnddddiiiiiiiiiiii,
//     rule: Bic_Rule_19_A1_P50,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       MaskedBinary2RegisterImmediateOpTester_Case131_TestCase131) {
  MaskedBinary2RegisterImmediateOpTester_Case131 baseline_tester;
  NamedMaskAddress_Bic_Rule_19_A1_P50 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011110snnnnddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=1110x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {'constraints': ,
//     'pattern': 'cccc0001110snnnnddddssss0tt1mmmm',
//     'rule': 'Bic_Rule_21_A1_P54',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1110x
//    = Binary4RegisterShiftedOp => Defs12To15RdRnRsRmNotPc {constraints: ,
//     pattern: cccc0001110snnnnddddssss0tt1mmmm,
//     rule: Bic_Rule_21_A1_P54,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterShiftedOpTester_Case132_TestCase132) {
  Binary4RegisterShiftedOpTester_Case132 baseline_tester;
  NamedDefs12To15RdRnRsRmNotPc_Bic_Rule_21_A1_P54 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001110snnnnddddssss0tt1mmmm");
}

// Neutral case:
// inst(24:20)=1111x & inst(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract => Defs12To15CondsDontCareRdRnNotPcBitfieldExtract {'constraints': ,
//     'pattern': 'cccc0111111mmmmmddddlllll101nnnn',
//     'rule': 'Ubfx_Rule_236_A1_P466',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1111x & op2(7:5)=x10
//    = Binary2RegisterBitRangeNotRnIsPcBitfieldExtract => Defs12To15CondsDontCareRdRnNotPcBitfieldExtract {constraints: ,
//     pattern: cccc0111111mmmmmddddlllll101nnnn,
//     rule: Ubfx_Rule_236_A1_P466,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case133_TestCase133) {
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester_Case133 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPcBitfieldExtract_Ubfx_Rule_236_A1_P466 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0111111mmmmmddddlllll101nnnn");
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0001111u0000ddddiiiiitt0mmmm',
//     'rule': 'Mvn_Rule_107_A1_P216',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary2RegisterImmedShiftedOp => Defs12To15 {constraints: ,
//     pattern: cccc0001111u0000ddddiiiiitt0mmmm,
//     rule: Mvn_Rule_107_A1_P216,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpTester_Case134_TestCase134) {
  Unary2RegisterImmedShiftedOpTester_Case134 baseline_tester;
  NamedDefs12To15_Mvn_Rule_107_A1_P216 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001111u0000ddddiiiiitt0mmmm");
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc0011111s0000ddddiiiiiiiiiiii',
//     'rule': 'Mvn_Rule_106_A1_P214',
//     'safety': ["'NotRdIsPcAndS'"]}
//
// Representative case:
// op(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc0011111s0000ddddiiiiiiiiiiii,
//     rule: Mvn_Rule_106_A1_P214,
//     safety: ['NotRdIsPcAndS']}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Case135_TestCase135) {
  Unary1RegisterImmediateOpTester_Case135 baseline_tester;
  NamedDefs12To15_Mvn_Rule_106_A1_P214 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011111s0000ddddiiiiiiiiiiii");
}

// Neutral case:
// inst(24:20)=1111x & inst(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp => Defs12To15RdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc0001111s0000ddddssss0tt1mmmm',
//     'rule': 'Mvn_Rule_108_A1_P218',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = Unary3RegisterShiftedOp => Defs12To15RdRmRnNotPc {constraints: ,
//     pattern: cccc0001111s0000ddddssss0tt1mmmm,
//     rule: Mvn_Rule_108_A1_P218,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary3RegisterShiftedOpTester_Case136_TestCase136) {
  Unary3RegisterShiftedOpTester_Case136 baseline_tester;
  NamedDefs12To15RdRmRnNotPc_Mvn_Rule_108_A1_P218 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001111s0000ddddssss0tt1mmmm");
}

// Neutral case:
// inst(26:20)=11xxx11 & inst(7:4)=xxx0
//    = UnpredictableUncondDecoder => Unpredictable {'constraints': ,
//     'pattern': '1111011xxx11xxxxxxxxxxxxxxx0xxxx',
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=11xxx11 & op2(7:4)=xxx0
//    = UnpredictableUncondDecoder => Unpredictable {constraints: ,
//     pattern: 1111011xxx11xxxxxxxxxxxxxxx0xxxx,
//     rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case137_TestCase137) {
  UnpredictableUncondDecoderTester_Case137 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111011xxx11xxxxxxxxxxxxxxx0xxxx");
}

// Neutral case:
// inst(27:20)=110xxxx0 & inst(27:20)=~11000x00
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '1111110pudw0nnnniiiiiiiiiiiiiiii',
//     'rule': 'Stc2_Rule_188_A2_P372'}
//
// Representative case:
// op1(27:20)=110xxxx0 & op1_repeated(27:20)=~11000x00
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 1111110pudw0nnnniiiiiiiiiiiiiiii,
//     rule: Stc2_Rule_188_A2_P372}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case138_TestCase138) {
  ForbiddenUncondDecoderTester_Case138 baseline_tester;
  NamedForbidden_Stc2_Rule_188_A2_P372 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111110pudw0nnnniiiiiiiiiiiiiiii");
}

// Neutral case:
// inst(27:20)=110xxxx1 & inst(19:16)=~1111 & inst(27:20)=~11000x01
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '1111110pudw1nnnniiiiiiiiiiiiiiii',
//     'rule': 'Ldc2_Rule_51_A2_P106'}
//
// Representative case:
// op1(27:20)=110xxxx1 & Rn(19:16)=~1111 & op1_repeated(27:20)=~11000x01
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 1111110pudw1nnnniiiiiiiiiiiiiiii,
//     rule: Ldc2_Rule_51_A2_P106}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case139_TestCase139) {
  ForbiddenUncondDecoderTester_Case139 baseline_tester;
  NamedForbidden_Ldc2_Rule_51_A2_P106 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111110pudw1nnnniiiiiiiiiiiiiiii");
}

// Neutral case:
// inst(27:20)=110xxxx1 & inst(19:16)=1111 & inst(27:20)=~11000x01
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '1111110pudw11111iiiiiiiiiiiiiiii',
//     'rule': 'Ldc2_Rule_52_A2_P108'}
//
// Representative case:
// op1(27:20)=110xxxx1 & Rn(19:16)=1111 & op1_repeated(27:20)=~11000x01
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 1111110pudw11111iiiiiiiiiiiiiiii,
//     rule: Ldc2_Rule_52_A2_P108}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case140_TestCase140) {
  ForbiddenUncondDecoderTester_Case140 baseline_tester;
  NamedForbidden_Ldc2_Rule_52_A2_P108 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111110pudw11111iiiiiiiiiiiiiiii");
}

// Neutral case:
// inst(27:20)=1110xxxx & inst(4)=0
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '11111110iiiiiiiiiiiiiiiiiii0iiii',
//     'rule': 'Cdp2_Rule_28_A2_P68'}
//
// Representative case:
// op1(27:20)=1110xxxx & op(4)=0
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 11111110iiiiiiiiiiiiiiiiiii0iiii,
//     rule: Cdp2_Rule_28_A2_P68}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case141_TestCase141) {
  ForbiddenUncondDecoderTester_Case141 baseline_tester;
  NamedForbidden_Cdp2_Rule_28_A2_P68 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11111110iiiiiiiiiiiiiiiiiii0iiii");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx00 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = Unary1RegisterUse => Unary1RegisterUse {'constraints': ,
//     'pattern': 'cccc00010010mm00111100000000nnnn',
//     'rule': 'Msr_Rule_104_A1_P210'}
//
// Representaive case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = Unary1RegisterUse => Unary1RegisterUse {constraints: ,
//     pattern: cccc00010010mm00111100000000nnnn,
//     rule: Msr_Rule_104_A1_P210}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterUseTester_Case142_TestCase142) {
  Unary1RegisterUseTester_Case142 tester;
  tester.Test("cccc00010010mm00111100000000nnnn");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc00010010mm01111100000000nnnn',
//     'rule': 'Msr_Rule_B6_1_7_P14'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc00010010mm01111100000000nnnn,
//     rule: Msr_Rule_B6_1_7_P14}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case143_TestCase143) {
  ForbiddenCondDecoderTester_Case143 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_7_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010mm01111100000000nnnn");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc00010010mm1m111100000000nnnn',
//     'rule': 'Msr_Rule_B6_1_7_P14'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc00010010mm1m111100000000nnnn,
//     rule: Msr_Rule_B6_1_7_P14}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case144_TestCase144) {
  ForbiddenCondDecoderTester_Case144 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_7_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010mm1m111100000000nnnn");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc00010110mmmm111100000000nnnn',
//     'rule': 'Msr_Rule_B6_1_7_P14'}
//
// Representative case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc00010110mmmm111100000000nnnn,
//     rule: Msr_Rule_B6_1_7_P14}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case145_TestCase145) {
  ForbiddenCondDecoderTester_Case145 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_7_P14 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010110mmmm111100000000nnnn");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = Unary1RegisterSet => Unary1RegisterSet {'constraints': ,
//     'pattern': 'cccc00010r001111dddd000000000000',
//     'rule': 'Mrs_Rule_102_A1_P206_Or_B6_10'}
//
// Representaive case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = Unary1RegisterSet => Unary1RegisterSet {constraints: ,
//     pattern: cccc00010r001111dddd000000000000,
//     rule: Mrs_Rule_102_A1_P206_Or_B6_10}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterSetTester_Case146_TestCase146) {
  Unary1RegisterSetTester_Case146 tester;
  tester.Test("cccc00010r001111dddd000000000000");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc00010r00mmmmdddd001m00000000',
//     'rule': 'Msr_Rule_Banked_register_A1_B9_1990'}
//
// Representative case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc00010r00mmmmdddd001m00000000,
//     rule: Msr_Rule_Banked_register_A1_B9_1990}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case147_TestCase147) {
  ForbiddenCondDecoderTester_Case147 baseline_tester;
  NamedForbidden_Msr_Rule_Banked_register_A1_B9_1990 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r00mmmmdddd001m00000000");
}

// Neutral case:
// inst(6:4)=000 & inst(9)=1 & inst(22:21)=x1 & inst(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc00010r10mmmm1111001m0000nnnn',
//     'rule': 'Msr_Rule_Banked_register_A1_B9_1992'}
//
// Representative case:
// op2(6:4)=000 & B(9)=1 & op(22:21)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc00010r10mmmm1111001m0000nnnn,
//     rule: Msr_Rule_Banked_register_A1_B9_1992}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case148_TestCase148) {
  ForbiddenCondDecoderTester_Case148 baseline_tester;
  NamedForbidden_Msr_Rule_Banked_register_A1_B9_1992 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r10mmmm1111001m0000nnnn");
}

// Neutral case:
// inst(6:4)=001 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = BranchToRegister => BxBlx {'constraints': ,
//     'pattern': 'cccc000100101111111111110001mmmm',
//     'rule': 'Bx_Rule_25_A1_P62'}
//
// Representative case:
// op2(6:4)=001 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = BranchToRegister => BxBlx {constraints: ,
//     pattern: cccc000100101111111111110001mmmm,
//     rule: Bx_Rule_25_A1_P62}
TEST_F(Arm32DecoderStateTests,
       BranchToRegisterTester_Case149_TestCase149) {
  BranchToRegisterTester_Case149 baseline_tester;
  NamedBxBlx_Bx_Rule_25_A1_P62 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110001mmmm");
}

// Neutral case:
// inst(6:4)=001 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPc => Defs12To15RdRnNotPc {'constraints': ,
//     'pattern': 'cccc000101101111dddd11110001mmmm',
//     'rule': 'Clz_Rule_31_A1_P72'}
//
// Representative case:
// op2(6:4)=001 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPc => Defs12To15RdRnNotPc {constraints: ,
//     pattern: cccc000101101111dddd11110001mmmm,
//     rule: Clz_Rule_31_A1_P72}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcTester_Case150_TestCase150) {
  Unary2RegisterOpNotRmIsPcTester_Case150 baseline_tester;
  NamedDefs12To15RdRnNotPc_Clz_Rule_31_A1_P72 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000101101111dddd11110001mmmm");
}

// Neutral case:
// inst(6:4)=010 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc000100101111111111110010mmmm',
//     'rule': 'Bxj_Rule_26_A1_P64'}
//
// Representative case:
// op2(6:4)=010 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc000100101111111111110010mmmm,
//     rule: Bxj_Rule_26_A1_P64}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case151_TestCase151) {
  ForbiddenCondDecoderTester_Case151 baseline_tester;
  NamedForbidden_Bxj_Rule_26_A1_P64 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110010mmmm");
}

// Neutral case:
// inst(6:4)=011 & inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = BranchToRegister => BxBlx {'constraints': ,
//     'pattern': 'cccc000100101111111111110011mmmm',
//     'rule': 'Blx_Rule_24_A1_P60',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op2(6:4)=011 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = BranchToRegister => BxBlx {constraints: ,
//     pattern: cccc000100101111111111110011mmmm,
//     rule: Blx_Rule_24_A1_P60,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       BranchToRegisterTester_Case152_TestCase152) {
  BranchToRegisterTester_Case152 baseline_tester;
  NamedBxBlx_Blx_Rule_24_A1_P60 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110011mmmm");
}

// Neutral case:
// inst(6:4)=110 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc0001011000000000000001101110',
//     'rule': 'Eret_Rule_A1'}
//
// Representative case:
// op2(6:4)=110 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc0001011000000000000001101110,
//     rule: Eret_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case153_TestCase153) {
  ForbiddenCondDecoderTester_Case153 baseline_tester;
  NamedForbidden_Eret_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001011000000000000001101110");
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=01
//    = BreakPointAndConstantPoolHead => Breakpoint {'constraints': ,
//     'pattern': 'cccc00010010iiiiiiiiiiii0111iiii',
//     'rule': 'Bkpt_Rule_22_A1_P56'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=01
//    = BreakPointAndConstantPoolHead => Breakpoint {constraints: ,
//     pattern: cccc00010010iiiiiiiiiiii0111iiii,
//     rule: Bkpt_Rule_22_A1_P56}
TEST_F(Arm32DecoderStateTests,
       BreakPointAndConstantPoolHeadTester_Case154_TestCase154) {
  BreakPointAndConstantPoolHeadTester_Case154 baseline_tester;
  NamedBreakpoint_Bkpt_Rule_22_A1_P56 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010iiiiiiiiiiii0111iiii");
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=10
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc00010100iiiiiiiiiiii0111iiii',
//     'rule': 'Hvc_Rule_A1'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=10
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc00010100iiiiiiiiiiii0111iiii,
//     rule: Hvc_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case155_TestCase155) {
  ForbiddenCondDecoderTester_Case155 baseline_tester;
  NamedForbidden_Hvc_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010100iiiiiiiiiiii0111iiii");
}

// Neutral case:
// inst(6:4)=111 & inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc000101100000000000000111iiii',
//     'rule': 'Smc_Rule_B6_1_9_P18'}
//
// Representative case:
// op2(6:4)=111 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc000101100000000000000111iiii,
//     rule: Smc_Rule_B6_1_9_P18}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case156_TestCase156) {
  ForbiddenCondDecoderTester_Case156 baseline_tester;
  NamedForbidden_Smc_Rule_B6_1_9_P18 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000101100000000000000111iiii");
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=000100
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc11000100ttttttttccccoooommmm',
//     'rule': 'Mcrr_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=000100
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc11000100ttttttttccccoooommmm,
//     rule: Mcrr_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case157_TestCase157) {
  ForbiddenCondDecoderTester_Case157 baseline_tester;
  NamedForbidden_Mcrr_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11000100ttttttttccccoooommmm");
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=000101
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc11000101ttttttttccccoooommmm',
//     'rule': 'Mrrc_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=000101
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc11000101ttttttttccccoooommmm,
//     rule: Mrrc_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case158_TestCase158) {
  ForbiddenCondDecoderTester_Case158 baseline_tester;
  NamedForbidden_Mrrc_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11000101ttttttttccccoooommmm");
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxx0 & inst(4)=1
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc1110ooo0nnnnttttccccooo1mmmm',
//     'rule': 'Mcr_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=10xxx0 & op(4)=1
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc1110ooo0nnnnttttccccooo1mmmm,
//     rule: Mcr_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case159_TestCase159) {
  ForbiddenCondDecoderTester_Case159 baseline_tester;
  NamedForbidden_Mcr_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1110ooo0nnnnttttccccooo1mmmm");
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxx1 & inst(4)=1
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc1110ooo1nnnnttttccccooo1mmmm',
//     'rule': 'Mrc_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=10xxx1 & op(4)=1
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc1110ooo1nnnnttttccccooo1mmmm,
//     rule: Mrc_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case160_TestCase160) {
  ForbiddenCondDecoderTester_Case160 baseline_tester;
  NamedForbidden_Mrc_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1110ooo1nnnnttttccccooo1mmmm");
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx0 & inst(25:20)=~000x00
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc110pudw0nnnnddddcccciiiiiiii',
//     'rule': 'Stc_Rule_A2'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx0 & op1_repeated(25:20)=~000x00
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc110pudw0nnnnddddcccciiiiiiii,
//     rule: Stc_Rule_A2}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case161_TestCase161) {
  ForbiddenCondDecoderTester_Case161 baseline_tester;
  NamedForbidden_Stc_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc110pudw0nnnnddddcccciiiiiiii");
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx1 & inst(19:16)=~1111 & inst(25:20)=~000x01
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc110pudw1nnnnddddcccciiiiiiii',
//     'rule': 'Ldc_immediate_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=~1111 & op1_repeated(25:20)=~000x01
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc110pudw1nnnnddddcccciiiiiiii,
//     rule: Ldc_immediate_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case162_TestCase162) {
  ForbiddenCondDecoderTester_Case162 baseline_tester;
  NamedForbidden_Ldc_immediate_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc110pudw1nnnnddddcccciiiiiiii");
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=0xxxx1 & inst(19:16)=1111 & inst(25:20)=~000x01
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc110pudw11111ddddcccciiiiiiii',
//     'rule': 'Ldc_literal_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=0xxxx1 & Rn(19:16)=1111 & op1_repeated(25:20)=~000x01
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc110pudw11111ddddcccciiiiiiii,
//     rule: Ldc_literal_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case163_TestCase163) {
  ForbiddenCondDecoderTester_Case163 baseline_tester;
  NamedForbidden_Ldc_literal_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc110pudw11111ddddcccciiiiiiii");
}

// Neutral case:
// inst(11:8)=~101x & inst(25:20)=10xxxx & inst(4)=0
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc1110oooonnnnddddccccooo0mmmm',
//     'rule': 'Cdp_Rule_A1'}
//
// Representative case:
// coproc(11:8)=~101x & op1(25:20)=10xxxx & op(4)=0
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc1110oooonnnnddddccccooo0mmmm,
//     rule: Cdp_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case164_TestCase164) {
  ForbiddenCondDecoderTester_Case164 baseline_tester;
  NamedForbidden_Cdp_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1110oooonnnnddddccccooo0mmmm");
}

// Neutral case:
// inst(19:16)=001x & inst(7:6)=x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d11001odddd1010t1m0mmmm',
//     'rule': 'Vcvtb_Vcvtt_Rule_300_A1_P588'}
//
// Representative case:
// opc2(19:16)=001x & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d11001odddd1010t1m0mmmm,
//     rule: Vcvtb_Vcvtt_Rule_300_A1_P588}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case165_TestCase165) {
  CondVfpOpTester_Case165 baseline_tester;
  NamedVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11001odddd1010t1m0mmmm");
}

// Neutral case:
// inst(19:16)=101x & inst(7:6)=x1
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d11101udddd101fx1i0iiii',
//     'rule': 'Vcvt_Rule_297_A1_P582'}
//
// Representative case:
// opc2(19:16)=101x & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d11101udddd101fx1i0iiii,
//     rule: Vcvt_Rule_297_A1_P582}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case166_TestCase166) {
  CondVfpOpTester_Case166 baseline_tester;
  NamedVfpOp_Vcvt_Rule_297_A1_P582 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11101udddd101fx1i0iiii");
}

// Neutral case:
// inst(19:16)=110x & inst(7:6)=x1
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d11110xdddd101sp1m0mmmm',
//     'rule': 'Vcvt_Vcvtr_Rule_295_A1_P578'}
//
// Representative case:
// opc2(19:16)=110x & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d11110xdddd101sp1m0mmmm,
//     rule: Vcvt_Vcvtr_Rule_295_A1_P578}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case167_TestCase167) {
  CondVfpOpTester_Case167 baseline_tester;
  NamedVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11110xdddd101sp1m0mmmm");
}

// Neutral case:
// inst(19:16)=111x & inst(7:6)=x1
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d11111udddd101fx1i0iiii',
//     'rule': 'Vcvt_Rule_297_A1_P582'}
//
// Representative case:
// opc2(19:16)=111x & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d11111udddd101fx1i0iiii,
//     rule: Vcvt_Rule_297_A1_P582}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case168_TestCase168) {
  CondVfpOpTester_Case168 baseline_tester;
  NamedVfpOp_Vcvt_Rule_297_A1_P582 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11111udddd101fx1i0iiii");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterImmedShiftedOpRegsNotPc => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01101000nnnnddddrr000111mmmm',
//     'rule': 'Sxtab16_Rule_221_A1_P436',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterImmedShiftedOpRegsNotPc => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01101000nnnnddddrr000111mmmm,
//     rule: Sxtab16_Rule_221_A1_P436,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpRegsNotPcTester_Case169_TestCase169) {
  Binary3RegisterImmedShiftedOpRegsNotPcTester_Case169 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtab16_Rule_221_A1_P436 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101000nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterImmedShiftedOpRegsNotPc => Defs12To15CondsDontCareRdRnNotPc {'constraints': ,
//     'pattern': 'cccc011010001111ddddrr000111mmmm',
//     'rule': 'Sxtb16_Rule_224_A1_P442',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterImmedShiftedOpRegsNotPc => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: cccc011010001111ddddrr000111mmmm,
//     rule: Sxtb16_Rule_224_A1_P442,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpRegsNotPcTester_Case170_TestCase170) {
  Unary2RegisterImmedShiftedOpRegsNotPcTester_Case170 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxtb16_Rule_224_A1_P442 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010001111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01101000nnnndddd11111011mmmm',
//     'rule': 'Sel_Rule_156_A1_P312',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01101000nnnndddd11111011mmmm,
//     rule: Sel_Rule_156_A1_P312,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case171_TestCase171) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case171 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sel_Rule_156_A1_P312 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101000nnnndddd11111011mmmm");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=00x & inst(15:12)=~1111
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {'constraints': ,
//     'pattern': 'cccc01110000ddddaaaammmm00m1nnnn',
//     'rule': 'Smlad_Rule_167_P332',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=~1111
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: cccc01110000ddddaaaammmm00m1nnnn,
//     rule: Smlad_Rule_167_P332,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case172_TestCase172) {
  Binary4RegisterDualOpTester_Case172 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlad_Rule_167_P332 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000ddddaaaammmm00m1nnnn");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=00x & inst(15:12)=1111
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc01110000dddd1111mmmm00m1nnnn',
//     'rule': 'Smuad_Rule_177_P352',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=1111
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc01110000dddd1111mmmm00m1nnnn,
//     rule: Smuad_Rule_177_P352,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case173_TestCase173) {
  Binary3RegisterOpAltATester_Case173 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smuad_Rule_177_P352 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000dddd1111mmmm00m1nnnn");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=01x & inst(15:12)=~1111
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {'constraints': ,
//     'pattern': 'cccc01110000ddddaaaammmm01m1nnnn',
//     'rule': 'Smlsd_Rule_172_P342',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=~1111
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: cccc01110000ddddaaaammmm01m1nnnn,
//     rule: Smlsd_Rule_172_P342,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case174_TestCase174) {
  Binary4RegisterDualOpTester_Case174 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlsd_Rule_172_P342 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000ddddaaaammmm01m1nnnn");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=01x & inst(15:12)=1111
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc01110000dddd1111mmmm01m1nnnn',
//     'rule': 'Smusd_Rule_181_P360',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=1111
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc01110000dddd1111mmmm01m1nnnn,
//     rule: Smusd_Rule_181_P360,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case175_TestCase175) {
  Binary3RegisterOpAltATester_Case175 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smusd_Rule_181_P360 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000dddd1111mmmm01m1nnnn");
}

// Neutral case:
// inst(22:20)=000 & inst(7:5)=xx0
//    = Binary3RegisterImmedShiftedOpRegsNotPc => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01101000nnnnddddiiiiit01mmmm',
//     'rule': 'Pkh_Rule_116_A1_P234',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=000 & op2(7:5)=xx0
//    = Binary3RegisterImmedShiftedOpRegsNotPc => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01101000nnnnddddiiiiit01mmmm,
//     rule: Pkh_Rule_116_A1_P234,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterImmedShiftedOpRegsNotPcTester_Case176_TestCase176) {
  Binary3RegisterImmedShiftedOpRegsNotPcTester_Case176 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Pkh_Rule_116_A1_P234 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101000nnnnddddiiiiit01mmmm");
}

// Neutral case:
// inst(22:20)=001 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc01110001dddd1111mmmm0001nnnn',
//     'rule': 'Sdiv_Rule_A1'}
//
// Representative case:
// op1(22:20)=001 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc01110001dddd1111mmmm0001nnnn,
//     rule: Sdiv_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case177_TestCase177) {
  Binary3RegisterOpAltATester_Case177 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Sdiv_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110001dddd1111mmmm0001nnnn");
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Unary2RegisterSatImmedShiftedOp => Defs12To15CondsDontCareRdRnNotPc {'constraints': ,
//     'pattern': 'cccc01101010iiiidddd11110011nnnn',
//     'rule': 'Ssat16_Rule_184_A1_P364',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=010 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Unary2RegisterSatImmedShiftedOp => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: cccc01101010iiiidddd11110011nnnn,
//     rule: Ssat16_Rule_184_A1_P364,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_Case178_TestCase178) {
  Unary2RegisterSatImmedShiftedOpTester_Case178 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Ssat16_Rule_184_A1_P364 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101010iiiidddd11110011nnnn");
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01101010nnnnddddrr000111mmmm',
//     'rule': 'Sxtab_Rule_220_A1_P434',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01101010nnnnddddrr000111mmmm,
//     rule: Sxtab_Rule_220_A1_P434,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case179_TestCase179) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case179 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtab_Rule_220_A1_P434 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101010nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=010 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterImmedShiftedOpRegsNotPc => Defs12To15CondsDontCareRdRnNotPc {'constraints': ,
//     'pattern': 'cccc011010101111ddddrr000111mmmm',
//     'rule': 'Sxtb_Rule_223_A1_P440',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=010 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterImmedShiftedOpRegsNotPc => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: cccc011010101111ddddrr000111mmmm,
//     rule: Sxtb_Rule_223_A1_P440,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterImmedShiftedOpRegsNotPcTester_Case180_TestCase180) {
  Unary2RegisterImmedShiftedOpRegsNotPcTester_Case180 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxtb_Rule_223_A1_P440 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010101111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc01110011dddd1111mmmm0001nnnn',
//     'rule': 'Udiv_Rule_A1'}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc01110011dddd1111mmmm0001nnnn,
//     rule: Udiv_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case181_TestCase181) {
  Binary3RegisterOpAltATester_Case181 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Udiv_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110011dddd1111mmmm0001nnnn");
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {'constraints': ,
//     'pattern': 'cccc011010111111dddd11110011mmmm',
//     'rule': 'Rev_Rule_135_A1_P272',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: cccc011010111111dddd11110011mmmm,
//     rule: Rev_Rule_135_A1_P272,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case182_TestCase182) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case182 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Rev_Rule_135_A1_P272 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010111111dddd11110011mmmm");
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01101011nnnnddddrr000111mmmm',
//     'rule': 'Sxtah_Rule_222_A1_P438',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01101011nnnnddddrr000111mmmm,
//     rule: Sxtah_Rule_222_A1_P438,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case183_TestCase183) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case183 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sxtah_Rule_222_A1_P438 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101011nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {'constraints': ,
//     'pattern': 'cccc011010111111ddddrr000111mmmm',
//     'rule': 'Sxth_Rule_225_A1_P444',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: cccc011010111111ddddrr000111mmmm,
//     rule: Sxth_Rule_225_A1_P444,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case184_TestCase184) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case184 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Sxth_Rule_225_A1_P444 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010111111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=011 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {'constraints': ,
//     'pattern': 'cccc011010111111dddd11111011mmmm',
//     'rule': 'Rev16_Rule_136_A1_P274',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=011 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: cccc011010111111dddd11111011mmmm,
//     rule: Rev16_Rule_136_A1_P274,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case185_TestCase185) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case185 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Rev16_Rule_136_A1_P274 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011010111111dddd11111011mmmm");
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01101100nnnnddddrr000111mmmm',
//     'rule': 'Uxtab16_Rule_262_A1_P516',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01101100nnnnddddrr000111mmmm,
//     rule: Uxtab16_Rule_262_A1_P516,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case186_TestCase186) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case186 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtab16_Rule_262_A1_P516 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101100nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {'constraints': ,
//     'pattern': 'cccc011011001111ddddrr000111mmmm',
//     'rule': 'Uxtb16_Rule_264_A1_P522',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: cccc011011001111ddddrr000111mmmm,
//     rule: Uxtb16_Rule_264_A1_P522,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case187_TestCase187) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case187 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxtb16_Rule_264_A1_P522 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011001111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=00x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc01110100hhhhllllmmmm00m1nnnn',
//     'rule': 'Smlald_Rule_170_P336',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=00x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc01110100hhhhllllmmmm00m1nnnn,
//     rule: Smlald_Rule_170_P336,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Case188_TestCase188) {
  Binary4RegisterDualResultTester_Case188 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlald_Rule_170_P336 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110100hhhhllllmmmm00m1nnnn");
}

// Neutral case:
// inst(22:20)=100 & inst(7:5)=01x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc01110100hhhhllllmmmm01m1nnnn',
//     'rule': 'Smlsld_Rule_173_P344',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=100 & op2(7:5)=01x
//    = Binary4RegisterDualResult => Defs12To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc01110100hhhhllllmmmm01m1nnnn,
//     rule: Smlsld_Rule_173_P344,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultTester_Case189_TestCase189) {
  Binary4RegisterDualResultTester_Case189 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlsld_Rule_173_P344 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110100hhhhllllmmmm01m1nnnn");
}

// Neutral case:
// inst(22:20)=101 & inst(7:5)=00x & inst(15:12)=~1111
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {'constraints': ,
//     'pattern': 'cccc01110101ddddaaaammmm00r1nnnn',
//     'rule': 'Smmla_Rule_174_P346',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=~1111
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: cccc01110101ddddaaaammmm00r1nnnn,
//     rule: Smmla_Rule_174_P346,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case190_TestCase190) {
  Binary4RegisterDualOpTester_Case190 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmla_Rule_174_P346 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110101ddddaaaammmm00r1nnnn");
}

// Neutral case:
// inst(22:20)=101 & inst(7:5)=00x & inst(15:12)=1111
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {'constraints': ,
//     'pattern': 'cccc01110101dddd1111mmmm00r1nnnn',
//     'rule': 'Smmul_Rule_176_P350',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=1111
//    = Binary3RegisterOpAltA => Defs16To19CondsDontCareRdRmRnNotPc {constraints: ,
//     pattern: cccc01110101dddd1111mmmm00r1nnnn,
//     rule: Smmul_Rule_176_P350,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltATester_Case191_TestCase191) {
  Binary3RegisterOpAltATester_Case191 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smmul_Rule_176_P350 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110101dddd1111mmmm00r1nnnn");
}

// Neutral case:
// inst(22:20)=101 & inst(7:5)=11x
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {'constraints': ,
//     'pattern': 'cccc01110101ddddaaaammmm11r1nnnn',
//     'rule': 'Smmls_Rule_175_P348',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=101 & op2(7:5)=11x
//    = Binary4RegisterDualOp => Defs16To19CondsDontCareRdRaRmRnNotPc {constraints: ,
//     pattern: cccc01110101ddddaaaammmm11r1nnnn,
//     rule: Smmls_Rule_175_P348,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpTester_Case192_TestCase192) {
  Binary4RegisterDualOpTester_Case192 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smmls_Rule_175_P348 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110101ddddaaaammmm11r1nnnn");
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Unary2RegisterSatImmedShiftedOp => Defs12To15CondsDontCareRdRnNotPc {'constraints': ,
//     'pattern': 'cccc01101110iiiidddd11110011nnnn',
//     'rule': 'Usat16_Rule_256_A1_P506',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=110 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Unary2RegisterSatImmedShiftedOp => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: cccc01101110iiiidddd11110011nnnn,
//     rule: Usat16_Rule_256_A1_P506,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_Case193_TestCase193) {
  Unary2RegisterSatImmedShiftedOpTester_Case193 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Usat16_Rule_256_A1_P506 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101110iiiidddd11110011nnnn");
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01101110nnnnddddrr000111mmmm',
//     'rule': 'Uxtab_Rule_260_A1_P514',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01101110nnnnddddrr000111mmmm,
//     rule: Uxtab_Rule_260_A1_P514,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case194_TestCase194) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case194 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtab_Rule_260_A1_P514 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101110nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=110 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {'constraints': ,
//     'pattern': 'cccc011011101111ddddrr000111mmmm',
//     'rule': 'Uxtb_Rule_263_A1_P520',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=110 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: cccc011011101111ddddrr000111mmmm,
//     rule: Uxtb_Rule_263_A1_P520,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case195_TestCase195) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case195 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxtb_Rule_263_A1_P520 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011101111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {'constraints': ,
//     'pattern': 'cccc011011111111dddd11110011mmmm',
//     'rule': 'Rbit_Rule_134_A1_P270',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: cccc011011111111dddd11110011mmmm,
//     rule: Rbit_Rule_134_A1_P270,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case196_TestCase196) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case196 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Rbit_Rule_134_A1_P270 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011111111dddd11110011mmmm");
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01101111nnnnddddrr000111mmmm',
//     'rule': 'Uxtah_Rule_262_A1_P518',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01101111nnnnddddrr000111mmmm,
//     rule: Uxtah_Rule_262_A1_P518,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case197_TestCase197) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case197 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uxtah_Rule_262_A1_P518 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01101111nnnnddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=011 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {'constraints': ,
//     'pattern': 'cccc011011111111ddddrr000111mmmm',
//     'rule': 'Uxth_Rule_265_A1_P524',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=011 & A(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxx00xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: cccc011011111111ddddrr000111mmmm,
//     rule: Uxth_Rule_265_A1_P524,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case198_TestCase198) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case198 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Uxth_Rule_265_A1_P524 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011111111ddddrr000111mmmm");
}

// Neutral case:
// inst(22:20)=111 & inst(7:5)=101 & inst(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {'constraints': ,
//     'pattern': 'cccc011011111111dddd11111011mmmm',
//     'rule': 'Revsh_Rule_137_A1_P276',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=111 & op2(7:5)=101 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = Unary2RegisterOpNotRmIsPcNoCondUpdates => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: cccc011011111111dddd11111011mmmm,
//     rule: Revsh_Rule_137_A1_P276,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case199_TestCase199) {
  Unary2RegisterOpNotRmIsPcNoCondUpdatesTester_Case199 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Revsh_Rule_137_A1_P276 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011011111111dddd11111011mmmm");
}

// Neutral case:
// inst(23:20)=0x00 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Deprecated => Deprecated {'constraints': ,
//     'pattern': 'cccc00010b00nnnntttt00001001tttt',
//     'rule': 'Swp_Swpb_Rule_A1'}
//
// Representaive case:
// op(23:20)=0x00 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Deprecated => Deprecated {constraints: ,
//     pattern: cccc00010b00nnnntttt00001001tttt,
//     rule: Swp_Swpb_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       DeprecatedTester_Case200_TestCase200) {
  DeprecatedTester_Case200 tester;
  tester.Test("cccc00010b00nnnntttt00001001tttt");
}

// Neutral case:
// inst(23:20)=0x00
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11100d00nnnndddd101snom0mmmm',
//     'rule': 'Vmla_vmls_Rule_423_A2_P636'}
//
// Representative case:
// opc1(23:20)=0x00
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11100d00nnnndddd101snom0mmmm,
//     rule: Vmla_vmls_Rule_423_A2_P636}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case201_TestCase201) {
  CondVfpOpTester_Case201 baseline_tester;
  NamedVfpOp_Vmla_vmls_Rule_423_A2_P636 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d00nnnndddd101snom0mmmm");
}

// Neutral case:
// inst(23:20)=0x01
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11100d01nnnndddd101snom0mmmm',
//     'rule': 'Vnmla_vnmls_Rule_343_A1_P674'}
//
// Representative case:
// opc1(23:20)=0x01
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11100d01nnnndddd101snom0mmmm,
//     rule: Vnmla_vnmls_Rule_343_A1_P674}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case202_TestCase202) {
  CondVfpOpTester_Case202 baseline_tester;
  NamedVfpOp_Vnmla_vnmls_Rule_343_A1_P674 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d01nnnndddd101snom0mmmm");
}

// Neutral case:
// inst(23:20)=0x10 & inst(7:6)=x0
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11100d10nnnndddd101sn0m0mmmm',
//     'rule': 'Vmul_Rule_338_A2_P664'}
//
// Representative case:
// opc1(23:20)=0x10 & opc3(7:6)=x0
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11100d10nnnndddd101sn0m0mmmm,
//     rule: Vmul_Rule_338_A2_P664}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case203_TestCase203) {
  CondVfpOpTester_Case203 baseline_tester;
  NamedVfpOp_Vmul_Rule_338_A2_P664 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d10nnnndddd101sn0m0mmmm");
}

// Neutral case:
// inst(23:20)=0x10 & inst(7:6)=x1
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11100d10nnnndddd101sn1m0mmmm',
//     'rule': 'Vnmul_Rule_343_A2_P674'}
//
// Representative case:
// opc1(23:20)=0x10 & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11100d10nnnndddd101sn1m0mmmm,
//     rule: Vnmul_Rule_343_A2_P674}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case204_TestCase204) {
  CondVfpOpTester_Case204 baseline_tester;
  NamedVfpOp_Vnmul_Rule_343_A2_P674 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d10nnnndddd101sn1m0mmmm");
}

// Neutral case:
// inst(23:20)=0x11 & inst(7:6)=x0
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11100d11nnnndddd101sn0m0mmmm',
//     'rule': 'Vadd_Rule_271_A2_P536'}
//
// Representative case:
// opc1(23:20)=0x11 & opc3(7:6)=x0
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11100d11nnnndddd101sn0m0mmmm,
//     rule: Vadd_Rule_271_A2_P536}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case205_TestCase205) {
  CondVfpOpTester_Case205 baseline_tester;
  NamedVfpOp_Vadd_Rule_271_A2_P536 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d11nnnndddd101sn0m0mmmm");
}

// Neutral case:
// inst(23:20)=0x11 & inst(7:6)=x1
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11100d11nnnndddd101sn1m0mmmm',
//     'rule': 'Vsub_Rule_402_A2_P790'}
//
// Representative case:
// opc1(23:20)=0x11 & opc3(7:6)=x1
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11100d11nnnndddd101sn1m0mmmm,
//     rule: Vsub_Rule_402_A2_P790}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case206_TestCase206) {
  CondVfpOpTester_Case206 baseline_tester;
  NamedVfpOp_Vsub_Rule_402_A2_P790 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11100d11nnnndddd101sn1m0mmmm");
}

// Neutral case:
// inst(23:20)=1x00 & inst(7:6)=x0
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d00nnnndddd101sn0m0mmmm',
//     'rule': 'Vdiv_Rule_301_A1_P590'}
//
// Representative case:
// opc1(23:20)=1x00 & opc3(7:6)=x0
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d00nnnndddd101sn0m0mmmm,
//     rule: Vdiv_Rule_301_A1_P590}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case207_TestCase207) {
  CondVfpOpTester_Case207 baseline_tester;
  NamedVfpOp_Vdiv_Rule_301_A1_P590 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d00nnnndddd101sn0m0mmmm");
}

// Neutral case:
// inst(23:20)=1x01
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d01nnnndddd101snom0mmmm',
//     'rule': 'Vfnma_vfnms_Rule_A1'}
//
// Representative case:
// opc1(23:20)=1x01
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d01nnnndddd101snom0mmmm,
//     rule: Vfnma_vfnms_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case208_TestCase208) {
  CondVfpOpTester_Case208 baseline_tester;
  NamedVfpOp_Vfnma_vfnms_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d01nnnndddd101snom0mmmm");
}

// Neutral case:
// inst(23:20)=1x10
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d10nnnndddd101snom0mmmm',
//     'rule': 'Vfma_vfms_Rule_A1'}
//
// Representative case:
// opc1(23:20)=1x10
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d10nnnndddd101snom0mmmm,
//     rule: Vfma_vfms_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case209_TestCase209) {
  CondVfpOpTester_Case209 baseline_tester;
  NamedVfpOp_Vfma_vfms_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d10nnnndddd101snom0mmmm");
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
       Binary3RegisterOpAltATester_Case210_TestCase210) {
  Binary3RegisterOpAltATester_Case210 baseline_tester;
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
       Binary4RegisterDualOpTester_Case211_TestCase211) {
  Binary4RegisterDualOpTester_Case211 baseline_tester;
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
       Binary4RegisterDualResultTester_Case212_TestCase212) {
  Binary4RegisterDualResultTester_Case212 baseline_tester;
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
       Binary4RegisterDualResultTester_Case213_TestCase213) {
  Binary4RegisterDualResultTester_Case213 baseline_tester;
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
       Binary4RegisterDualResultTester_Case214_TestCase214) {
  Binary4RegisterDualResultTester_Case214 baseline_tester;
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
       Binary4RegisterDualResultTester_Case215_TestCase215) {
  Binary4RegisterDualResultTester_Case215 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlal_Rule_168_A1_P334 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111shhhhllllmmmm1001nnnn");
}

// Neutral case:
// inst(24:20)=1xx00
//    = StoreVectorRegister => StoreVectorRegister {'constraints': ,
//     'pattern': 'cccc1101ud00nnnndddd101xiiiiiiii',
//     'rule': 'Vstr_Rule_400_A1_A2_P786'}
//
// Representaive case:
// opcode(24:20)=1xx00
//    = StoreVectorRegister => StoreVectorRegister {constraints: ,
//     pattern: cccc1101ud00nnnndddd101xiiiiiiii,
//     rule: Vstr_Rule_400_A1_A2_P786}
TEST_F(Arm32DecoderStateTests,
       StoreVectorRegisterTester_Case216_TestCase216) {
  StoreVectorRegisterTester_Case216 tester;
  tester.Test("cccc1101ud00nnnndddd101xiiiiiiii");
}

// Neutral case:
// inst(24:20)=1xx01
//    = LoadVectorRegister => LoadVectorRegister {'constraints': ,
//     'pattern': 'cccc1101ud01nnnndddd101xiiiiiiii',
//     'rule': 'Vldr_Rule_320_A1_A2_P628'}
//
// Representaive case:
// opcode(24:20)=1xx01
//    = LoadVectorRegister => LoadVectorRegister {constraints: ,
//     pattern: cccc1101ud01nnnndddd101xiiiiiiii,
//     rule: Vldr_Rule_320_A1_A2_P628}
TEST_F(Arm32DecoderStateTests,
       LoadVectorRegisterTester_Case217_TestCase217) {
  LoadVectorRegisterTester_Case217 tester;
  tester.Test("cccc1101ud01nnnndddd101xiiiiiiii");
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
       ForbiddenCondDecoderTester_Case218_TestCase218) {
  ForbiddenCondDecoderTester_Case218 baseline_tester;
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
       ForbiddenCondDecoderTester_Case219_TestCase219) {
  ForbiddenCondDecoderTester_Case219 baseline_tester;
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
       ForbiddenCondDecoderTester_Case220_TestCase220) {
  ForbiddenCondDecoderTester_Case220 baseline_tester;
  NamedForbidden_Ldm_Rule_2_B6_A1_P5 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc100pu1w1nnnn1rrrrrrrrrrrrrrr");
}

// Neutral case:
// inst(27:20)=101xxxxx
//    = ForbiddenUncondDecoder => Forbidden {'constraints': ,
//     'pattern': '1111101hiiiiiiiiiiiiiiiiiiiiiiii',
//     'rule': 'Blx_Rule_23_A2_P58'}
//
// Representative case:
// op1(27:20)=101xxxxx
//    = ForbiddenUncondDecoder => Forbidden {constraints: ,
//     pattern: 1111101hiiiiiiiiiiiiiiiiiiiiiiii,
//     rule: Blx_Rule_23_A2_P58}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case221_TestCase221) {
  ForbiddenUncondDecoderTester_Case221 baseline_tester;
  NamedForbidden_Blx_Rule_23_A2_P58 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111101hiiiiiiiiiiiiiiiiiiiiiiii");
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Store3RegisterOp => StoreBasedOffsetMemory {'constraints': ,
//     'pattern': 'cccc000pu0w0nnnntttt00001011mmmm',
//     'rule': 'Strh_Rule_208_A1_P412'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Store3RegisterOp => StoreBasedOffsetMemory {constraints: ,
//     pattern: cccc000pu0w0nnnntttt00001011mmmm,
//     rule: Strh_Rule_208_A1_P412}
TEST_F(Arm32DecoderStateTests,
       Store3RegisterOpTester_Case222_TestCase222) {
  Store3RegisterOpTester_Case222 baseline_tester;
  NamedStoreBasedOffsetMemory_Strh_Rule_208_A1_P412 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w0nnnntttt00001011mmmm");
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp => LoadBasedOffsetMemory {'constraints': ,
//     'pattern': 'cccc000pu0w1nnnntttt00001011mmmm',
//     'rule': 'Ldrh_Rule_76_A1_P156'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp => LoadBasedOffsetMemory {constraints: ,
//     pattern: cccc000pu0w1nnnntttt00001011mmmm,
//     rule: Ldrh_Rule_76_A1_P156}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_Case223_TestCase223) {
  Load3RegisterOpTester_Case223 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrh_Rule_76_A1_P156 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w1nnnntttt00001011mmmm");
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x0
//    = Store2RegisterImm8Op => StoreBasedImmedMemory {'constraints': ,
//     'pattern': 'cccc000pu1w0nnnnttttiiii1011iiii',
//     'rule': 'Strh_Rule_207_A1_P410'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx1x0
//    = Store2RegisterImm8Op => StoreBasedImmedMemory {constraints: ,
//     pattern: cccc000pu1w0nnnnttttiiii1011iiii,
//     rule: Strh_Rule_207_A1_P410}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm8OpTester_Case224_TestCase224) {
  Store2RegisterImm8OpTester_Case224 baseline_tester;
  NamedStoreBasedImmedMemory_Strh_Rule_207_A1_P410 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w0nnnnttttiiii1011iiii");
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {'constraints': ,
//     'pattern': 'cccc000pu1w1nnnnttttiiii1011iiii',
//     'rule': 'Ldrh_Rule_74_A1_P152'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {constraints: ,
//     pattern: cccc000pu1w1nnnnttttiiii1011iiii,
//     rule: Ldrh_Rule_74_A1_P152}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Case225_TestCase225) {
  Load2RegisterImm8OpTester_Case225 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrh_Rule_74_A1_P152 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w1nnnnttttiiii1011iiii");
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {'constraints': ,
//     'pattern': 'cccc0001u1011111ttttiiii1011iiii',
//     'rule': 'Ldrh_Rule_75_A1_P154'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {constraints: ,
//     pattern: cccc0001u1011111ttttiiii1011iiii,
//     rule: Ldrh_Rule_75_A1_P154}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Case226_TestCase226) {
  Load2RegisterImm8OpTester_Case226 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrh_Rule_75_A1_P154 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1011111ttttiiii1011iiii");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterDoubleOp => LoadBasedOffsetMemoryDouble {'constraints': ,
//     'pattern': 'cccc000pu0w0nnnntttt00001101mmmm',
//     'rule': 'Ldrd_Rule_68_A1_P140'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterDoubleOp => LoadBasedOffsetMemoryDouble {constraints: ,
//     pattern: cccc000pu0w0nnnntttt00001101mmmm,
//     rule: Ldrd_Rule_68_A1_P140}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterDoubleOpTester_Case227_TestCase227) {
  Load3RegisterDoubleOpTester_Case227 baseline_tester;
  NamedLoadBasedOffsetMemoryDouble_Ldrd_Rule_68_A1_P140 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w0nnnntttt00001101mmmm");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp => LoadBasedOffsetMemory {'constraints': ,
//     'pattern': 'cccc000pu0w1nnnntttt00001101mmmm',
//     'rule': 'Ldrsb_Rule_80_A1_P164'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp => LoadBasedOffsetMemory {constraints: ,
//     pattern: cccc000pu0w1nnnntttt00001101mmmm,
//     rule: Ldrsb_Rule_80_A1_P164}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_Case228_TestCase228) {
  Load3RegisterOpTester_Case228 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrsb_Rule_80_A1_P164 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w1nnnntttt00001101mmmm");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=~1111
//    = Load2RegisterImm8DoubleOp => LoadBasedImmedMemoryDouble {'constraints': ,
//     'pattern': 'cccc000pu1w0nnnnttttiiii1101iiii',
//     'rule': 'Ldrd_Rule_66_A1_P136'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=~1111
//    = Load2RegisterImm8DoubleOp => LoadBasedImmedMemoryDouble {constraints: ,
//     pattern: cccc000pu1w0nnnnttttiiii1101iiii,
//     rule: Ldrd_Rule_66_A1_P136}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8DoubleOpTester_Case229_TestCase229) {
  Load2RegisterImm8DoubleOpTester_Case229 baseline_tester;
  NamedLoadBasedImmedMemoryDouble_Ldrd_Rule_66_A1_P136 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w0nnnnttttiiii1101iiii");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=1111
//    = Load2RegisterImm8DoubleOp => LoadBasedImmedMemoryDouble {'constraints': ,
//     'pattern': 'cccc0001u1001111ttttiiii1101iiii',
//     'rule': 'Ldrd_Rule_67_A1_P138'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=1111
//    = Load2RegisterImm8DoubleOp => LoadBasedImmedMemoryDouble {constraints: ,
//     pattern: cccc0001u1001111ttttiiii1101iiii,
//     rule: Ldrd_Rule_67_A1_P138}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8DoubleOpTester_Case230_TestCase230) {
  Load2RegisterImm8DoubleOpTester_Case230 baseline_tester;
  NamedLoadBasedImmedMemoryDouble_Ldrd_Rule_67_A1_P138 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1001111ttttiiii1101iiii");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {'constraints': ,
//     'pattern': 'cccc000pu1w1nnnnttttiiii1101iiii',
//     'rule': 'Ldrsb_Rule_78_A1_P160'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {constraints: ,
//     pattern: cccc000pu1w1nnnnttttiiii1101iiii,
//     rule: Ldrsb_Rule_78_A1_P160}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Case231_TestCase231) {
  Load2RegisterImm8OpTester_Case231 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrsb_Rule_78_A1_P160 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w1nnnnttttiiii1101iiii");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {'constraints': ,
//     'pattern': 'cccc0001u1011111ttttiiii1101iiii',
//     'rule': 'ldrsb_Rule_79_A1_162'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {constraints: ,
//     pattern: cccc0001u1011111ttttiiii1101iiii,
//     rule: ldrsb_Rule_79_A1_162}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Case232_TestCase232) {
  Load2RegisterImm8OpTester_Case232 baseline_tester;
  NamedLoadBasedImmedMemory_ldrsb_Rule_79_A1_162 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1011111ttttiiii1101iiii");
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Store3RegisterDoubleOp => StoreBasedOffsetMemoryDouble {'constraints': ,
//     'pattern': 'cccc000pu0w0nnnntttt00001111mmmm',
//     'rule': 'Strd_Rule_201_A1_P398'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Store3RegisterDoubleOp => StoreBasedOffsetMemoryDouble {constraints: ,
//     pattern: cccc000pu0w0nnnntttt00001111mmmm,
//     rule: Strd_Rule_201_A1_P398}
TEST_F(Arm32DecoderStateTests,
       Store3RegisterDoubleOpTester_Case233_TestCase233) {
  Store3RegisterDoubleOpTester_Case233 baseline_tester;
  NamedStoreBasedOffsetMemoryDouble_Strd_Rule_201_A1_P398 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w0nnnntttt00001111mmmm");
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp => LoadBasedOffsetMemory {'constraints': ,
//     'pattern': 'cccc000pu0w1nnnntttt00001111mmmm',
//     'rule': 'Ldrsh_Rule_84_A1_P172'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Load3RegisterOp => LoadBasedOffsetMemory {constraints: ,
//     pattern: cccc000pu0w1nnnntttt00001111mmmm,
//     rule: Ldrsh_Rule_84_A1_P172}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_Case234_TestCase234) {
  Load3RegisterOpTester_Case234 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrsh_Rule_84_A1_P172 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w1nnnntttt00001111mmmm");
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x0
//    = Store2RegisterImm8DoubleOp => StoreBasedImmedMemoryDouble {'constraints': ,
//     'pattern': 'cccc000pu1w0nnnnttttiiii1111iiii',
//     'rule': 'Strd_Rule_200_A1_P396'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx1x0
//    = Store2RegisterImm8DoubleOp => StoreBasedImmedMemoryDouble {constraints: ,
//     pattern: cccc000pu1w0nnnnttttiiii1111iiii,
//     rule: Strd_Rule_200_A1_P396}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm8DoubleOpTester_Case235_TestCase235) {
  Store2RegisterImm8DoubleOpTester_Case235 baseline_tester;
  NamedStoreBasedImmedMemoryDouble_Strd_Rule_200_A1_P396 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w0nnnnttttiiii1111iiii");
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {'constraints': ,
//     'pattern': 'cccc000pu1w1nnnnttttiiii1111iiii',
//     'rule': 'Ldrsh_Rule_82_A1_P168'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {constraints: ,
//     pattern: cccc000pu1w1nnnnttttiiii1111iiii,
//     rule: Ldrsh_Rule_82_A1_P168}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Case236_TestCase236) {
  Load2RegisterImm8OpTester_Case236 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrsh_Rule_82_A1_P168 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w1nnnnttttiiii1111iiii");
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {'constraints': ,
//     'pattern': 'cccc0001u1011111ttttiiii1111iiii',
//     'rule': 'Ldrsh_Rule_83_A1_P170'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=1111
//    = Load2RegisterImm8Op => LoadBasedImmedMemory {constraints: ,
//     pattern: cccc0001u1011111ttttiiii1111iiii,
//     rule: Ldrsh_Rule_83_A1_P170}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Case237_TestCase237) {
  Load2RegisterImm8OpTester_Case237 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrsh_Rule_83_A1_P170 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1011111ttttiiii1111iiii");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100001nnnndddd11110001mmmm',
//     'rule': 'Sadd16_Rule_148_A1_P296',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100001nnnndddd11110001mmmm,
//     rule: Sadd16_Rule_148_A1_P296,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case238_TestCase238) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case238 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd16_Rule_148_A1_P296 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110001mmmm");
}

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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case239_TestCase239) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case239 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd16_Rule_233_A1_P460 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100101nnnndddd11110001mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100001nnnndddd11110011mmmm',
//     'rule': 'Sasx_Rule_150_A1_P300',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100001nnnndddd11110011mmmm,
//     rule: Sasx_Rule_150_A1_P300,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case240_TestCase240) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case240 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sasx_Rule_150_A1_P300 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110011mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case241_TestCase241) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case241 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uasx_Rule_235_A1_P464 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100101nnnndddd11110011mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100001nnnndddd11110101mmmm',
//     'rule': 'Ssax_Rule_185_A1_P366',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100001nnnndddd11110101mmmm,
//     rule: Ssax_Rule_185_A1_P366,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case242_TestCase242) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case242 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssax_Rule_185_A1_P366 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110101mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case243_TestCase243) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case243 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Usax_Rule_257_A1_P508 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100101nnnndddd11110101mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100001nnnndddd11110111mmmm',
//     'rule': 'Ssub16_Rule_186_A1_P368',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100001nnnndddd11110111mmmm,
//     rule: Ssub16_Rule_186_A1_P368,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case244_TestCase244) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case244 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub16_Rule_186_A1_P368 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11110111mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case245_TestCase245) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case245 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub16_Rule_258_A1_P510 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100101nnnndddd11110111mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100001nnnndddd11111001mmmm',
//     'rule': 'Sadd8_Rule_149_A1_P298',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100001nnnndddd11111001mmmm,
//     rule: Sadd8_Rule_149_A1_P298,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case246_TestCase246) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case246 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Sadd8_Rule_149_A1_P298 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11111001mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case247_TestCase247) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case247 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uadd8_Rule_234_A1_P462 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100101nnnndddd11111001mmmm");
}

// Neutral case:
// inst(21:20)=01 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100001nnnndddd11111111mmmm',
//     'rule': 'Ssub8_Rule_187_A1_P370',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=01 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100001nnnndddd11111111mmmm,
//     rule: Ssub8_Rule_187_A1_P370,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case248_TestCase248) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case248 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Ssub8_Rule_187_A1_P370 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100001nnnndddd11111111mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case249_TestCase249) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case249 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Usub8_Rule_259_A1_P512 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100101nnnndddd11111111mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100010nnnndddd11110001mmmm',
//     'rule': 'Qadd16_Rule_125_A1_P252',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100010nnnndddd11110001mmmm,
//     rule: Qadd16_Rule_125_A1_P252,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case250_TestCase250) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case250 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd16_Rule_125_A1_P252 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110001mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case251_TestCase251) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case251 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd16_Rule_247_A1_P488 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100110nnnndddd11110001mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100010nnnndddd11110011mmmm',
//     'rule': 'Qasx_Rule_127_A1_P256',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100010nnnndddd11110011mmmm,
//     rule: Qasx_Rule_127_A1_P256,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case252_TestCase252) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case252 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qasx_Rule_127_A1_P256 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110011mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case253_TestCase253) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case253 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqasx_Rule_249_A1_P492 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100110nnnndddd11110011mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100010nnnndddd11110101mmmm',
//     'rule': 'Qsax_Rule_130_A1_P262',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100010nnnndddd11110101mmmm,
//     rule: Qsax_Rule_130_A1_P262,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case254_TestCase254) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case254 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsax_Rule_130_A1_P262 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110101mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case255_TestCase255) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case255 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsax_Rule_250_A1_P494 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100110nnnndddd11110101mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100010nnnndddd11110111mmmm',
//     'rule': 'Qsub16_Rule_132_A1_P266',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100010nnnndddd11110111mmmm,
//     rule: Qsub16_Rule_132_A1_P266,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case256_TestCase256) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case256 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub16_Rule_132_A1_P266 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11110111mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case257_TestCase257) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case257 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub16_Rule_251_A1_P496 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100110nnnndddd11110111mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100010nnnndddd11111001mmmm',
//     'rule': 'Qadd8_Rule_126_A1_P254',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100010nnnndddd11111001mmmm,
//     rule: Qadd8_Rule_126_A1_P254,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case258_TestCase258) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case258 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd8_Rule_126_A1_P254 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11111001mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case259_TestCase259) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case259 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqadd8_Rule_248_A1_P490 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100110nnnndddd11111001mmmm");
}

// Neutral case:
// inst(21:20)=10 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100010nnnndddd11111111mmmm',
//     'rule': 'Qsub8_Rule_133_A1_P268',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=10 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100010nnnndddd11111111mmmm,
//     rule: Qsub8_Rule_133_A1_P268,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case260_TestCase260) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case260 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub8_Rule_133_A1_P268 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100010nnnndddd11111111mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case261_TestCase261) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case261 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uqsub8_Rule_252_A1_P498 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100110nnnndddd11111111mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100011nnnndddd11110001mmmm',
//     'rule': 'Shadd16_Rule_159_A1_P318',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100011nnnndddd11110001mmmm,
//     rule: Shadd16_Rule_159_A1_P318,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case262_TestCase262) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case262 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd16_Rule_159_A1_P318 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110001mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case263_TestCase263) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case263 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd16_Rule_238_A1_P470 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100111nnnndddd11110001mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=001 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100011nnnndddd11110011mmmm',
//     'rule': 'Shasx_Rule_161_A1_P322',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=001 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100011nnnndddd11110011mmmm,
//     rule: Shasx_Rule_161_A1_P322,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case264_TestCase264) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case264 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shasx_Rule_161_A1_P322 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110011mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case265_TestCase265) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case265 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhasx_Rule_240_A1_P474 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100111nnnndddd11110011mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=010 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100011nnnndddd11110101mmmm',
//     'rule': 'Shsax_Rule_162_A1_P324',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=010 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100011nnnndddd11110101mmmm,
//     rule: Shsax_Rule_162_A1_P324,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case266_TestCase266) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case266 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsax_Rule_162_A1_P324 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110101mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case267_TestCase267) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case267 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsax_Rule_241_A1_P476 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100111nnnndddd11110101mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=011 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100011nnnndddd11110111mmmm',
//     'rule': 'Shsub16_Rule_163_A1_P326',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=011 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100011nnnndddd11110111mmmm,
//     rule: Shsub16_Rule_163_A1_P326,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case268_TestCase268) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case268 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub16_Rule_163_A1_P326 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11110111mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case269_TestCase269) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case269 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub16_Rule_242_A1_P478 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100111nnnndddd11110111mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=100 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100011nnnndddd11111001mmmm',
//     'rule': 'Shadd8_Rule_160_A1_P320',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=100 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100011nnnndddd11111001mmmm,
//     rule: Shadd8_Rule_160_A1_P320,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case270_TestCase270) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case270 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shadd8_Rule_160_A1_P320 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11111001mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case271_TestCase271) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case271 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhadd8_Rule_239_A1_P472 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100111nnnndddd11111001mmmm");
}

// Neutral case:
// inst(21:20)=11 & inst(7:5)=111 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc01100011nnnndddd11111111mmmm',
//     'rule': 'Shsub8_Rule_164_A1_P328',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(21:20)=11 & op2(7:5)=111 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx1111xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc01100011nnnndddd11111111mmmm,
//     rule: Shsub8_Rule_164_A1_P328,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case272_TestCase272) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case272 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Shsub8_Rule_164_A1_P328 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100011nnnndddd11111111mmmm");
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
       Binary3RegisterOpAltBNoCondUpdatesTester_Case273_TestCase273) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case273 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Uhsub8_Rule_243_A1_P480 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01100111nnnndddd11111111mmmm");
}

// Neutral case:
// inst(22:21)=00 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc00010000nnnndddd00000101mmmm',
//     'rule': 'Qadd_Rule_124_A1_P250',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=00 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc00010000nnnndddd00000101mmmm,
//     rule: Qadd_Rule_124_A1_P250,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case274_TestCase274) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case274 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qadd_Rule_124_A1_P250 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010000nnnndddd00000101mmmm");
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
       Binary4RegisterDualOpTester_Case275_TestCase275) {
  Binary4RegisterDualOpTester_Case275 baseline_tester;
  NamedDefs16To19CondsDontCareRdRaRmRnNotPc_Smlaxx_Rule_166_A1_P330 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010000ddddaaaammmm1xx0nnnn");
}

// Neutral case:
// inst(22:20)=01x & inst(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp => Defs12To15CondsDontCareRdRnNotPc {'constraints': ,
//     'pattern': 'cccc0110101iiiiiddddiiiiis01nnnn',
//     'rule': 'Ssat_Rule_183_A1_P362',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=01x & op2(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: cccc0110101iiiiiddddiiiiis01nnnn,
//     rule: Ssat_Rule_183_A1_P362,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_Case276_TestCase276) {
  Unary2RegisterSatImmedShiftedOpTester_Case276 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Ssat_Rule_183_A1_P362 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110101iiiiiddddiiiiis01nnnn");
}

// Neutral case:
// inst(22:21)=01 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc00010010nnnndddd00000101mmmm',
//     'rule': 'Qsub_Rule_131_A1_P264',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc00010010nnnndddd00000101mmmm,
//     rule: Qsub_Rule_131_A1_P264,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case277_TestCase277) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case277 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qsub_Rule_131_A1_P264 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010nnnndddd00000101mmmm");
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
       Binary4RegisterDualOpTester_Case278_TestCase278) {
  Binary4RegisterDualOpTester_Case278 baseline_tester;
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
       Binary3RegisterOpAltATester_Case279_TestCase279) {
  Binary3RegisterOpAltATester_Case279 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulwx_Rule_180_A1_P358 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010dddd0000mmmm1x10nnnn");
}

// Neutral case:
// inst(22:21)=10 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc00010100nnnndddd00000101mmmm',
//     'rule': 'Qdadd_Rule_128_A1_P258',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=10 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc00010100nnnndddd00000101mmmm,
//     rule: Qdadd_Rule_128_A1_P258,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case280_TestCase280) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case280 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdadd_Rule_128_A1_P258 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010100nnnndddd00000101mmmm");
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
       Binary4RegisterDualResultTester_Case281_TestCase281) {
  Binary4RegisterDualResultTester_Case281 baseline_tester;
  NamedDefs12To19CondsDontCareRdRmRnNotPc_Smlalxx_Rule_169_A1_P336 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010100hhhhllllmmmm1xx0nnnn");
}

// Neutral case:
// inst(22:20)=11x & inst(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp => Defs12To15CondsDontCareRdRnNotPc {'constraints': ,
//     'pattern': 'cccc0110111iiiiiddddiiiiis01nnnn',
//     'rule': 'Usat_Rule_255_A1_P504',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op1(22:20)=11x & op2(7:5)=xx0
//    = Unary2RegisterSatImmedShiftedOp => Defs12To15CondsDontCareRdRnNotPc {constraints: ,
//     pattern: cccc0110111iiiiiddddiiiiis01nnnn,
//     rule: Usat_Rule_255_A1_P504,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Unary2RegisterSatImmedShiftedOpTester_Case282_TestCase282) {
  Unary2RegisterSatImmedShiftedOpTester_Case282 baseline_tester;
  NamedDefs12To15CondsDontCareRdRnNotPc_Usat_Rule_255_A1_P504 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110111iiiiiddddiiiiis01nnnn");
}

// Neutral case:
// inst(22:21)=11 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {'constraints': ,
//     'pattern': 'cccc00010110nnnndddd00000101mmmm',
//     'rule': 'Qdsub_Rule_129_A1_P260',
//     'safety': ["'RegsNotPc'"]}
//
// Representative case:
// op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = Binary3RegisterOpAltBNoCondUpdates => Defs12To15CondsDontCareRnRdRmNotPc {constraints: ,
//     pattern: cccc00010110nnnndddd00000101mmmm,
//     rule: Qdsub_Rule_129_A1_P260,
//     safety: ['RegsNotPc']}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltBNoCondUpdatesTester_Case283_TestCase283) {
  Binary3RegisterOpAltBNoCondUpdatesTester_Case283 baseline_tester;
  NamedDefs12To15CondsDontCareRnRdRmNotPc_Qdsub_Rule_129_A1_P260 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010110nnnndddd00000101mmmm");
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
       Binary3RegisterOpAltATester_Case284_TestCase284) {
  Binary3RegisterOpAltATester_Case284 baseline_tester;
  NamedDefs16To19CondsDontCareRdRmRnNotPc_Smulxx_Rule_178_P354 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010110dddd0000mmmm1xx0nnnn");
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
       BranchImmediate24Tester_Case285_TestCase285) {
  BranchImmediate24Tester_Case285 baseline_tester;
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
       BranchImmediate24Tester_Case286_TestCase286) {
  BranchImmediate24Tester_Case286 baseline_tester;
  NamedBranch_Bl_Blx_Rule_23_A1_P58 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1011iiiiiiiiiiiiiiiiiiiiiiii");
}

// Neutral case:
// inst(8)=0 & inst(7:4)=00x1
//    = MoveDoubleVfpRegisterOp => MoveDoubleVfpRegisterOp {'constraints': ,
//     'pattern': 'cccc1100010otttttttt101000m1mmmm',
//     'rule': 'Vmov_two_S_Rule_A1'}
//
// Representaive case:
// C(8)=0 & op(7:4)=00x1
//    = MoveDoubleVfpRegisterOp => MoveDoubleVfpRegisterOp {constraints: ,
//     pattern: cccc1100010otttttttt101000m1mmmm,
//     rule: Vmov_two_S_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       MoveDoubleVfpRegisterOpTester_Case287_TestCase287) {
  MoveDoubleVfpRegisterOpTester_Case287 tester;
  tester.Test("cccc1100010otttttttt101000m1mmmm");
}

// Neutral case:
// inst(8)=1 & inst(7:4)=00x1
//    = MoveDoubleVfpRegisterOp => MoveDoubleVfpRegisterOp {'constraints': ,
//     'pattern': 'cccc1100010otttttttt101100m1mmmm',
//     'rule': 'Vmov_one_D_Rule_A1'}
//
// Representaive case:
// C(8)=1 & op(7:4)=00x1
//    = MoveDoubleVfpRegisterOp => MoveDoubleVfpRegisterOp {constraints: ,
//     pattern: cccc1100010otttttttt101100m1mmmm,
//     rule: Vmov_one_D_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       MoveDoubleVfpRegisterOpTester_Case288_TestCase288) {
  MoveDoubleVfpRegisterOpTester_Case288 tester;
  tester.Test("cccc1100010otttttttt101100m1mmmm");
}

// Neutral case:
// inst(20)=0 & inst(8)=0 & inst(23:21)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = MoveVfpRegisterOp => MoveVfpRegisterOp {'constraints': ,
//     'pattern': 'cccc11100000nnnntttt1010n0010000',
//     'rule': 'Vmov_Rule_330_A1_P648'}
//
// Representaive case:
// L(20)=0 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = MoveVfpRegisterOp => MoveVfpRegisterOp {constraints: ,
//     pattern: cccc11100000nnnntttt1010n0010000,
//     rule: Vmov_Rule_330_A1_P648}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpTester_Case289_TestCase289) {
  MoveVfpRegisterOpTester_Case289 tester;
  tester.Test("cccc11100000nnnntttt1010n0010000");
}

// Neutral case:
// inst(20)=0 & inst(8)=0 & inst(23:21)=111 & inst(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = VfpUsesRegOp => DontCareInstRdNotPc {'constraints': ,
//     'pattern': 'cccc111011100001tttt101000010000',
//     'rule': 'Vmsr_Rule_336_A1_P660'}
//
// Representative case:
// L(20)=0 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = VfpUsesRegOp => DontCareInstRdNotPc {constraints: ,
//     pattern: cccc111011100001tttt101000010000,
//     rule: Vmsr_Rule_336_A1_P660}
TEST_F(Arm32DecoderStateTests,
       VfpUsesRegOpTester_Case290_TestCase290) {
  VfpUsesRegOpTester_Case290 baseline_tester;
  NamedDontCareInstRdNotPc_Vmsr_Rule_336_A1_P660 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc111011100001tttt101000010000");
}

// Neutral case:
// inst(20)=0 & inst(8)=1 & inst(23:21)=0xx & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = MoveVfpRegisterOpWithTypeSel => MoveVfpRegisterOpWithTypeSel {'constraints': ,
//     'pattern': 'cccc11100ii0ddddtttt1011dii10000',
//     'rule': 'Vmov_Rule_328_A1_P644'}
//
// Representaive case:
// L(20)=0 & C(8)=1 & A(23:21)=0xx & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = MoveVfpRegisterOpWithTypeSel => MoveVfpRegisterOpWithTypeSel {constraints: ,
//     pattern: cccc11100ii0ddddtttt1011dii10000,
//     rule: Vmov_Rule_328_A1_P644}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpWithTypeSelTester_Case291_TestCase291) {
  MoveVfpRegisterOpWithTypeSelTester_Case291 tester;
  tester.Test("cccc11100ii0ddddtttt1011dii10000");
}

// Neutral case:
// inst(20)=0 & inst(8)=1 & inst(23:21)=1xx & inst(6:5)=0x & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = DuplicateToAdvSIMDRegisters => DuplicateToAdvSIMDRegisters {'constraints': ,
//     'pattern': 'cccc11101bq0ddddtttt1011d0e10000',
//     'rule': 'Vdup_Rule_303_A1_P594'}
//
// Representaive case:
// L(20)=0 & C(8)=1 & A(23:21)=1xx & B(6:5)=0x & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = DuplicateToAdvSIMDRegisters => DuplicateToAdvSIMDRegisters {constraints: ,
//     pattern: cccc11101bq0ddddtttt1011d0e10000,
//     rule: Vdup_Rule_303_A1_P594}
TEST_F(Arm32DecoderStateTests,
       DuplicateToAdvSIMDRegistersTester_Case292_TestCase292) {
  DuplicateToAdvSIMDRegistersTester_Case292 tester;
  tester.Test("cccc11101bq0ddddtttt1011d0e10000");
}

// Neutral case:
// inst(20)=1 & inst(8)=0 & inst(23:21)=000 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = MoveVfpRegisterOp => MoveVfpRegisterOp {'constraints': ,
//     'pattern': 'cccc11100001nnnntttt1010n0010000',
//     'rule': 'Vmov_Rule_330_A1_P648'}
//
// Representaive case:
// L(20)=1 & C(8)=0 & A(23:21)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxx00x0000
//    = MoveVfpRegisterOp => MoveVfpRegisterOp {constraints: ,
//     pattern: cccc11100001nnnntttt1010n0010000,
//     rule: Vmov_Rule_330_A1_P648}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpTester_Case293_TestCase293) {
  MoveVfpRegisterOpTester_Case293 tester;
  tester.Test("cccc11100001nnnntttt1010n0010000");
}

// Neutral case:
// inst(20)=1 & inst(8)=0 & inst(23:21)=111 & inst(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = VfpMrsOp => VfpMrsOp {'constraints': ,
//     'pattern': 'cccc111011110001tttt101000010000',
//     'rule': 'Vmrs_Rule_335_A1_P658'}
//
// Representaive case:
// L(20)=1 & C(8)=0 & A(23:21)=111 & $pattern(31:0)=xxxxxxxxxxxx0001xxxxxxxx000x0000
//    = VfpMrsOp => VfpMrsOp {constraints: ,
//     pattern: cccc111011110001tttt101000010000,
//     rule: Vmrs_Rule_335_A1_P658}
TEST_F(Arm32DecoderStateTests,
       VfpMrsOpTester_Case294_TestCase294) {
  VfpMrsOpTester_Case294 tester;
  tester.Test("cccc111011110001tttt101000010000");
}

// Neutral case:
// inst(20)=1 & inst(8)=1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = MoveVfpRegisterOpWithTypeSel => MoveVfpRegisterOpWithTypeSel {'constraints': ,
//     'pattern': 'cccc1110iii1nnnntttt1011nii10000',
//     'rule': 'Vmov_Rule_329_A1_P646'}
//
// Representaive case:
// L(20)=1 & C(8)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxxxx0000
//    = MoveVfpRegisterOpWithTypeSel => MoveVfpRegisterOpWithTypeSel {constraints: ,
//     pattern: cccc1110iii1nnnntttt1011nii10000,
//     rule: Vmov_Rule_329_A1_P646}
TEST_F(Arm32DecoderStateTests,
       MoveVfpRegisterOpWithTypeSelTester_Case295_TestCase295) {
  MoveVfpRegisterOpWithTypeSelTester_Case295 tester;
  tester.Test("cccc1110iii1nnnntttt1011nii10000");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000000 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = CondDecoder => DontCareInst {'constraints': ,
//     'pattern': 'cccc0011001000001111000000000000',
//     'rule': 'Nop_Rule_110_A1_P222'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000000 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = CondDecoder => DontCareInst {constraints: ,
//     pattern: cccc0011001000001111000000000000,
//     rule: Nop_Rule_110_A1_P222}
TEST_F(Arm32DecoderStateTests,
       CondDecoderTester_Case296_TestCase296) {
  CondDecoderTester_Case296 baseline_tester;
  NamedDontCareInst_Nop_Rule_110_A1_P222 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000000");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000001 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = CondDecoder => DontCareInst {'constraints': ,
//     'pattern': 'cccc0011001000001111000000000001',
//     'rule': 'Yield_Rule_413_A1_P812'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000001 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = CondDecoder => DontCareInst {constraints: ,
//     pattern: cccc0011001000001111000000000001,
//     rule: Yield_Rule_413_A1_P812}
TEST_F(Arm32DecoderStateTests,
       CondDecoderTester_Case297_TestCase297) {
  CondDecoderTester_Case297 baseline_tester;
  NamedDontCareInst_Yield_Rule_413_A1_P812 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000001");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000010 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc0011001000001111000000000010',
//     'rule': 'Wfe_Rule_411_A1_P808'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000010 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc0011001000001111000000000010,
//     rule: Wfe_Rule_411_A1_P808}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case298_TestCase298) {
  ForbiddenCondDecoderTester_Case298 baseline_tester;
  NamedForbidden_Wfe_Rule_411_A1_P808 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000010");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000011 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc0011001000001111000000000011',
//     'rule': 'Wfi_Rule_412_A1_P810'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000011 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc0011001000001111000000000011,
//     rule: Wfi_Rule_412_A1_P810}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case299_TestCase299) {
  ForbiddenCondDecoderTester_Case299 baseline_tester;
  NamedForbidden_Wfi_Rule_412_A1_P810 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000011");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=00000100 & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc0011001000001111000000000100',
//     'rule': 'Sev_Rule_158_A1_P316'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=00000100 & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc0011001000001111000000000100,
//     rule: Sev_Rule_158_A1_P316}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case300_TestCase300) {
  ForbiddenCondDecoderTester_Case300 baseline_tester;
  NamedForbidden_Sev_Rule_158_A1_P316 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001000001111000000000100");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0000 & inst(7:0)=1111xxxx & inst(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc001100100000111100001111iiii',
//     'rule': 'Dbg_Rule_40_A1_P88'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0000 & op2(7:0)=1111xxxx & $pattern(31:0)=xxxxxxxxxxxxxxxx11110000xxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc001100100000111100001111iiii,
//     rule: Dbg_Rule_40_A1_P88}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case301_TestCase301) {
  ForbiddenCondDecoderTester_Case301 baseline_tester;
  NamedForbidden_Dbg_Rule_40_A1_P88 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001100100000111100001111iiii");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=0100 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = MoveImmediate12ToApsr => DontCareInst {'constraints': ,
//     'pattern': 'cccc0011001001001111iiiiiiiiiiii',
//     'rule': 'Msr_Rule_103_A1_P208'}
//
// Representative case:
// op(22)=0 & op1(19:16)=0100 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = MoveImmediate12ToApsr => DontCareInst {constraints: ,
//     pattern: cccc0011001001001111iiiiiiiiiiii,
//     rule: Msr_Rule_103_A1_P208}
TEST_F(Arm32DecoderStateTests,
       MoveImmediate12ToApsrTester_Case302_TestCase302) {
  MoveImmediate12ToApsrTester_Case302 baseline_tester;
  NamedDontCareInst_Msr_Rule_103_A1_P208 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0011001001001111iiiiiiiiiiii");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=1x00 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = MoveImmediate12ToApsr => DontCareInst {'constraints': ,
//     'pattern': 'cccc001100101x001111iiiiiiiiiiii',
//     'rule': 'Msr_Rule_103_A1_P208'}
//
// Representative case:
// op(22)=0 & op1(19:16)=1x00 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = MoveImmediate12ToApsr => DontCareInst {constraints: ,
//     pattern: cccc001100101x001111iiiiiiiiiiii,
//     rule: Msr_Rule_103_A1_P208}
TEST_F(Arm32DecoderStateTests,
       MoveImmediate12ToApsrTester_Case303_TestCase303) {
  MoveImmediate12ToApsrTester_Case303 baseline_tester;
  NamedDontCareInst_Msr_Rule_103_A1_P208 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc001100101x001111iiiiiiiiiiii");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=xx01 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc00110010ii011111iiiiiiiiiiii',
//     'rule': 'Msr_Rule_B6_1_6_A1_PB6_12'}
//
// Representative case:
// op(22)=0 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc00110010ii011111iiiiiiiiiiii,
//     rule: Msr_Rule_B6_1_6_A1_PB6_12}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case304_TestCase304) {
  ForbiddenCondDecoderTester_Case304 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110010ii011111iiiiiiiiiiii");
}

// Neutral case:
// inst(22)=0 & inst(19:16)=xx1x & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc00110010ii1i1111iiiiiiiiiiii',
//     'rule': 'Msr_Rule_B6_1_6_A1_PB6_12'}
//
// Representative case:
// op(22)=0 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc00110010ii1i1111iiiiiiiiiiii,
//     rule: Msr_Rule_B6_1_6_A1_PB6_12}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case305_TestCase305) {
  ForbiddenCondDecoderTester_Case305 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110010ii1i1111iiiiiiiiiiii");
}

// Neutral case:
// inst(22)=1 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc00110110iiii1111iiiiiiiiiiii',
//     'rule': 'Msr_Rule_B6_1_6_A1_PB6_12'}
//
// Representative case:
// op(22)=1 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc00110110iiii1111iiiiiiiiiiii,
//     rule: Msr_Rule_B6_1_6_A1_PB6_12}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case306_TestCase306) {
  ForbiddenCondDecoderTester_Case306 baseline_tester;
  NamedForbidden_Msr_Rule_B6_1_6_A1_PB6_12 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110110iiii1111iiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0x010
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc0100u010nnnnttttiiiiiiiiiiii',
//     'rule': 'Strt_Rule_A1'}
//
// Representative case:
// A(25)=0 & op1(24:20)=0x010
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc0100u010nnnnttttiiiiiiiiiiii,
//     rule: Strt_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case307_TestCase307) {
  ForbiddenCondDecoderTester_Case307 baseline_tester;
  NamedForbidden_Strt_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0100u010nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0x011
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc0100u011nnnnttttiiiiiiiiiiii',
//     'rule': 'Ldrt_Rule_A1'}
//
// Representative case:
// A(25)=0 & op1(24:20)=0x011
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc0100u011nnnnttttiiiiiiiiiiii,
//     rule: Ldrt_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case308_TestCase308) {
  ForbiddenCondDecoderTester_Case308 baseline_tester;
  NamedForbidden_Ldrt_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0100u011nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0x110
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc0100u110nnnnttttiiiiiiiiiiii',
//     'rule': 'Strtb_Rule_A1'}
//
// Representative case:
// A(25)=0 & op1(24:20)=0x110
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc0100u110nnnnttttiiiiiiiiiiii,
//     rule: Strtb_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case309_TestCase309) {
  ForbiddenCondDecoderTester_Case309 baseline_tester;
  NamedForbidden_Strtb_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0100u110nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0x111
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc0100u111nnnnttttiiiiiiiiiiii',
//     'rule': 'Ldrtb_Rule_A1'}
//
// Representative case:
// A(25)=0 & op1(24:20)=0x111
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc0100u111nnnnttttiiiiiiiiiiii,
//     rule: Ldrtb_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case310_TestCase310) {
  ForbiddenCondDecoderTester_Case310 baseline_tester;
  NamedForbidden_Ldrtb_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0100u111nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=~1111 & inst(24:20)=~0x011
//    = Load2RegisterImm12Op => LoadBasedImmedMemory {'constraints': ,
//     'pattern': 'cccc010pu0w1nnnnttttiiiiiiiiiiii',
//     'rule': 'Ldr_Rule_58_A1_P120',
//     'safety': ["'NotRnIsPc'"]}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x011
//    = Load2RegisterImm12Op => LoadBasedImmedMemory {constraints: ,
//     pattern: cccc010pu0w1nnnnttttiiiiiiiiiiii,
//     rule: Ldr_Rule_58_A1_P120,
//     safety: ['NotRnIsPc']}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_Case311_TestCase311) {
  Load2RegisterImm12OpTester_Case311 baseline_tester;
  NamedLoadBasedImmedMemory_Ldr_Rule_58_A1_P120 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pu0w1nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=1111 & inst(24:20)=~0x011 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = Load2RegisterImm12Op => LoadBasedImmedMemory {'constraints': ,
//     'pattern': 'cccc0101u0011111ttttiiiiiiiiiiii',
//     'rule': 'Ldr_Rule_59_A1_P122'}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x011 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = Load2RegisterImm12Op => LoadBasedImmedMemory {constraints: ,
//     pattern: cccc0101u0011111ttttiiiiiiiiiiii,
//     rule: Ldr_Rule_59_A1_P122}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_Case312_TestCase312) {
  Load2RegisterImm12OpTester_Case312 baseline_tester;
  NamedLoadBasedImmedMemory_Ldr_Rule_59_A1_P122 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0101u0011111ttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x0 & inst(24:20)=~0x110
//    = Store2RegisterImm12Op => StoreBasedImmedMemory {'constraints': ,
//     'pattern': 'cccc010pu1w0nnnnttttiiiiiiiiiiii',
//     'rule': 'Strb_Rule_197_A1_P390'}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx1x0 & op1_repeated(24:20)=~0x110
//    = Store2RegisterImm12Op => StoreBasedImmedMemory {constraints: ,
//     pattern: cccc010pu1w0nnnnttttiiiiiiiiiiii,
//     rule: Strb_Rule_197_A1_P390}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm12OpTester_Case313_TestCase313) {
  Store2RegisterImm12OpTester_Case313 baseline_tester;
  NamedStoreBasedImmedMemory_Strb_Rule_197_A1_P390 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pu1w0nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x1 & inst(19:16)=~1111 & inst(24:20)=~0x111
//    = Load2RegisterImm12Op => LoadBasedImmedMemory {'constraints': ,
//     'pattern': 'cccc010pu1w1nnnnttttiiiiiiiiiiii',
//     'rule': 'Ldrb_Rule_62_A1_P128',
//     'safety': ["'NotRnIsPc'"]}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x111
//    = Load2RegisterImm12Op => LoadBasedImmedMemory {constraints: ,
//     pattern: cccc010pu1w1nnnnttttiiiiiiiiiiii,
//     rule: Ldrb_Rule_62_A1_P128,
//     safety: ['NotRnIsPc']}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_Case314_TestCase314) {
  Load2RegisterImm12OpTester_Case314 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrb_Rule_62_A1_P128 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pu1w1nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(24:20)=~0x111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = Load2RegisterImm12Op => LoadBasedImmedMemory {'constraints': ,
//     'pattern': 'cccc0101u1011111ttttiiiiiiiiiiii',
//     'rule': 'Ldrb_Rule_63_A1_P130'}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = Load2RegisterImm12Op => LoadBasedImmedMemory {constraints: ,
//     pattern: cccc0101u1011111ttttiiiiiiiiiiii,
//     rule: Ldrb_Rule_63_A1_P130}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm12OpTester_Case315_TestCase315) {
  Load2RegisterImm12OpTester_Case315 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrb_Rule_63_A1_P130 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0101u1011111ttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0xx1x & inst(7:4)=1011
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc0000xx1xxxxxxxxxxxxx1011xxxx',
//     'rule': 'extra_load_store_instructions_unpriviledged'}
//
// Representative case:
// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=1011
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc0000xx1xxxxxxxxxxxxx1011xxxx,
//     rule: extra_load_store_instructions_unpriviledged}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case316_TestCase316) {
  ForbiddenCondDecoderTester_Case316 baseline_tester;
  NamedForbidden_extra_load_store_instructions_unpriviledged actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000xx1xxxxxxxxxxxxx1011xxxx");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=0xx1x & inst(7:4)=11x1
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc0000xx1xxxxxxxxxxxxx11x1xxxx',
//     'rule': 'extra_load_store_instructions_unpriviledged'}
//
// Representative case:
// op(25)=0 & op1(24:20)=0xx1x & op2(7:4)=11x1
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc0000xx1xxxxxxxxxxxxx11x1xxxx,
//     rule: extra_load_store_instructions_unpriviledged}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case317_TestCase317) {
  ForbiddenCondDecoderTester_Case317 baseline_tester;
  NamedForbidden_extra_load_store_instructions_unpriviledged actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000xx1xxxxxxxxxxxxx11x1xxxx");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=10000
//    = Unary1RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc00110000iiiiddddIIIIIIIIIIII',
//     'rule': 'Mov_Rule_96_A2_P194',
//     'safety': ['inst(15:12)=1111 => UNPREDICTABLE']}
//
// Representative case:
// op(25)=1 & op1(24:20)=10000
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc00110000iiiiddddIIIIIIIIIIII,
//     rule: Mov_Rule_96_A2_P194,
//     safety: [Rd(15:12)=1111 => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Case318_TestCase318) {
  Unary1RegisterImmediateOpTester_Case318 baseline_tester;
  NamedDefs12To15_Mov_Rule_96_A2_P194 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110000iiiiddddIIIIIIIIIIII");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=10100
//    = Unary1RegisterImmediateOp => Defs12To15 {'constraints': ,
//     'pattern': 'cccc00110100iiiiddddIIIIIIIIIIII',
//     'rule': 'Mov_Rule_99_A1_P200',
//     'safety': ['inst(15:12)=1111 => UNPREDICTABLE']}
//
// Representative case:
// op(25)=1 & op1(24:20)=10100
//    = Unary1RegisterImmediateOp => Defs12To15 {constraints: ,
//     pattern: cccc00110100iiiiddddIIIIIIIIIIII,
//     rule: Mov_Rule_99_A1_P200,
//     safety: [Rd(15:12)=1111 => UNPREDICTABLE]}
TEST_F(Arm32DecoderStateTests,
       Unary1RegisterImmediateOpTester_Case319_TestCase319) {
  Unary1RegisterImmediateOpTester_Case319 baseline_tester;
  NamedDefs12To15_Mov_Rule_99_A1_P200 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00110100iiiiddddIIIIIIIIIIII");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=0x010 & inst(4)=0
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc0110u010nnnnttttiiiiitt0mmmm',
//     'rule': 'Strt_Rule_A2'}
//
// Representative case:
// A(25)=1 & op1(24:20)=0x010 & B(4)=0
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc0110u010nnnnttttiiiiitt0mmmm,
//     rule: Strt_Rule_A2}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case320_TestCase320) {
  ForbiddenCondDecoderTester_Case320 baseline_tester;
  NamedForbidden_Strt_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110u010nnnnttttiiiiitt0mmmm");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=0x011 & inst(4)=0
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc0110u011nnnnttttiiiiitt0mmmm',
//     'rule': 'Ldrt_Rule_A2'}
//
// Representative case:
// A(25)=1 & op1(24:20)=0x011 & B(4)=0
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc0110u011nnnnttttiiiiitt0mmmm,
//     rule: Ldrt_Rule_A2}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case321_TestCase321) {
  ForbiddenCondDecoderTester_Case321 baseline_tester;
  NamedForbidden_Ldrt_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110u011nnnnttttiiiiitt0mmmm");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=0x110 & inst(4)=0
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc0110u110nnnnttttiiiiitt0mmmm',
//     'rule': 'Strtb_Rule_A2'}
//
// Representative case:
// A(25)=1 & op1(24:20)=0x110 & B(4)=0
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc0110u110nnnnttttiiiiitt0mmmm,
//     rule: Strtb_Rule_A2}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case322_TestCase322) {
  ForbiddenCondDecoderTester_Case322 baseline_tester;
  NamedForbidden_Strtb_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110u110nnnnttttiiiiitt0mmmm");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=0x111 & inst(4)=0
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc0110u111nnnnttttiiiiitt0mmmm',
//     'rule': 'Ldrtb_Rule_A2'}
//
// Representative case:
// A(25)=1 & op1(24:20)=0x111 & B(4)=0
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc0110u111nnnnttttiiiiitt0mmmm,
//     rule: Ldrtb_Rule_A2}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case323_TestCase323) {
  ForbiddenCondDecoderTester_Case323 baseline_tester;
  NamedForbidden_Ldrtb_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0110u111nnnnttttiiiiitt0mmmm");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=xx0x0 & inst(4)=0 & inst(24:20)=~0x010
//    = Store3RegisterImm5Op => StoreBasedOffsetMemory {'constraints': ,
//     'pattern': 'cccc011pd0w0nnnnttttiiiiitt0mmmm',
//     'rule': 'Str_Rule_195_A1_P386'}
//
// Representative case:
// A(25)=1 & op1(24:20)=xx0x0 & B(4)=0 & op1_repeated(24:20)=~0x010
//    = Store3RegisterImm5Op => StoreBasedOffsetMemory {constraints: ,
//     pattern: cccc011pd0w0nnnnttttiiiiitt0mmmm,
//     rule: Str_Rule_195_A1_P386}
TEST_F(Arm32DecoderStateTests,
       Store3RegisterImm5OpTester_Case324_TestCase324) {
  Store3RegisterImm5OpTester_Case324 baseline_tester;
  NamedStoreBasedOffsetMemory_Str_Rule_195_A1_P386 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pd0w0nnnnttttiiiiitt0mmmm");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=xx0x1 & inst(4)=0 & inst(24:20)=~0x011
//    = Load3RegisterImm5Op => LoadBasedOffsetMemory {'constraints': ,
//     'pattern': 'cccc011pu0w1nnnnttttiiiiitt0mmmm',
//     'rule': 'Ldr_Rule_60_A1_P124'}
//
// Representative case:
// A(25)=1 & op1(24:20)=xx0x1 & B(4)=0 & op1_repeated(24:20)=~0x011
//    = Load3RegisterImm5Op => LoadBasedOffsetMemory {constraints: ,
//     pattern: cccc011pu0w1nnnnttttiiiiitt0mmmm,
//     rule: Ldr_Rule_60_A1_P124}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterImm5OpTester_Case325_TestCase325) {
  Load3RegisterImm5OpTester_Case325 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldr_Rule_60_A1_P124 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pu0w1nnnnttttiiiiitt0mmmm");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=xx1x0 & inst(4)=0 & inst(24:20)=~0x110
//    = Store3RegisterImm5Op => StoreBasedOffsetMemory {'constraints': ,
//     'pattern': 'cccc011pu1w0nnnnttttiiiiitt0mmmm',
//     'rule': 'Strb_Rule_198_A1_P392'}
//
// Representative case:
// A(25)=1 & op1(24:20)=xx1x0 & B(4)=0 & op1_repeated(24:20)=~0x110
//    = Store3RegisterImm5Op => StoreBasedOffsetMemory {constraints: ,
//     pattern: cccc011pu1w0nnnnttttiiiiitt0mmmm,
//     rule: Strb_Rule_198_A1_P392}
TEST_F(Arm32DecoderStateTests,
       Store3RegisterImm5OpTester_Case326_TestCase326) {
  Store3RegisterImm5OpTester_Case326 baseline_tester;
  NamedStoreBasedOffsetMemory_Strb_Rule_198_A1_P392 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pu1w0nnnnttttiiiiitt0mmmm");
}

// Neutral case:
// inst(25)=1 & inst(24:20)=xx1x1 & inst(4)=0 & inst(24:20)=~0x111
//    = Load3RegisterImm5Op => LoadBasedOffsetMemory {'constraints': ,
//     'pattern': 'cccc011pu1w1nnnnttttiiiiitt0mmmm',
//     'rule': 'Ldrb_Rule_64_A1_P132'}
//
// Representative case:
// A(25)=1 & op1(24:20)=xx1x1 & B(4)=0 & op1_repeated(24:20)=~0x111
//    = Load3RegisterImm5Op => LoadBasedOffsetMemory {constraints: ,
//     pattern: cccc011pu1w1nnnnttttiiiiitt0mmmm,
//     rule: Ldrb_Rule_64_A1_P132}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterImm5OpTester_Case327_TestCase327) {
  Load3RegisterImm5OpTester_Case327 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrb_Rule_64_A1_P132 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pu1w1nnnnttttiiiiitt0mmmm");
}

// Neutral case:
// 
//    = Store2RegisterImm12Op => StoreBasedImmedMemory {'constraints': & ~cccc010100101101tttt000000000100 ,
//     'pattern': 'cccc010pu0w0nnnnttttiiiiiiiiiiii',
//     'rule': 'Str_Rule_194_A1_P384'}
//
// Representative case:
// 
//    = Store2RegisterImm12Op => StoreBasedImmedMemory {constraints: & ~cccc010100101101tttt000000000100 ,
//     pattern: cccc010pu0w0nnnnttttiiiiiiiiiiii,
//     rule: Str_Rule_194_A1_P384}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm12OpTester_Case328_TestCase328) {
  Store2RegisterImm12OpTester_Case328 baseline_tester;
  NamedStoreBasedImmedMemory_Str_Rule_194_A1_P384 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc010pu0w0nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25:20)=00000x
//    = UndefinedCondDecoder => Undefined {'constraints': ,
//     'pattern': 'cccc1100000xnnnnxxxxccccxxxoxxxx',
//     'rule': 'Undefined_A5_6'}
//
// Representative case:
// op1(25:20)=00000x
//    = UndefinedCondDecoder => Undefined {constraints: ,
//     pattern: cccc1100000xnnnnxxxxccccxxxoxxxx,
//     rule: Undefined_A5_6}
TEST_F(Arm32DecoderStateTests,
       UndefinedCondDecoderTester_Case329_TestCase329) {
  UndefinedCondDecoderTester_Case329 baseline_tester;
  NamedUndefined_Undefined_A5_6 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1100000xnnnnxxxxccccxxxoxxxx");
}

// Neutral case:
// inst(25:20)=11xxxx
//    = ForbiddenCondDecoder => Forbidden {'constraints': ,
//     'pattern': 'cccc1111iiiiiiiiiiiiiiiiiiiiiiii',
//     'rule': 'Svc_Rule_A1'}
//
// Representative case:
// op1(25:20)=11xxxx
//    = ForbiddenCondDecoder => Forbidden {constraints: ,
//     pattern: cccc1111iiiiiiiiiiiiiiiiiiiiiiii,
//     rule: Svc_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case330_TestCase330) {
  ForbiddenCondDecoderTester_Case330 baseline_tester;
  NamedForbidden_Svc_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc1111iiiiiiiiiiiiiiiiiiiiiiii");
}

// Neutral case:
// inst(7:6)=x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
//    = CondVfpOp => VfpOp {'constraints': ,
//     'pattern': 'cccc11101d11iiiidddd101s0000iiii',
//     'rule': 'Vmov_Rule_326_A2_P640'}
//
// Representative case:
// opc3(7:6)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
//    = CondVfpOp => VfpOp {constraints: ,
//     pattern: cccc11101d11iiiidddd101s0000iiii,
//     rule: Vmov_Rule_326_A2_P640}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case331_TestCase331) {
  CondVfpOpTester_Case331 baseline_tester;
  NamedVfpOp_Vmov_Rule_326_A2_P640 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11iiiidddd101s0000iiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
