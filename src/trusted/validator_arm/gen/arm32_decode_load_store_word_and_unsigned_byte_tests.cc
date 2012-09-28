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
// inst(25)=0 & inst(24:20)=0x010
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// A(25)=0 & op1(24:20)=0x010
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase0
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase0(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase0
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
class UnsafeCondDecoderTesterCase2
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase2(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase2
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
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25)=~0 */) return false;
  if ((inst.Bits() & 0x01700000) != 0x00700000 /* op1(24:20)=~0x111 */) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=~1111 & inst(24:20)=~0x011
//    = LdrImmediateOp {'constraints': }
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x011
//    = LdrImmediateOp {constraints: }
class LoadStore2RegisterImm12OpTesterCase4
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase4(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x02000000) != 0x00000000 /* A(25)=~0 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00100000 /* op1(24:20)=~xx0x1 */) return false;
  if ((inst.Bits() & 0x000F0000) == 0x000F0000 /* Rn(19:16)=1111 */) return false;
  if ((inst.Bits() & 0x01700000) == 0x00300000 /* op1_repeated(24:20)=0x011 */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=1111 & inst(24:20)=~0x011 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = Load2RegisterImm12Op {'constraints': }
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=1111 & op1_repeated(24:20)=~0x011 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = Load2RegisterImm12Op {constraints: }
class LoadStore2RegisterImm12OpTesterCase5
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase5(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase5
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
class LoadStore2RegisterImm12OpTesterCase6
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase6(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase6
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
class LoadStore2RegisterImm12OpTesterCase7
    : public LoadStore2RegisterImm12OpTesterNotRnIsPc {
 public:
  LoadStore2RegisterImm12OpTesterCase7(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTesterNotRnIsPc(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase7
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
class LoadStore2RegisterImm12OpTesterCase8
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterCase8(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm12OpTesterCase8
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
// inst(25)=1 & inst(24:20)=0x010 & inst(4)=0
//    = ForbiddenCondDecoder {'constraints': }
//
// Representaive case:
// A(25)=1 & op1(24:20)=0x010 & B(4)=0
//    = ForbiddenCondDecoder {constraints: }
class UnsafeCondDecoderTesterCase9
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase9(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase9
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
class UnsafeCondDecoderTesterCase10
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase10(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase10
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
class UnsafeCondDecoderTesterCase11
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase11(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase11
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
class UnsafeCondDecoderTesterCase12
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase12(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase12
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
class LoadStore3RegisterImm5OpTesterCase13
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterCase13(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterCase13
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
class LoadStore3RegisterImm5OpTesterCase14
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterCase14(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterCase14
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
class LoadStore3RegisterImm5OpTesterCase15
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterCase15(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterCase15
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
class LoadStore3RegisterImm5OpTesterCase16
    : public LoadStore3RegisterImm5OpTester {
 public:
  LoadStore3RegisterImm5OpTesterCase16(const NamedClassDecoder& decoder)
    : LoadStore3RegisterImm5OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterImm5OpTesterCase16
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

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(25)=0 & inst(24:20)=0x010
//    = ForbiddenCondDecoder {'constraints': ,
//     'rule': 'Strt_Rule_A1'}
//
// Representative case:
// A(25)=0 & op1(24:20)=0x010
//    = ForbiddenCondDecoder {constraints: ,
//     rule: Strt_Rule_A1}
class ForbiddenCondDecoderTester_Case0
    : public UnsafeCondDecoderTesterCase0 {
 public:
  ForbiddenCondDecoderTester_Case0()
    : UnsafeCondDecoderTesterCase0(
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
class ForbiddenCondDecoderTester_Case1
    : public UnsafeCondDecoderTesterCase1 {
 public:
  ForbiddenCondDecoderTester_Case1()
    : UnsafeCondDecoderTesterCase1(
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
class ForbiddenCondDecoderTester_Case2
    : public UnsafeCondDecoderTesterCase2 {
 public:
  ForbiddenCondDecoderTester_Case2()
    : UnsafeCondDecoderTesterCase2(
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
class ForbiddenCondDecoderTester_Case3
    : public UnsafeCondDecoderTesterCase3 {
 public:
  ForbiddenCondDecoderTester_Case3()
    : UnsafeCondDecoderTesterCase3(
      state_.ForbiddenCondDecoder_Ldrtb_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=~1111 & inst(24:20)=~0x011
//    = LdrImmediateOp {'constraints': ,
//     'rule': 'Ldr_Rule_58_A1_P120'}
//
// Representative case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x011
//    = LdrImmediateOp {constraints: ,
//     rule: Ldr_Rule_58_A1_P120}
class LdrImmediateOpTester_Case4
    : public LoadStore2RegisterImm12OpTesterCase4 {
 public:
  LdrImmediateOpTester_Case4()
    : LoadStore2RegisterImm12OpTesterCase4(
      state_.LdrImmediateOp_Ldr_Rule_58_A1_P120_instance_)
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
class Load2RegisterImm12OpTester_Case5
    : public LoadStore2RegisterImm12OpTesterCase5 {
 public:
  Load2RegisterImm12OpTester_Case5()
    : LoadStore2RegisterImm12OpTesterCase5(
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
class Store2RegisterImm12OpTester_Case6
    : public LoadStore2RegisterImm12OpTesterCase6 {
 public:
  Store2RegisterImm12OpTester_Case6()
    : LoadStore2RegisterImm12OpTesterCase6(
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
class Load2RegisterImm12OpTester_Case7
    : public LoadStore2RegisterImm12OpTesterCase7 {
 public:
  Load2RegisterImm12OpTester_Case7()
    : LoadStore2RegisterImm12OpTesterCase7(
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
class Load2RegisterImm12OpTester_Case8
    : public LoadStore2RegisterImm12OpTesterCase8 {
 public:
  Load2RegisterImm12OpTester_Case8()
    : LoadStore2RegisterImm12OpTesterCase8(
      state_.Load2RegisterImm12Op_Ldrb_Rule_63_A1_P130_instance_)
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
class ForbiddenCondDecoderTester_Case9
    : public UnsafeCondDecoderTesterCase9 {
 public:
  ForbiddenCondDecoderTester_Case9()
    : UnsafeCondDecoderTesterCase9(
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
class ForbiddenCondDecoderTester_Case10
    : public UnsafeCondDecoderTesterCase10 {
 public:
  ForbiddenCondDecoderTester_Case10()
    : UnsafeCondDecoderTesterCase10(
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
class ForbiddenCondDecoderTester_Case11
    : public UnsafeCondDecoderTesterCase11 {
 public:
  ForbiddenCondDecoderTester_Case11()
    : UnsafeCondDecoderTesterCase11(
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
class ForbiddenCondDecoderTester_Case12
    : public UnsafeCondDecoderTesterCase12 {
 public:
  ForbiddenCondDecoderTester_Case12()
    : UnsafeCondDecoderTesterCase12(
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
class Store3RegisterImm5OpTester_Case13
    : public LoadStore3RegisterImm5OpTesterCase13 {
 public:
  Store3RegisterImm5OpTester_Case13()
    : LoadStore3RegisterImm5OpTesterCase13(
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
class Load3RegisterImm5OpTester_Case14
    : public LoadStore3RegisterImm5OpTesterCase14 {
 public:
  Load3RegisterImm5OpTester_Case14()
    : LoadStore3RegisterImm5OpTesterCase14(
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
class Store3RegisterImm5OpTester_Case15
    : public LoadStore3RegisterImm5OpTesterCase15 {
 public:
  Store3RegisterImm5OpTester_Case15()
    : LoadStore3RegisterImm5OpTesterCase15(
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
class Load3RegisterImm5OpTester_Case16
    : public LoadStore3RegisterImm5OpTesterCase16 {
 public:
  Load3RegisterImm5OpTester_Case16()
    : LoadStore3RegisterImm5OpTesterCase16(
      state_.Load3RegisterImm5Op_Ldrb_Rule_64_A1_P132_instance_)
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
       ForbiddenCondDecoderTester_Case0_TestCase0) {
  ForbiddenCondDecoderTester_Case0 baseline_tester;
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
       ForbiddenCondDecoderTester_Case1_TestCase1) {
  ForbiddenCondDecoderTester_Case1 baseline_tester;
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
       ForbiddenCondDecoderTester_Case2_TestCase2) {
  ForbiddenCondDecoderTester_Case2 baseline_tester;
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
       ForbiddenCondDecoderTester_Case3_TestCase3) {
  ForbiddenCondDecoderTester_Case3 baseline_tester;
  NamedForbidden_Ldrtb_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0100u111nnnnttttiiiiiiiiiiii");
}

// Neutral case:
// inst(25)=0 & inst(24:20)=xx0x1 & inst(19:16)=~1111 & inst(24:20)=~0x011
//    = LdrImmediateOp => LdrImmediateOp {'constraints': ,
//     'pattern': 'cccc010pu0w1nnnnttttiiiiiiiiiiii',
//     'rule': 'Ldr_Rule_58_A1_P120'}
//
// Representaive case:
// A(25)=0 & op1(24:20)=xx0x1 & Rn(19:16)=~1111 & op1_repeated(24:20)=~0x011
//    = LdrImmediateOp => LdrImmediateOp {constraints: ,
//     pattern: cccc010pu0w1nnnnttttiiiiiiiiiiii,
//     rule: Ldr_Rule_58_A1_P120}
TEST_F(Arm32DecoderStateTests,
       LdrImmediateOpTester_Case4_TestCase4) {
  LdrImmediateOpTester_Case4 tester;
  tester.Test("cccc010pu0w1nnnnttttiiiiiiiiiiii");
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
       Load2RegisterImm12OpTester_Case5_TestCase5) {
  Load2RegisterImm12OpTester_Case5 baseline_tester;
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
       Store2RegisterImm12OpTester_Case6_TestCase6) {
  Store2RegisterImm12OpTester_Case6 baseline_tester;
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
       Load2RegisterImm12OpTester_Case7_TestCase7) {
  Load2RegisterImm12OpTester_Case7 baseline_tester;
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
       Load2RegisterImm12OpTester_Case8_TestCase8) {
  Load2RegisterImm12OpTester_Case8 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrb_Rule_63_A1_P130 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0101u1011111ttttiiiiiiiiiiii");
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
       ForbiddenCondDecoderTester_Case9_TestCase9) {
  ForbiddenCondDecoderTester_Case9 baseline_tester;
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
       ForbiddenCondDecoderTester_Case10_TestCase10) {
  ForbiddenCondDecoderTester_Case10 baseline_tester;
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
       ForbiddenCondDecoderTester_Case11_TestCase11) {
  ForbiddenCondDecoderTester_Case11 baseline_tester;
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
       ForbiddenCondDecoderTester_Case12_TestCase12) {
  ForbiddenCondDecoderTester_Case12 baseline_tester;
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
       Store3RegisterImm5OpTester_Case13_TestCase13) {
  Store3RegisterImm5OpTester_Case13 baseline_tester;
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
       Load3RegisterImm5OpTester_Case14_TestCase14) {
  Load3RegisterImm5OpTester_Case14 baseline_tester;
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
       Store3RegisterImm5OpTester_Case15_TestCase15) {
  Store3RegisterImm5OpTester_Case15 baseline_tester;
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
       Load3RegisterImm5OpTester_Case16_TestCase16) {
  Load3RegisterImm5OpTester_Case16 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrb_Rule_64_A1_P132 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc011pu1w1nnnnttttiiiiitt0mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
