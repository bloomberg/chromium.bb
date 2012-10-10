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
// inst(26:20)=0010000 & inst(7:4)=0000 & inst(19:16)=xxx1 & inst(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(26:20)=0010000 & op2(7:4)=0000 & Rn(19:16)=xxx1 & $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
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
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(26:20)=0010000 & op2(7:4)=xx0x & Rn(19:16)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
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
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(26:20)=1010011
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: }
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
  if ((inst.Bits() & 0x07F00000) != 0x05300000 /* op1(26:20)=~1010011 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0000
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0000
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: }
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
  if ((inst.Bits() & 0x07F00000) != 0x05700000 /* op1(26:20)=~1010111 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x00000000 /* op2(7:4)=~0000 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0001 & inst(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0001 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
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
  if ((inst.Bits() & 0x07F00000) != 0x05700000 /* op1(26:20)=~1010111 */) return false;
  if ((inst.Bits() & 0x000000F0) != 0x00000010 /* op2(7:4)=~0001 */) return false;
  if ((inst.Bits() & 0x000FFF0F) != 0x000FF00F /* $pattern(31:0)=~xxxxxxxxxxxx111111110000xxxx1111 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0100 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {baseline: 'DataBarrier',
//       constraints: }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0100 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {baseline: DataBarrier,
//       constraints: }
class DataBarrierTesterCase5
    : public DataBarrierTester {
 public:
  DataBarrierTesterCase5(const NamedClassDecoder& decoder)
    : DataBarrierTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool DataBarrierTesterCase5
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
//    = {baseline: 'DataBarrier',
//       constraints: }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0101 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {baseline: DataBarrier,
//       constraints: }
class DataBarrierTesterCase6
    : public DataBarrierTester {
 public:
  DataBarrierTesterCase6(const NamedClassDecoder& decoder)
    : DataBarrierTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool DataBarrierTesterCase6
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
//    = {baseline: 'InstructionBarrier',
//       constraints: }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0110 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {baseline: InstructionBarrier,
//       constraints: }
class InstructionBarrierTesterCase7
    : public InstructionBarrierTester {
 public:
  InstructionBarrierTesterCase7(const NamedClassDecoder& decoder)
    : InstructionBarrierTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool InstructionBarrierTesterCase7
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
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0111
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase8
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase8(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase8
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
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=001x
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase9
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase9(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase9
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
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=1xxx
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: }
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
  if ((inst.Bits() & 0x00000080) != 0x00000080 /* op2(7:4)=~1xxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=100x001
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(26:20)=100x001
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
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
  if ((inst.Bits() & 0x07700000) != 0x04100000 /* op1(26:20)=~100x001 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=100x101 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'PreloadRegisterImm12Op',
//       constraints: }
//
// Representaive case:
// op1(26:20)=100x101 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: PreloadRegisterImm12Op,
//       constraints: }
class PreloadRegisterImm12OpTesterCase12
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterCase12(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterCase12
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
//    = {baseline: 'PreloadRegisterImm12Op',
//       constraints: & inst(31:0)=~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx }
//
// Representaive case:
// op1(26:20)=101x001 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: PreloadRegisterImm12Op,
//       constraints: & constraint(31:0)=~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx }
class PreloadRegisterImm12OpTesterCase13
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterCase13(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterCase13
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
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(26:20)=101x001 & Rn(19:16)=1111
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase14
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase14(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase14
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
//    = {baseline: 'PreloadRegisterImm12Op',
//       constraints: & inst(31:0)=~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx }
//
// Representaive case:
// op1(26:20)=101x101 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: PreloadRegisterImm12Op,
//       constraints: & constraint(31:0)=~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx }
class PreloadRegisterImm12OpTesterCase15
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterCase15(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterCase15
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
//    = {baseline: 'PreloadRegisterImm12Op',
//       constraints: }
//
// Representaive case:
// op1(26:20)=101x101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: PreloadRegisterImm12Op,
//       constraints: }
class PreloadRegisterImm12OpTesterCase16
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterCase16(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterCase16
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
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(26:20)=110x001 & op2(7:4)=xxx0
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase17
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase17(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase17
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
//    = {baseline: 'PreloadRegisterPairOp',
//       constraints: }
//
// Representaive case:
// op1(26:20)=110x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: PreloadRegisterPairOp,
//       constraints: }
class PreloadRegisterPairOpTesterCase18
    : public PreloadRegisterPairOpTester {
 public:
  PreloadRegisterPairOpTesterCase18(const NamedClassDecoder& decoder)
    : PreloadRegisterPairOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterPairOpTesterCase18
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
//    = {baseline: 'PreloadRegisterPairOpWAndRnNotPc',
//       constraints: }
//
// Representaive case:
// op1(26:20)=111x001 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: PreloadRegisterPairOpWAndRnNotPc,
//       constraints: }
class PreloadRegisterPairOpWAndRnNotPcTesterCase19
    : public PreloadRegisterPairOpWAndRnNotPcTester {
 public:
  PreloadRegisterPairOpWAndRnNotPcTesterCase19(const NamedClassDecoder& decoder)
    : PreloadRegisterPairOpWAndRnNotPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterPairOpWAndRnNotPcTesterCase19
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
//    = {baseline: 'PreloadRegisterPairOpWAndRnNotPc',
//       constraints: }
//
// Representaive case:
// op1(26:20)=111x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: PreloadRegisterPairOpWAndRnNotPc,
//       constraints: }
class PreloadRegisterPairOpWAndRnNotPcTesterCase20
    : public PreloadRegisterPairOpWAndRnNotPcTester {
 public:
  PreloadRegisterPairOpWAndRnNotPcTesterCase20(const NamedClassDecoder& decoder)
    : PreloadRegisterPairOpWAndRnNotPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool PreloadRegisterPairOpWAndRnNotPcTesterCase20
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
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(26:20)=1011x11
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase21
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase21(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase21
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
// inst(26:20)=100xx11
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(26:20)=100xx11
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: }
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
  if ((inst.Bits() & 0x07300000) != 0x04300000 /* op1(26:20)=~100xx11 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=11xxx11 & inst(7:4)=xxx0
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: }
//
// Representaive case:
// op1(26:20)=11xxx11 & op2(7:4)=xxx0
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: }
class UnsafeUncondDecoderTesterCase23
    : public UnsafeUncondDecoderTester {
 public:
  UnsafeUncondDecoderTesterCase23(const NamedClassDecoder& decoder)
    : UnsafeUncondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeUncondDecoderTesterCase23
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

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(26:20)=0010000 & inst(7:4)=0000 & inst(19:16)=xxx1 & inst(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Setend_Rule_157_P314'}
//
// Representative case:
// op1(26:20)=0010000 & op2(7:4)=0000 & Rn(19:16)=xxx1 & $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Setend_Rule_157_P314}
class ForbiddenUncondDecoderTester_Case0
    : public UnsafeUncondDecoderTesterCase0 {
 public:
  ForbiddenUncondDecoderTester_Case0()
    : UnsafeUncondDecoderTesterCase0(
      state_.ForbiddenUncondDecoder_Setend_Rule_157_P314_instance_)
  {}
};

// Neutral case:
// inst(26:20)=0010000 & inst(7:4)=xx0x & inst(19:16)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Cps_Rule_b6_1_1_A1_B6_3'}
//
// Representative case:
// op1(26:20)=0010000 & op2(7:4)=xx0x & Rn(19:16)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Cps_Rule_b6_1_1_A1_B6_3}
class ForbiddenUncondDecoderTester_Case1
    : public UnsafeUncondDecoderTesterCase1 {
 public:
  ForbiddenUncondDecoderTester_Case1()
    : UnsafeUncondDecoderTesterCase1(
      state_.ForbiddenUncondDecoder_Cps_Rule_b6_1_1_A1_B6_3_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010011
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010011
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case2
    : public UnsafeUncondDecoderTesterCase2 {
 public:
  UnpredictableUncondDecoderTester_Case2()
    : UnsafeUncondDecoderTesterCase2(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0000
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0000
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case3
    : public UnsafeUncondDecoderTesterCase3 {
 public:
  UnpredictableUncondDecoderTester_Case3()
    : UnsafeUncondDecoderTesterCase3(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0001 & inst(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Clrex_Rule_30_A1_P70'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0001 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Clrex_Rule_30_A1_P70}
class ForbiddenUncondDecoderTester_Case4
    : public UnsafeUncondDecoderTesterCase4 {
 public:
  ForbiddenUncondDecoderTester_Case4()
    : UnsafeUncondDecoderTesterCase4(
      state_.ForbiddenUncondDecoder_Clrex_Rule_30_A1_P70_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0100 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {baseline: 'DataBarrier',
//       constraints: ,
//       rule: 'Dsb_Rule_42_A1_P92'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0100 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {baseline: DataBarrier,
//       constraints: ,
//       rule: Dsb_Rule_42_A1_P92}
class DataBarrierTester_Case5
    : public DataBarrierTesterCase5 {
 public:
  DataBarrierTester_Case5()
    : DataBarrierTesterCase5(
      state_.DataBarrier_Dsb_Rule_42_A1_P92_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0101 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {baseline: 'DataBarrier',
//       constraints: ,
//       rule: 'Dmb_Rule_41_A1_P90'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0101 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {baseline: DataBarrier,
//       constraints: ,
//       rule: Dmb_Rule_41_A1_P90}
class DataBarrierTester_Case6
    : public DataBarrierTesterCase6 {
 public:
  DataBarrierTester_Case6()
    : DataBarrierTesterCase6(
      state_.DataBarrier_Dmb_Rule_41_A1_P90_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0110 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {baseline: 'InstructionBarrier',
//       constraints: ,
//       rule: 'Isb_Rule_49_A1_P102'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0110 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {baseline: InstructionBarrier,
//       constraints: ,
//       rule: Isb_Rule_49_A1_P102}
class InstructionBarrierTester_Case7
    : public InstructionBarrierTesterCase7 {
 public:
  InstructionBarrierTester_Case7()
    : InstructionBarrierTesterCase7(
      state_.InstructionBarrier_Isb_Rule_49_A1_P102_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0111
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0111
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case8
    : public UnsafeUncondDecoderTesterCase8 {
 public:
  UnpredictableUncondDecoderTester_Case8()
    : UnsafeUncondDecoderTesterCase8(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=001x
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=001x
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case9
    : public UnsafeUncondDecoderTesterCase9 {
 public:
  UnpredictableUncondDecoderTester_Case9()
    : UnsafeUncondDecoderTesterCase9(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=1xxx
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=1xxx
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case10
    : public UnsafeUncondDecoderTesterCase10 {
 public:
  UnpredictableUncondDecoderTester_Case10()
    : UnsafeUncondDecoderTesterCase10(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(26:20)=100x001
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Unallocated_hints'}
//
// Representative case:
// op1(26:20)=100x001
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Unallocated_hints}
class ForbiddenUncondDecoderTester_Case11
    : public UnsafeUncondDecoderTesterCase11 {
 public:
  ForbiddenUncondDecoderTester_Case11()
    : UnsafeUncondDecoderTesterCase11(
      state_.ForbiddenUncondDecoder_Unallocated_hints_instance_)
  {}
};

// Neutral case:
// inst(26:20)=100x101 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'PreloadRegisterImm12Op',
//       constraints: ,
//       rule: 'Pli_Rule_120_A1_P242'}
//
// Representative case:
// op1(26:20)=100x101 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       rule: Pli_Rule_120_A1_P242}
class PreloadRegisterImm12OpTester_Case12
    : public PreloadRegisterImm12OpTesterCase12 {
 public:
  PreloadRegisterImm12OpTester_Case12()
    : PreloadRegisterImm12OpTesterCase12(
      state_.PreloadRegisterImm12Op_Pli_Rule_120_A1_P242_instance_)
  {}
};

// Neutral case:
// inst(26:20)=101x001 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'PreloadRegisterImm12Op',
//       constraints: & inst(31:0)=~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//       rule: 'Pldw_Rule_117_A1_P236'}
//
// Representative case:
// op1(26:20)=101x001 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: PreloadRegisterImm12Op,
//       constraints: & constraint(31:0)=~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//       rule: Pldw_Rule_117_A1_P236}
class PreloadRegisterImm12OpTester_Case13
    : public PreloadRegisterImm12OpTesterCase13 {
 public:
  PreloadRegisterImm12OpTester_Case13()
    : PreloadRegisterImm12OpTesterCase13(
      state_.PreloadRegisterImm12Op_Pldw_Rule_117_A1_P236_instance_)
  {}
};

// Neutral case:
// inst(26:20)=101x001 & inst(19:16)=1111
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=101x001 & Rn(19:16)=1111
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case14
    : public UnsafeUncondDecoderTesterCase14 {
 public:
  UnpredictableUncondDecoderTester_Case14()
    : UnsafeUncondDecoderTesterCase14(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(26:20)=101x101 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'PreloadRegisterImm12Op',
//       constraints: & inst(31:0)=~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//       rule: 'Pld_Rule_117_A1_P236'}
//
// Representative case:
// op1(26:20)=101x101 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: PreloadRegisterImm12Op,
//       constraints: & constraint(31:0)=~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//       rule: Pld_Rule_117_A1_P236}
class PreloadRegisterImm12OpTester_Case15
    : public PreloadRegisterImm12OpTesterCase15 {
 public:
  PreloadRegisterImm12OpTester_Case15()
    : PreloadRegisterImm12OpTesterCase15(
      state_.PreloadRegisterImm12Op_Pld_Rule_117_A1_P236_instance_)
  {}
};

// Neutral case:
// inst(26:20)=101x101 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'PreloadRegisterImm12Op',
//       constraints: ,
//       rule: 'Pld_Rule_118_A1_P238'}
//
// Representative case:
// op1(26:20)=101x101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       rule: Pld_Rule_118_A1_P238}
class PreloadRegisterImm12OpTester_Case16
    : public PreloadRegisterImm12OpTesterCase16 {
 public:
  PreloadRegisterImm12OpTester_Case16()
    : PreloadRegisterImm12OpTesterCase16(
      state_.PreloadRegisterImm12Op_Pld_Rule_118_A1_P238_instance_)
  {}
};

// Neutral case:
// inst(26:20)=110x001 & inst(7:4)=xxx0
//    = {baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       rule: 'Unallocated_hints'}
//
// Representative case:
// op1(26:20)=110x001 & op2(7:4)=xxx0
//    = {baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       rule: Unallocated_hints}
class ForbiddenUncondDecoderTester_Case17
    : public UnsafeUncondDecoderTesterCase17 {
 public:
  ForbiddenUncondDecoderTester_Case17()
    : UnsafeUncondDecoderTesterCase17(
      state_.ForbiddenUncondDecoder_Unallocated_hints_instance_)
  {}
};

// Neutral case:
// inst(26:20)=110x101 & inst(7:4)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'PreloadRegisterPairOp',
//       constraints: ,
//       rule: 'Pli_Rule_121_A1_P244'}
//
// Representative case:
// op1(26:20)=110x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: PreloadRegisterPairOp,
//       constraints: ,
//       rule: Pli_Rule_121_A1_P244}
class PreloadRegisterPairOpTester_Case18
    : public PreloadRegisterPairOpTesterCase18 {
 public:
  PreloadRegisterPairOpTester_Case18()
    : PreloadRegisterPairOpTesterCase18(
      state_.PreloadRegisterPairOp_Pli_Rule_121_A1_P244_instance_)
  {}
};

// Neutral case:
// inst(26:20)=111x001 & inst(7:4)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'PreloadRegisterPairOpWAndRnNotPc',
//       constraints: ,
//       rule: 'Pldw_Rule_119_A1_P240'}
//
// Representative case:
// op1(26:20)=111x001 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: PreloadRegisterPairOpWAndRnNotPc,
//       constraints: ,
//       rule: Pldw_Rule_119_A1_P240}
class PreloadRegisterPairOpWAndRnNotPcTester_Case19
    : public PreloadRegisterPairOpWAndRnNotPcTesterCase19 {
 public:
  PreloadRegisterPairOpWAndRnNotPcTester_Case19()
    : PreloadRegisterPairOpWAndRnNotPcTesterCase19(
      state_.PreloadRegisterPairOpWAndRnNotPc_Pldw_Rule_119_A1_P240_instance_)
  {}
};

// Neutral case:
// inst(26:20)=111x101 & inst(7:4)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: 'PreloadRegisterPairOpWAndRnNotPc',
//       constraints: ,
//       rule: 'Pld_Rule_119_A1_P240'}
//
// Representative case:
// op1(26:20)=111x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {baseline: PreloadRegisterPairOpWAndRnNotPc,
//       constraints: ,
//       rule: Pld_Rule_119_A1_P240}
class PreloadRegisterPairOpWAndRnNotPcTester_Case20
    : public PreloadRegisterPairOpWAndRnNotPcTesterCase20 {
 public:
  PreloadRegisterPairOpWAndRnNotPcTester_Case20()
    : PreloadRegisterPairOpWAndRnNotPcTesterCase20(
      state_.PreloadRegisterPairOpWAndRnNotPc_Pld_Rule_119_A1_P240_instance_)
  {}
};

// Neutral case:
// inst(26:20)=1011x11
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1011x11
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case21
    : public UnsafeUncondDecoderTesterCase21 {
 public:
  UnpredictableUncondDecoderTester_Case21()
    : UnsafeUncondDecoderTesterCase21(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(26:20)=100xx11
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=100xx11
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case22
    : public UnsafeUncondDecoderTesterCase22 {
 public:
  UnpredictableUncondDecoderTester_Case22()
    : UnsafeUncondDecoderTesterCase22(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
  {}
};

// Neutral case:
// inst(26:20)=11xxx11 & inst(7:4)=xxx0
//    = {baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=11xxx11 & op2(7:4)=xxx0
//    = {baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case23
    : public UnsafeUncondDecoderTesterCase23 {
 public:
  UnpredictableUncondDecoderTester_Case23()
    : UnsafeUncondDecoderTesterCase23(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
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
// inst(26:20)=0010000 & inst(7:4)=0000 & inst(19:16)=xxx1 & inst(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '1111000100000001000000i000000000',
//       rule: 'Setend_Rule_157_P314'}
//
// Representative case:
// op1(26:20)=0010000 & op2(7:4)=0000 & Rn(19:16)=xxx1 & $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 1111000100000001000000i000000000,
//       rule: Setend_Rule_157_P314}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case0_TestCase0) {
  ForbiddenUncondDecoderTester_Case0 baseline_tester;
  NamedForbidden_Setend_Rule_157_P314 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111000100000001000000i000000000");
}

// Neutral case:
// inst(26:20)=0010000 & inst(7:4)=xx0x & inst(19:16)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '111100010000iii00000000iii0iiiii',
//       rule: 'Cps_Rule_b6_1_1_A1_B6_3'}
//
// Representative case:
// op1(26:20)=0010000 & op2(7:4)=xx0x & Rn(19:16)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 111100010000iii00000000iii0iiiii,
//       rule: Cps_Rule_b6_1_1_A1_B6_3}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case1_TestCase1) {
  ForbiddenUncondDecoderTester_Case1 baseline_tester;
  NamedForbidden_Cps_Rule_b6_1_1_A1_B6_3 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111100010000iii00000000iii0iiiii");
}

// Neutral case:
// inst(26:20)=1010011
//    = {actual: 'Unpredictable',
//       baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       pattern: '111101010011xxxxxxxxxxxxxxxxxxxx',
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010011
//    = {actual: Unpredictable,
//       baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       pattern: 111101010011xxxxxxxxxxxxxxxxxxxx,
//       rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case2_TestCase2) {
  UnpredictableUncondDecoderTester_Case2 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010011xxxxxxxxxxxxxxxxxxxx");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0000
//    = {actual: 'Unpredictable',
//       baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       pattern: '111101010111xxxxxxxxxxxx0000xxxx',
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0000
//    = {actual: Unpredictable,
//       baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       pattern: 111101010111xxxxxxxxxxxx0000xxxx,
//       rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case3_TestCase3) {
  UnpredictableUncondDecoderTester_Case3 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010111xxxxxxxxxxxx0000xxxx");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0001 & inst(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '11110101011111111111000000011111',
//       rule: 'Clrex_Rule_30_A1_P70'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0001 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 11110101011111111111000000011111,
//       rule: Clrex_Rule_30_A1_P70}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case4_TestCase4) {
  ForbiddenUncondDecoderTester_Case4 baseline_tester;
  NamedForbidden_Clrex_Rule_30_A1_P70 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110101011111111111000000011111");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0100 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {actual: 'DataBarrier',
//       baseline: 'DataBarrier',
//       constraints: ,
//       pattern: '1111010101111111111100000100xxxx',
//       rule: 'Dsb_Rule_42_A1_P92'}
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0100 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {actual: DataBarrier,
//       baseline: DataBarrier,
//       constraints: ,
//       pattern: 1111010101111111111100000100xxxx,
//       rule: Dsb_Rule_42_A1_P92}
TEST_F(Arm32DecoderStateTests,
       DataBarrierTester_Case5_TestCase5) {
  DataBarrierTester_Case5 tester;
  tester.Test("1111010101111111111100000100xxxx");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0101 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {actual: 'DataBarrier',
//       baseline: 'DataBarrier',
//       constraints: ,
//       pattern: '1111010101111111111100000101xxxx',
//       rule: 'Dmb_Rule_41_A1_P90'}
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0101 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {actual: DataBarrier,
//       baseline: DataBarrier,
//       constraints: ,
//       pattern: 1111010101111111111100000101xxxx,
//       rule: Dmb_Rule_41_A1_P90}
TEST_F(Arm32DecoderStateTests,
       DataBarrierTester_Case6_TestCase6) {
  DataBarrierTester_Case6 tester;
  tester.Test("1111010101111111111100000101xxxx");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0110 & inst(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {actual: 'InstructionBarrier',
//       baseline: 'InstructionBarrier',
//       constraints: ,
//       pattern: '1111010101111111111100000110xxxx',
//       rule: 'Isb_Rule_49_A1_P102'}
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0110 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {actual: InstructionBarrier,
//       baseline: InstructionBarrier,
//       constraints: ,
//       pattern: 1111010101111111111100000110xxxx,
//       rule: Isb_Rule_49_A1_P102}
TEST_F(Arm32DecoderStateTests,
       InstructionBarrierTester_Case7_TestCase7) {
  InstructionBarrierTester_Case7 tester;
  tester.Test("1111010101111111111100000110xxxx");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=0111
//    = {actual: 'Unpredictable',
//       baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       pattern: '111101010111xxxxxxxxxxxx0111xxxx',
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0111
//    = {actual: Unpredictable,
//       baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       pattern: 111101010111xxxxxxxxxxxx0111xxxx,
//       rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case8_TestCase8) {
  UnpredictableUncondDecoderTester_Case8 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010111xxxxxxxxxxxx0111xxxx");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=001x
//    = {actual: 'Unpredictable',
//       baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       pattern: '111101010111xxxxxxxxxxxx001xxxxx',
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=001x
//    = {actual: Unpredictable,
//       baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       pattern: 111101010111xxxxxxxxxxxx001xxxxx,
//       rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case9_TestCase9) {
  UnpredictableUncondDecoderTester_Case9 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010111xxxxxxxxxxxx001xxxxx");
}

// Neutral case:
// inst(26:20)=1010111 & inst(7:4)=1xxx
//    = {actual: 'Unpredictable',
//       baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       pattern: '111101010111xxxxxxxxxxxx1xxxxxxx',
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=1xxx
//    = {actual: Unpredictable,
//       baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       pattern: 111101010111xxxxxxxxxxxx1xxxxxxx,
//       rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case10_TestCase10) {
  UnpredictableUncondDecoderTester_Case10 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010111xxxxxxxxxxxx1xxxxxxx");
}

// Neutral case:
// inst(26:20)=100x001
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '11110100x001xxxxxxxxxxxxxxxxxxxx',
//       rule: 'Unallocated_hints'}
//
// Representative case:
// op1(26:20)=100x001
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 11110100x001xxxxxxxxxxxxxxxxxxxx,
//       rule: Unallocated_hints}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case11_TestCase11) {
  ForbiddenUncondDecoderTester_Case11 baseline_tester;
  NamedForbidden_Unallocated_hints actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110100x001xxxxxxxxxxxxxxxxxxxx");
}

// Neutral case:
// inst(26:20)=100x101 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'DontCareInst',
//       baseline: 'PreloadRegisterImm12Op',
//       constraints: ,
//       pattern: '11110100u101nnnn1111iiiiiiiiiiii',
//       rule: 'Pli_Rule_120_A1_P242'}
//
// Representative case:
// op1(26:20)=100x101 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: DontCareInst,
//       baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       pattern: 11110100u101nnnn1111iiiiiiiiiiii,
//       rule: Pli_Rule_120_A1_P242}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_Case12_TestCase12) {
  PreloadRegisterImm12OpTester_Case12 baseline_tester;
  NamedDontCareInst_Pli_Rule_120_A1_P242 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110100u101nnnn1111iiiiiiiiiiii");
}

// Neutral case:
// inst(26:20)=101x001 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'DontCareInst',
//       baseline: 'PreloadRegisterImm12Op',
//       constraints: & inst(31:0)=~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//       pattern: '11110101u001nnnn1111iiiiiiiiiiii',
//       rule: 'Pldw_Rule_117_A1_P236'}
//
// Representative case:
// op1(26:20)=101x001 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: DontCareInst,
//       baseline: PreloadRegisterImm12Op,
//       constraints: & constraint(31:0)=~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//       pattern: 11110101u001nnnn1111iiiiiiiiiiii,
//       rule: Pldw_Rule_117_A1_P236}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_Case13_TestCase13) {
  PreloadRegisterImm12OpTester_Case13 baseline_tester;
  NamedDontCareInst_Pldw_Rule_117_A1_P236 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110101u001nnnn1111iiiiiiiiiiii");
}

// Neutral case:
// inst(26:20)=101x001 & inst(19:16)=1111
//    = {actual: 'Unpredictable',
//       baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       pattern: '11110101x001xxxxxxxxxxxxxxxxxxxx',
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=101x001 & Rn(19:16)=1111
//    = {actual: Unpredictable,
//       baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       pattern: 11110101x001xxxxxxxxxxxxxxxxxxxx,
//       rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case14_TestCase14) {
  UnpredictableUncondDecoderTester_Case14 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110101x001xxxxxxxxxxxxxxxxxxxx");
}

// Neutral case:
// inst(26:20)=101x101 & inst(19:16)=~1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'DontCareInst',
//       baseline: 'PreloadRegisterImm12Op',
//       constraints: & inst(31:0)=~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//       pattern: '11110101u101nnnn1111iiiiiiiiiiii',
//       rule: 'Pld_Rule_117_A1_P236'}
//
// Representative case:
// op1(26:20)=101x101 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: DontCareInst,
//       baseline: PreloadRegisterImm12Op,
//       constraints: & constraint(31:0)=~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//       pattern: 11110101u101nnnn1111iiiiiiiiiiii,
//       rule: Pld_Rule_117_A1_P236}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_Case15_TestCase15) {
  PreloadRegisterImm12OpTester_Case15 baseline_tester;
  NamedDontCareInst_Pld_Rule_117_A1_P236 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110101u101nnnn1111iiiiiiiiiiii");
}

// Neutral case:
// inst(26:20)=101x101 & inst(19:16)=1111 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'DontCareInst',
//       baseline: 'PreloadRegisterImm12Op',
//       constraints: ,
//       pattern: '11110101u10111111111iiiiiiiiiiii',
//       rule: 'Pld_Rule_118_A1_P238'}
//
// Representative case:
// op1(26:20)=101x101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: DontCareInst,
//       baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       pattern: 11110101u10111111111iiiiiiiiiiii,
//       rule: Pld_Rule_118_A1_P238}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_Case16_TestCase16) {
  PreloadRegisterImm12OpTester_Case16 baseline_tester;
  NamedDontCareInst_Pld_Rule_118_A1_P238 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110101u10111111111iiiiiiiiiiii");
}

// Neutral case:
// inst(26:20)=110x001 & inst(7:4)=xxx0
//    = {actual: 'Forbidden',
//       baseline: 'ForbiddenUncondDecoder',
//       constraints: ,
//       pattern: '11110110x001xxxxxxxxxxxxxxx0xxxx',
//       rule: 'Unallocated_hints'}
//
// Representative case:
// op1(26:20)=110x001 & op2(7:4)=xxx0
//    = {actual: Forbidden,
//       baseline: ForbiddenUncondDecoder,
//       constraints: ,
//       pattern: 11110110x001xxxxxxxxxxxxxxx0xxxx,
//       rule: Unallocated_hints}
TEST_F(Arm32DecoderStateTests,
       ForbiddenUncondDecoderTester_Case17_TestCase17) {
  ForbiddenUncondDecoderTester_Case17 baseline_tester;
  NamedForbidden_Unallocated_hints actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110110x001xxxxxxxxxxxxxxx0xxxx");
}

// Neutral case:
// inst(26:20)=110x101 & inst(7:4)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'PreloadRegisterPairOp',
//       baseline: 'PreloadRegisterPairOp',
//       constraints: ,
//       pattern: '11110110u101nnnn1111iiiiitt0mmmm',
//       rule: 'Pli_Rule_121_A1_P244'}
//
// Representaive case:
// op1(26:20)=110x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: PreloadRegisterPairOp,
//       baseline: PreloadRegisterPairOp,
//       constraints: ,
//       pattern: 11110110u101nnnn1111iiiiitt0mmmm,
//       rule: Pli_Rule_121_A1_P244}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterPairOpTester_Case18_TestCase18) {
  PreloadRegisterPairOpTester_Case18 tester;
  tester.Test("11110110u101nnnn1111iiiiitt0mmmm");
}

// Neutral case:
// inst(26:20)=111x001 & inst(7:4)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'PreloadRegisterPairOpWAndRnNotPc',
//       baseline: 'PreloadRegisterPairOpWAndRnNotPc',
//       constraints: ,
//       pattern: '11110111u001nnnn1111iiiiitt0mmmm',
//       rule: 'Pldw_Rule_119_A1_P240'}
//
// Representaive case:
// op1(26:20)=111x001 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: PreloadRegisterPairOpWAndRnNotPc,
//       baseline: PreloadRegisterPairOpWAndRnNotPc,
//       constraints: ,
//       pattern: 11110111u001nnnn1111iiiiitt0mmmm,
//       rule: Pldw_Rule_119_A1_P240}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterPairOpWAndRnNotPcTester_Case19_TestCase19) {
  PreloadRegisterPairOpWAndRnNotPcTester_Case19 tester;
  tester.Test("11110111u001nnnn1111iiiiitt0mmmm");
}

// Neutral case:
// inst(26:20)=111x101 & inst(7:4)=xxx0 & inst(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: 'PreloadRegisterPairOpWAndRnNotPc',
//       baseline: 'PreloadRegisterPairOpWAndRnNotPc',
//       constraints: ,
//       pattern: '11110111u101nnnn1111iiiiitt0mmmm',
//       rule: 'Pld_Rule_119_A1_P240'}
//
// Representaive case:
// op1(26:20)=111x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {actual: PreloadRegisterPairOpWAndRnNotPc,
//       baseline: PreloadRegisterPairOpWAndRnNotPc,
//       constraints: ,
//       pattern: 11110111u101nnnn1111iiiiitt0mmmm,
//       rule: Pld_Rule_119_A1_P240}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterPairOpWAndRnNotPcTester_Case20_TestCase20) {
  PreloadRegisterPairOpWAndRnNotPcTester_Case20 tester;
  tester.Test("11110111u101nnnn1111iiiiitt0mmmm");
}

// Neutral case:
// inst(26:20)=1011x11
//    = {actual: 'Unpredictable',
//       baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       pattern: '111101011x11xxxxxxxxxxxxxxxxxxxx',
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1011x11
//    = {actual: Unpredictable,
//       baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       pattern: 111101011x11xxxxxxxxxxxxxxxxxxxx,
//       rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case21_TestCase21) {
  UnpredictableUncondDecoderTester_Case21 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101011x11xxxxxxxxxxxxxxxxxxxx");
}

// Neutral case:
// inst(26:20)=100xx11
//    = {actual: 'Unpredictable',
//       baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       pattern: '11110100xx11xxxxxxxxxxxxxxxxxxxx',
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=100xx11
//    = {actual: Unpredictable,
//       baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       pattern: 11110100xx11xxxxxxxxxxxxxxxxxxxx,
//       rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case22_TestCase22) {
  UnpredictableUncondDecoderTester_Case22 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110100xx11xxxxxxxxxxxxxxxxxxxx");
}

// Neutral case:
// inst(26:20)=11xxx11 & inst(7:4)=xxx0
//    = {actual: 'Unpredictable',
//       baseline: 'UnpredictableUncondDecoder',
//       constraints: ,
//       pattern: '1111011xxx11xxxxxxxxxxxxxxx0xxxx',
//       rule: 'Unpredictable'}
//
// Representative case:
// op1(26:20)=11xxx11 & op2(7:4)=xxx0
//    = {actual: Unpredictable,
//       baseline: UnpredictableUncondDecoder,
//       constraints: ,
//       pattern: 1111011xxx11xxxxxxxxxxxxxxx0xxxx,
//       rule: Unpredictable}
TEST_F(Arm32DecoderStateTests,
       UnpredictableUncondDecoderTester_Case23_TestCase23) {
  UnpredictableUncondDecoderTester_Case23 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111011xxx11xxxxxxxxxxxxxxx0xxxx");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
