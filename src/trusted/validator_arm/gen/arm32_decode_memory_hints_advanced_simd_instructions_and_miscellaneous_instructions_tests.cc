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
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=0010000 & op2(7:4)=0000 & Rn(19:16)=xxx1 & $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
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
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=1010011
//    = UnpredictableUncondDecoder {constraints: }
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
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0000
//    = UnpredictableUncondDecoder {constraints: }
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
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0001 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = ForbiddenUncondDecoder {constraints: }
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
//    = DataBarrier {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0100 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier {constraints: }
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
//    = DataBarrier {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0101 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier {constraints: }
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
//    = InstructionBarrier {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0110 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = InstructionBarrier {constraints: }
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
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=0111
//    = UnpredictableUncondDecoder {constraints: }
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
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=001x
//    = UnpredictableUncondDecoder {constraints: }
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
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=1010111 & op2(7:4)=1xxx
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
  if ((inst.Bits() & 0x00000080) != 0x00000080 /* op2(7:4)=~1xxx */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=100x001
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=100x001
//    = ForbiddenUncondDecoder {constraints: }
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
//    = PreloadRegisterImm12Op {'constraints': }
//
// Representaive case:
// op1(26:20)=100x101 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: }
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
//    = PreloadRegisterImm12Op {'constraints': & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx }
//
// Representaive case:
// op1(26:20)=101x001 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx }
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
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=101x001 & Rn(19:16)=1111
//    = UnpredictableUncondDecoder {constraints: }
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
//    = PreloadRegisterImm12Op {'constraints': & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx }
//
// Representaive case:
// op1(26:20)=101x101 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx }
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
//    = PreloadRegisterImm12Op {'constraints': }
//
// Representaive case:
// op1(26:20)=101x101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: }
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
//    = ForbiddenUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=110x001 & op2(7:4)=xxx0
//    = ForbiddenUncondDecoder {constraints: }
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
//    = PreloadRegisterPairOp {'constraints': }
//
// Representaive case:
// op1(26:20)=110x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOp {constraints: }
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
//    = PreloadRegisterPairOpWAndRnNotPc {'constraints': }
//
// Representaive case:
// op1(26:20)=111x001 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc {constraints: }
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
//    = PreloadRegisterPairOpWAndRnNotPc {'constraints': }
//
// Representaive case:
// op1(26:20)=111x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc {constraints: }
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
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=1011x11
//    = UnpredictableUncondDecoder {constraints: }
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
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=100xx11
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
  if ((inst.Bits() & 0x07300000) != 0x04300000 /* op1(26:20)=~100xx11 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeUncondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(26:20)=11xxx11 & inst(7:4)=xxx0
//    = UnpredictableUncondDecoder {'constraints': }
//
// Representaive case:
// op1(26:20)=11xxx11 & op2(7:4)=xxx0
//    = UnpredictableUncondDecoder {constraints: }
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

// Neutral case:
// inst(6:4)=000 & inst(9)=0 & inst(22:21)=01 & inst(19:16)=xx00 & inst(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = Unary1RegisterUse {'constraints': }
//
// Representaive case:
// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = Unary1RegisterUse {constraints: }
class Unary1RegisterUseTesterCase24
    : public Unary1RegisterUseTester {
 public:
  Unary1RegisterUseTesterCase24(const NamedClassDecoder& decoder)
    : Unary1RegisterUseTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterUseTesterCase24
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
class UnsafeCondDecoderTesterCase25
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase25(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase25
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
class UnsafeCondDecoderTesterCase26
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase26(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase26
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
class UnsafeCondDecoderTesterCase27
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase27(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase27
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
class Unary1RegisterSetTesterCase28
    : public Unary1RegisterSetTester {
 public:
  Unary1RegisterSetTesterCase28(const NamedClassDecoder& decoder)
    : Unary1RegisterSetTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary1RegisterSetTesterCase28
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
class UnsafeCondDecoderTesterCase29
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase29(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase29
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
class UnsafeCondDecoderTesterCase30
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase30(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase30
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
class BranchToRegisterTesterCase31
    : public BranchToRegisterTester {
 public:
  BranchToRegisterTesterCase31(const NamedClassDecoder& decoder)
    : BranchToRegisterTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BranchToRegisterTesterCase31
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
class Unary2RegisterOpNotRmIsPcTesterCase32
    : public Unary2RegisterOpNotRmIsPcTester {
 public:
  Unary2RegisterOpNotRmIsPcTesterCase32(const NamedClassDecoder& decoder)
    : Unary2RegisterOpNotRmIsPcTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Unary2RegisterOpNotRmIsPcTesterCase32
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
class BranchToRegisterTesterCase34
    : public BranchToRegisterTesterRegsNotPc {
 public:
  BranchToRegisterTesterCase34(const NamedClassDecoder& decoder)
    : BranchToRegisterTesterRegsNotPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BranchToRegisterTesterCase34
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
class UnsafeCondDecoderTesterCase35
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase35(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase35
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
class Immediate16UseTesterCase36
    : public Immediate16UseTester {
 public:
  Immediate16UseTesterCase36(const NamedClassDecoder& decoder)
    : Immediate16UseTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Immediate16UseTesterCase36
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
class UnsafeCondDecoderTesterCase37
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase37(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase37
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
class UnsafeCondDecoderTesterCase38
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase38(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase38
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

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(26:20)=0010000 & inst(7:4)=0000 & inst(19:16)=xxx1 & inst(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Setend_Rule_157_P314'}
//
// Representative case:
// op1(26:20)=0010000 & op2(7:4)=0000 & Rn(19:16)=xxx1 & $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Setend_Rule_157_P314}
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
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Cps_Rule_b6_1_1_A1_B6_3'}
//
// Representative case:
// op1(26:20)=0010000 & op2(7:4)=xx0x & Rn(19:16)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Cps_Rule_b6_1_1_A1_B6_3}
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
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010011
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
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
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0000
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
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
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Clrex_Rule_30_A1_P70'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0001 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Clrex_Rule_30_A1_P70}
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
//    = DataBarrier {'constraints': ,
//     'rule': 'Dsb_Rule_42_A1_P92'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0100 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier {constraints: ,
//     rule: Dsb_Rule_42_A1_P92}
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
//    = DataBarrier {'constraints': ,
//     'rule': 'Dmb_Rule_41_A1_P90'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0101 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = DataBarrier {constraints: ,
//     rule: Dmb_Rule_41_A1_P90}
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
//    = InstructionBarrier {'constraints': ,
//     'rule': 'Isb_Rule_49_A1_P102'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0110 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = InstructionBarrier {constraints: ,
//     rule: Isb_Rule_49_A1_P102}
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
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=0111
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
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
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=001x
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
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
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1010111 & op2(7:4)=1xxx
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
// inst(26:20)=100x001
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Unallocated_hints'}
//
// Representative case:
// op1(26:20)=100x001
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Unallocated_hints}
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
//    = PreloadRegisterImm12Op {'constraints': ,
//     'rule': 'Pli_Rule_120_A1_P242'}
//
// Representative case:
// op1(26:20)=100x101 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: ,
//     rule: Pli_Rule_120_A1_P242}
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
//    = PreloadRegisterImm12Op {'constraints': & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     'rule': 'Pldw_Rule_117_A1_P236'}
//
// Representative case:
// op1(26:20)=101x001 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     rule: Pldw_Rule_117_A1_P236}
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
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=101x001 & Rn(19:16)=1111
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
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
//    = PreloadRegisterImm12Op {'constraints': & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     'rule': 'Pld_Rule_117_A1_P236'}
//
// Representative case:
// op1(26:20)=101x101 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: & ~xxxxxxxxxxxx1111xxxxxxxxxxxxxxxx ,
//     rule: Pld_Rule_117_A1_P236}
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
//    = PreloadRegisterImm12Op {'constraints': ,
//     'rule': 'Pld_Rule_118_A1_P238'}
//
// Representative case:
// op1(26:20)=101x101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterImm12Op {constraints: ,
//     rule: Pld_Rule_118_A1_P238}
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
//    = ForbiddenUncondDecoder {'constraints': ,
//     'rule': 'Unallocated_hints'}
//
// Representative case:
// op1(26:20)=110x001 & op2(7:4)=xxx0
//    = ForbiddenUncondDecoder {constraints: ,
//     rule: Unallocated_hints}
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
//    = PreloadRegisterPairOp {'constraints': ,
//     'rule': 'Pli_Rule_121_A1_P244'}
//
// Representative case:
// op1(26:20)=110x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOp {constraints: ,
//     rule: Pli_Rule_121_A1_P244}
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
//    = PreloadRegisterPairOpWAndRnNotPc {'constraints': ,
//     'rule': 'Pldw_Rule_119_A1_P240'}
//
// Representative case:
// op1(26:20)=111x001 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc {constraints: ,
//     rule: Pldw_Rule_119_A1_P240}
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
//    = PreloadRegisterPairOpWAndRnNotPc {'constraints': ,
//     'rule': 'Pld_Rule_119_A1_P240'}
//
// Representative case:
// op1(26:20)=111x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = PreloadRegisterPairOpWAndRnNotPc {constraints: ,
//     rule: Pld_Rule_119_A1_P240}
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
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=1011x11
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
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
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=100xx11
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
// inst(26:20)=11xxx11 & inst(7:4)=xxx0
//    = UnpredictableUncondDecoder {'constraints': ,
//     'rule': 'Unpredictable'}
//
// Representative case:
// op1(26:20)=11xxx11 & op2(7:4)=xxx0
//    = UnpredictableUncondDecoder {constraints: ,
//     rule: Unpredictable}
class UnpredictableUncondDecoderTester_Case23
    : public UnsafeUncondDecoderTesterCase23 {
 public:
  UnpredictableUncondDecoderTester_Case23()
    : UnsafeUncondDecoderTesterCase23(
      state_.UnpredictableUncondDecoder_Unpredictable_instance_)
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
class Unary1RegisterUseTester_Case24
    : public Unary1RegisterUseTesterCase24 {
 public:
  Unary1RegisterUseTester_Case24()
    : Unary1RegisterUseTesterCase24(
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
class ForbiddenCondDecoderTester_Case25
    : public UnsafeCondDecoderTesterCase25 {
 public:
  ForbiddenCondDecoderTester_Case25()
    : UnsafeCondDecoderTesterCase25(
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
class ForbiddenCondDecoderTester_Case26
    : public UnsafeCondDecoderTesterCase26 {
 public:
  ForbiddenCondDecoderTester_Case26()
    : UnsafeCondDecoderTesterCase26(
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
class ForbiddenCondDecoderTester_Case27
    : public UnsafeCondDecoderTesterCase27 {
 public:
  ForbiddenCondDecoderTester_Case27()
    : UnsafeCondDecoderTesterCase27(
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
class Unary1RegisterSetTester_Case28
    : public Unary1RegisterSetTesterCase28 {
 public:
  Unary1RegisterSetTester_Case28()
    : Unary1RegisterSetTesterCase28(
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
class ForbiddenCondDecoderTester_Case29
    : public UnsafeCondDecoderTesterCase29 {
 public:
  ForbiddenCondDecoderTester_Case29()
    : UnsafeCondDecoderTesterCase29(
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
class ForbiddenCondDecoderTester_Case30
    : public UnsafeCondDecoderTesterCase30 {
 public:
  ForbiddenCondDecoderTester_Case30()
    : UnsafeCondDecoderTesterCase30(
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
class BranchToRegisterTester_Case31
    : public BranchToRegisterTesterCase31 {
 public:
  BranchToRegisterTester_Case31()
    : BranchToRegisterTesterCase31(
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
class Unary2RegisterOpNotRmIsPcTester_Case32
    : public Unary2RegisterOpNotRmIsPcTesterCase32 {
 public:
  Unary2RegisterOpNotRmIsPcTester_Case32()
    : Unary2RegisterOpNotRmIsPcTesterCase32(
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
class ForbiddenCondDecoderTester_Case33
    : public UnsafeCondDecoderTesterCase33 {
 public:
  ForbiddenCondDecoderTester_Case33()
    : UnsafeCondDecoderTesterCase33(
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
class BranchToRegisterTester_Case34
    : public BranchToRegisterTesterCase34 {
 public:
  BranchToRegisterTester_Case34()
    : BranchToRegisterTesterCase34(
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
class ForbiddenCondDecoderTester_Case35
    : public UnsafeCondDecoderTesterCase35 {
 public:
  ForbiddenCondDecoderTester_Case35()
    : UnsafeCondDecoderTesterCase35(
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
class BreakPointAndConstantPoolHeadTester_Case36
    : public Immediate16UseTesterCase36 {
 public:
  BreakPointAndConstantPoolHeadTester_Case36()
    : Immediate16UseTesterCase36(
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
class ForbiddenCondDecoderTester_Case37
    : public UnsafeCondDecoderTesterCase37 {
 public:
  ForbiddenCondDecoderTester_Case37()
    : UnsafeCondDecoderTesterCase37(
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
class ForbiddenCondDecoderTester_Case38
    : public UnsafeCondDecoderTesterCase38 {
 public:
  ForbiddenCondDecoderTester_Case38()
    : UnsafeCondDecoderTesterCase38(
      state_.ForbiddenCondDecoder_Smc_Rule_B6_1_9_P18_instance_)
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
       ForbiddenUncondDecoderTester_Case0_TestCase0) {
  ForbiddenUncondDecoderTester_Case0 baseline_tester;
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
       ForbiddenUncondDecoderTester_Case1_TestCase1) {
  ForbiddenUncondDecoderTester_Case1 baseline_tester;
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
       UnpredictableUncondDecoderTester_Case2_TestCase2) {
  UnpredictableUncondDecoderTester_Case2 baseline_tester;
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
       UnpredictableUncondDecoderTester_Case3_TestCase3) {
  UnpredictableUncondDecoderTester_Case3 baseline_tester;
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
       ForbiddenUncondDecoderTester_Case4_TestCase4) {
  ForbiddenUncondDecoderTester_Case4 baseline_tester;
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
       DataBarrierTester_Case5_TestCase5) {
  DataBarrierTester_Case5 tester;
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
       DataBarrierTester_Case6_TestCase6) {
  DataBarrierTester_Case6 tester;
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
       InstructionBarrierTester_Case7_TestCase7) {
  InstructionBarrierTester_Case7 tester;
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
       UnpredictableUncondDecoderTester_Case8_TestCase8) {
  UnpredictableUncondDecoderTester_Case8 baseline_tester;
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
       UnpredictableUncondDecoderTester_Case9_TestCase9) {
  UnpredictableUncondDecoderTester_Case9 baseline_tester;
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
       UnpredictableUncondDecoderTester_Case10_TestCase10) {
  UnpredictableUncondDecoderTester_Case10 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101010111xxxxxxxxxxxx1xxxxxxx");
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
       ForbiddenUncondDecoderTester_Case11_TestCase11) {
  ForbiddenUncondDecoderTester_Case11 baseline_tester;
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
       PreloadRegisterImm12OpTester_Case12_TestCase12) {
  PreloadRegisterImm12OpTester_Case12 baseline_tester;
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
       PreloadRegisterImm12OpTester_Case13_TestCase13) {
  PreloadRegisterImm12OpTester_Case13 baseline_tester;
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
       UnpredictableUncondDecoderTester_Case14_TestCase14) {
  UnpredictableUncondDecoderTester_Case14 baseline_tester;
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
       PreloadRegisterImm12OpTester_Case15_TestCase15) {
  PreloadRegisterImm12OpTester_Case15 baseline_tester;
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
       PreloadRegisterImm12OpTester_Case16_TestCase16) {
  PreloadRegisterImm12OpTester_Case16 baseline_tester;
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
       ForbiddenUncondDecoderTester_Case17_TestCase17) {
  ForbiddenUncondDecoderTester_Case17 baseline_tester;
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
       PreloadRegisterPairOpTester_Case18_TestCase18) {
  PreloadRegisterPairOpTester_Case18 tester;
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
       PreloadRegisterPairOpWAndRnNotPcTester_Case19_TestCase19) {
  PreloadRegisterPairOpWAndRnNotPcTester_Case19 tester;
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
       PreloadRegisterPairOpWAndRnNotPcTester_Case20_TestCase20) {
  PreloadRegisterPairOpWAndRnNotPcTester_Case20 tester;
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
       UnpredictableUncondDecoderTester_Case21_TestCase21) {
  UnpredictableUncondDecoderTester_Case21 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101011x11xxxxxxxxxxxxxxxxxxxx");
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
       UnpredictableUncondDecoderTester_Case22_TestCase22) {
  UnpredictableUncondDecoderTester_Case22 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("11110100xx11xxxxxxxxxxxxxxxxxxxx");
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
       UnpredictableUncondDecoderTester_Case23_TestCase23) {
  UnpredictableUncondDecoderTester_Case23 baseline_tester;
  NamedUnpredictable_Unpredictable actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("1111011xxx11xxxxxxxxxxxxxxx0xxxx");
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
       Unary1RegisterUseTester_Case24_TestCase24) {
  Unary1RegisterUseTester_Case24 tester;
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
       ForbiddenCondDecoderTester_Case25_TestCase25) {
  ForbiddenCondDecoderTester_Case25 baseline_tester;
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
       ForbiddenCondDecoderTester_Case26_TestCase26) {
  ForbiddenCondDecoderTester_Case26 baseline_tester;
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
       ForbiddenCondDecoderTester_Case27_TestCase27) {
  ForbiddenCondDecoderTester_Case27 baseline_tester;
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
       Unary1RegisterSetTester_Case28_TestCase28) {
  Unary1RegisterSetTester_Case28 tester;
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
       ForbiddenCondDecoderTester_Case29_TestCase29) {
  ForbiddenCondDecoderTester_Case29 baseline_tester;
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
       ForbiddenCondDecoderTester_Case30_TestCase30) {
  ForbiddenCondDecoderTester_Case30 baseline_tester;
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
       BranchToRegisterTester_Case31_TestCase31) {
  BranchToRegisterTester_Case31 baseline_tester;
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
       Unary2RegisterOpNotRmIsPcTester_Case32_TestCase32) {
  Unary2RegisterOpNotRmIsPcTester_Case32 baseline_tester;
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
       ForbiddenCondDecoderTester_Case33_TestCase33) {
  ForbiddenCondDecoderTester_Case33 baseline_tester;
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
       BranchToRegisterTester_Case34_TestCase34) {
  BranchToRegisterTester_Case34 baseline_tester;
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
       ForbiddenCondDecoderTester_Case35_TestCase35) {
  ForbiddenCondDecoderTester_Case35 baseline_tester;
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
       BreakPointAndConstantPoolHeadTester_Case36_TestCase36) {
  BreakPointAndConstantPoolHeadTester_Case36 baseline_tester;
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
       ForbiddenCondDecoderTester_Case37_TestCase37) {
  ForbiddenCondDecoderTester_Case37 baseline_tester;
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
       ForbiddenCondDecoderTester_Case38_TestCase38) {
  ForbiddenCondDecoderTester_Case38 baseline_tester;
  NamedForbidden_Smc_Rule_B6_1_9_P18 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000101100000000000000111iiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
