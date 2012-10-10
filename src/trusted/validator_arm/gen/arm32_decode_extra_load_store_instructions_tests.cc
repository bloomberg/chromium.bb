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
// inst(6:5)=01 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Store3RegisterOp',
//       constraints: }
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Store3RegisterOp,
//       constraints: }
class LoadStore3RegisterOpTesterCase0
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterCase0(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterOpTesterCase0
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
//    = {baseline: 'Load3RegisterOp',
//       constraints: }
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Load3RegisterOp,
//       constraints: }
class LoadStore3RegisterOpTesterCase1
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterCase1(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterOpTesterCase1
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
//    = {baseline: 'Store2RegisterImm8Op',
//       constraints: }
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx1x0
//    = {baseline: Store2RegisterImm8Op,
//       constraints: }
class LoadStore2RegisterImm8OpTesterCase2
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase2(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase2
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
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: }
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {baseline: Load2RegisterImm8Op,
//       constraints: }
class LoadStore2RegisterImm8OpTesterCase3
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase3(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase3
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
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: }
//
// Representaive case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: Load2RegisterImm8Op,
//       constraints: }
class LoadStore2RegisterImm8OpTesterCase4
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase4(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000020 /* op2(6:5)=~01 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x01200000) != 0x01000000 /* $pattern(31:0)=~xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Load3RegisterDoubleOp',
//       constraints: }
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Load3RegisterDoubleOp,
//       constraints: }
class LoadStore3RegisterDoubleOpTesterCase5
    : public LoadStore3RegisterDoubleOpTester {
 public:
  LoadStore3RegisterDoubleOpTesterCase5(const NamedClassDecoder& decoder)
    : LoadStore3RegisterDoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterDoubleOpTesterCase5
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
//    = {baseline: 'Load3RegisterOp',
//       constraints: }
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Load3RegisterOp,
//       constraints: }
class LoadStore3RegisterOpTesterCase6
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterCase6(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterOpTesterCase6
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
//    = {baseline: 'Load2RegisterImm8DoubleOp',
//       constraints: }
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=~1111
//    = {baseline: Load2RegisterImm8DoubleOp,
//       constraints: }
class LoadStore2RegisterImm8DoubleOpTesterCase7
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  LoadStore2RegisterImm8DoubleOpTesterCase7(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8DoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8DoubleOpTesterCase7
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
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'Load2RegisterImm8DoubleOp',
//       constraints: }
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: Load2RegisterImm8DoubleOp,
//       constraints: }
class LoadStore2RegisterImm8DoubleOpTesterCase8
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  LoadStore2RegisterImm8DoubleOpTesterCase8(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8DoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8DoubleOpTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00400000 /* op1(24:20)=~xx1x0 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x01200000) != 0x01000000 /* $pattern(31:0)=~xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8DoubleOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: }
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {baseline: Load2RegisterImm8Op,
//       constraints: }
class LoadStore2RegisterImm8OpTesterCase9
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase9(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase9
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
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: }
//
// Representaive case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: Load2RegisterImm8Op,
//       constraints: }
class LoadStore2RegisterImm8OpTesterCase10
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase10(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000040 /* op2(6:5)=~10 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x01200000) != 0x01000000 /* $pattern(31:0)=~xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Store3RegisterDoubleOp',
//       constraints: }
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Store3RegisterDoubleOp,
//       constraints: }
class LoadStore3RegisterDoubleOpTesterCase11
    : public LoadStore3RegisterDoubleOpTester {
 public:
  LoadStore3RegisterDoubleOpTesterCase11(const NamedClassDecoder& decoder)
    : LoadStore3RegisterDoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterDoubleOpTesterCase11
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
//    = {baseline: 'Load3RegisterOp',
//       constraints: }
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Load3RegisterOp,
//       constraints: }
class LoadStore3RegisterOpTesterCase12
    : public LoadStore3RegisterOpTester {
 public:
  LoadStore3RegisterOpTesterCase12(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore3RegisterOpTesterCase12
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
//    = {baseline: 'Store2RegisterImm8DoubleOp',
//       constraints: }
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx1x0
//    = {baseline: Store2RegisterImm8DoubleOp,
//       constraints: }
class LoadStore2RegisterImm8DoubleOpTesterCase13
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  LoadStore2RegisterImm8DoubleOpTesterCase13(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8DoubleOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8DoubleOpTesterCase13
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
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: }
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {baseline: Load2RegisterImm8Op,
//       constraints: }
class LoadStore2RegisterImm8OpTesterCase14
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase14(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase14
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
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: }
//
// Representaive case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: Load2RegisterImm8Op,
//       constraints: }
class LoadStore2RegisterImm8OpTesterCase15
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterCase15(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImm8OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LoadStore2RegisterImm8OpTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  if ((inst.Bits() & 0x00000060) != 0x00000060 /* op2(6:5)=~11 */) return false;
  if ((inst.Bits() & 0x00500000) != 0x00500000 /* op1(24:20)=~xx1x1 */) return false;
  if ((inst.Bits() & 0x000F0000) != 0x000F0000 /* Rn(19:16)=~1111 */) return false;
  if ((inst.Bits() & 0x01200000) != 0x01000000 /* $pattern(31:0)=~xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx */) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStore2RegisterImm8OpTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Store3RegisterOp',
//       constraints: ,
//       rule: 'Strh_Rule_208_A1_P412'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Store3RegisterOp,
//       constraints: ,
//       rule: Strh_Rule_208_A1_P412}
class Store3RegisterOpTester_Case0
    : public LoadStore3RegisterOpTesterCase0 {
 public:
  Store3RegisterOpTester_Case0()
    : LoadStore3RegisterOpTesterCase0(
      state_.Store3RegisterOp_Strh_Rule_208_A1_P412_instance_)
  {}
};

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Load3RegisterOp',
//       constraints: ,
//       rule: 'Ldrh_Rule_76_A1_P156'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Load3RegisterOp,
//       constraints: ,
//       rule: Ldrh_Rule_76_A1_P156}
class Load3RegisterOpTester_Case1
    : public LoadStore3RegisterOpTesterCase1 {
 public:
  Load3RegisterOpTester_Case1()
    : LoadStore3RegisterOpTesterCase1(
      state_.Load3RegisterOp_Ldrh_Rule_76_A1_P156_instance_)
  {}
};

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x0
//    = {baseline: 'Store2RegisterImm8Op',
//       constraints: ,
//       rule: 'Strh_Rule_207_A1_P410'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx1x0
//    = {baseline: Store2RegisterImm8Op,
//       constraints: ,
//       rule: Strh_Rule_207_A1_P410}
class Store2RegisterImm8OpTester_Case2
    : public LoadStore2RegisterImm8OpTesterCase2 {
 public:
  Store2RegisterImm8OpTester_Case2()
    : LoadStore2RegisterImm8OpTesterCase2(
      state_.Store2RegisterImm8Op_Strh_Rule_207_A1_P410_instance_)
  {}
};

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       rule: 'Ldrh_Rule_74_A1_P152'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {baseline: Load2RegisterImm8Op,
//       constraints: ,
//       rule: Ldrh_Rule_74_A1_P152}
class Load2RegisterImm8OpTester_Case3
    : public LoadStore2RegisterImm8OpTesterCase3 {
 public:
  Load2RegisterImm8OpTester_Case3()
    : LoadStore2RegisterImm8OpTesterCase3(
      state_.Load2RegisterImm8Op_Ldrh_Rule_74_A1_P152_instance_)
  {}
};

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       rule: 'Ldrh_Rule_75_A1_P154'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: Load2RegisterImm8Op,
//       constraints: ,
//       rule: Ldrh_Rule_75_A1_P154}
class Load2RegisterImm8OpTester_Case4
    : public LoadStore2RegisterImm8OpTesterCase4 {
 public:
  Load2RegisterImm8OpTester_Case4()
    : LoadStore2RegisterImm8OpTesterCase4(
      state_.Load2RegisterImm8Op_Ldrh_Rule_75_A1_P154_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Load3RegisterDoubleOp',
//       constraints: ,
//       rule: 'Ldrd_Rule_68_A1_P140'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Load3RegisterDoubleOp,
//       constraints: ,
//       rule: Ldrd_Rule_68_A1_P140}
class Load3RegisterDoubleOpTester_Case5
    : public LoadStore3RegisterDoubleOpTesterCase5 {
 public:
  Load3RegisterDoubleOpTester_Case5()
    : LoadStore3RegisterDoubleOpTesterCase5(
      state_.Load3RegisterDoubleOp_Ldrd_Rule_68_A1_P140_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Load3RegisterOp',
//       constraints: ,
//       rule: 'Ldrsb_Rule_80_A1_P164'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Load3RegisterOp,
//       constraints: ,
//       rule: Ldrsb_Rule_80_A1_P164}
class Load3RegisterOpTester_Case6
    : public LoadStore3RegisterOpTesterCase6 {
 public:
  Load3RegisterOpTester_Case6()
    : LoadStore3RegisterOpTesterCase6(
      state_.Load3RegisterOp_Ldrsb_Rule_80_A1_P164_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=~1111
//    = {baseline: 'Load2RegisterImm8DoubleOp',
//       constraints: ,
//       rule: 'Ldrd_Rule_66_A1_P136'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=~1111
//    = {baseline: Load2RegisterImm8DoubleOp,
//       constraints: ,
//       rule: Ldrd_Rule_66_A1_P136}
class Load2RegisterImm8DoubleOpTester_Case7
    : public LoadStore2RegisterImm8DoubleOpTesterCase7 {
 public:
  Load2RegisterImm8DoubleOpTester_Case7()
    : LoadStore2RegisterImm8DoubleOpTesterCase7(
      state_.Load2RegisterImm8DoubleOp_Ldrd_Rule_66_A1_P136_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'Load2RegisterImm8DoubleOp',
//       constraints: ,
//       rule: 'Ldrd_Rule_67_A1_P138'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: Load2RegisterImm8DoubleOp,
//       constraints: ,
//       rule: Ldrd_Rule_67_A1_P138}
class Load2RegisterImm8DoubleOpTester_Case8
    : public LoadStore2RegisterImm8DoubleOpTesterCase8 {
 public:
  Load2RegisterImm8DoubleOpTester_Case8()
    : LoadStore2RegisterImm8DoubleOpTesterCase8(
      state_.Load2RegisterImm8DoubleOp_Ldrd_Rule_67_A1_P138_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       rule: 'Ldrsb_Rule_78_A1_P160'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {baseline: Load2RegisterImm8Op,
//       constraints: ,
//       rule: Ldrsb_Rule_78_A1_P160}
class Load2RegisterImm8OpTester_Case9
    : public LoadStore2RegisterImm8OpTesterCase9 {
 public:
  Load2RegisterImm8OpTester_Case9()
    : LoadStore2RegisterImm8OpTesterCase9(
      state_.Load2RegisterImm8Op_Ldrsb_Rule_78_A1_P160_instance_)
  {}
};

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       rule: 'ldrsb_Rule_79_A1_162'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: Load2RegisterImm8Op,
//       constraints: ,
//       rule: ldrsb_Rule_79_A1_162}
class Load2RegisterImm8OpTester_Case10
    : public LoadStore2RegisterImm8OpTesterCase10 {
 public:
  Load2RegisterImm8OpTester_Case10()
    : LoadStore2RegisterImm8OpTesterCase10(
      state_.Load2RegisterImm8Op_ldrsb_Rule_79_A1_162_instance_)
  {}
};

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Store3RegisterDoubleOp',
//       constraints: ,
//       rule: 'Strd_Rule_201_A1_P398'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Store3RegisterDoubleOp,
//       constraints: ,
//       rule: Strd_Rule_201_A1_P398}
class Store3RegisterDoubleOpTester_Case11
    : public LoadStore3RegisterDoubleOpTesterCase11 {
 public:
  Store3RegisterDoubleOpTester_Case11()
    : LoadStore3RegisterDoubleOpTesterCase11(
      state_.Store3RegisterDoubleOp_Strd_Rule_201_A1_P398_instance_)
  {}
};

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: 'Load3RegisterOp',
//       constraints: ,
//       rule: 'Ldrsh_Rule_84_A1_P172'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {baseline: Load3RegisterOp,
//       constraints: ,
//       rule: Ldrsh_Rule_84_A1_P172}
class Load3RegisterOpTester_Case12
    : public LoadStore3RegisterOpTesterCase12 {
 public:
  Load3RegisterOpTester_Case12()
    : LoadStore3RegisterOpTesterCase12(
      state_.Load3RegisterOp_Ldrsh_Rule_84_A1_P172_instance_)
  {}
};

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x0
//    = {baseline: 'Store2RegisterImm8DoubleOp',
//       constraints: ,
//       rule: 'Strd_Rule_200_A1_P396'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx1x0
//    = {baseline: Store2RegisterImm8DoubleOp,
//       constraints: ,
//       rule: Strd_Rule_200_A1_P396}
class Store2RegisterImm8DoubleOpTester_Case13
    : public LoadStore2RegisterImm8DoubleOpTesterCase13 {
 public:
  Store2RegisterImm8DoubleOpTester_Case13()
    : LoadStore2RegisterImm8DoubleOpTesterCase13(
      state_.Store2RegisterImm8DoubleOp_Strd_Rule_200_A1_P396_instance_)
  {}
};

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       rule: 'Ldrsh_Rule_82_A1_P168'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {baseline: Load2RegisterImm8Op,
//       constraints: ,
//       rule: Ldrsh_Rule_82_A1_P168}
class Load2RegisterImm8OpTester_Case14
    : public LoadStore2RegisterImm8OpTesterCase14 {
 public:
  Load2RegisterImm8OpTester_Case14()
    : LoadStore2RegisterImm8OpTesterCase14(
      state_.Load2RegisterImm8Op_Ldrsh_Rule_82_A1_P168_instance_)
  {}
};

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       rule: 'Ldrsh_Rule_83_A1_P170'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {baseline: Load2RegisterImm8Op,
//       constraints: ,
//       rule: Ldrsh_Rule_83_A1_P170}
class Load2RegisterImm8OpTester_Case15
    : public LoadStore2RegisterImm8OpTesterCase15 {
 public:
  Load2RegisterImm8OpTester_Case15()
    : LoadStore2RegisterImm8OpTesterCase15(
      state_.Load2RegisterImm8Op_Ldrsh_Rule_83_A1_P170_instance_)
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
// inst(6:5)=01 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'StoreBasedOffsetMemory',
//       baseline: 'Store3RegisterOp',
//       constraints: ,
//       pattern: 'cccc000pu0w0nnnntttt00001011mmmm',
//       rule: 'Strh_Rule_208_A1_P412'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: StoreBasedOffsetMemory,
//       baseline: Store3RegisterOp,
//       constraints: ,
//       pattern: cccc000pu0w0nnnntttt00001011mmmm,
//       rule: Strh_Rule_208_A1_P412}
TEST_F(Arm32DecoderStateTests,
       Store3RegisterOpTester_Case0_TestCase0) {
  Store3RegisterOpTester_Case0 baseline_tester;
  NamedStoreBasedOffsetMemory_Strh_Rule_208_A1_P412 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w0nnnntttt00001011mmmm");
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'LoadBasedOffsetMemory',
//       baseline: 'Load3RegisterOp',
//       constraints: ,
//       pattern: 'cccc000pu0w1nnnntttt00001011mmmm',
//       rule: 'Ldrh_Rule_76_A1_P156'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: LoadBasedOffsetMemory,
//       baseline: Load3RegisterOp,
//       constraints: ,
//       pattern: cccc000pu0w1nnnntttt00001011mmmm,
//       rule: Ldrh_Rule_76_A1_P156}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_Case1_TestCase1) {
  Load3RegisterOpTester_Case1 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrh_Rule_76_A1_P156 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w1nnnntttt00001011mmmm");
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x0
//    = {actual: 'StoreBasedImmedMemory',
//       baseline: 'Store2RegisterImm8Op',
//       constraints: ,
//       pattern: 'cccc000pu1w0nnnnttttiiii1011iiii',
//       rule: 'Strh_Rule_207_A1_P410'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx1x0
//    = {actual: StoreBasedImmedMemory,
//       baseline: Store2RegisterImm8Op,
//       constraints: ,
//       pattern: cccc000pu1w0nnnnttttiiii1011iiii,
//       rule: Strh_Rule_207_A1_P410}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm8OpTester_Case2_TestCase2) {
  Store2RegisterImm8OpTester_Case2 baseline_tester;
  NamedStoreBasedImmedMemory_Strh_Rule_207_A1_P410 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w0nnnnttttiiii1011iiii");
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {actual: 'LoadBasedImmedMemory',
//       baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       pattern: 'cccc000pu1w1nnnnttttiiii1011iiii',
//       rule: 'Ldrh_Rule_74_A1_P152'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {actual: LoadBasedImmedMemory,
//       baseline: Load2RegisterImm8Op,
//       constraints: ,
//       pattern: cccc000pu1w1nnnnttttiiii1011iiii,
//       rule: Ldrh_Rule_74_A1_P152}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Case3_TestCase3) {
  Load2RegisterImm8OpTester_Case3 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrh_Rule_74_A1_P152 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w1nnnnttttiiii1011iiii");
}

// Neutral case:
// inst(6:5)=01 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: 'LoadBasedImmedMemory',
//       baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       pattern: 'cccc0001u1011111ttttiiii1011iiii',
//       rule: 'Ldrh_Rule_75_A1_P154'}
//
// Representative case:
// op2(6:5)=01 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: LoadBasedImmedMemory,
//       baseline: Load2RegisterImm8Op,
//       constraints: ,
//       pattern: cccc0001u1011111ttttiiii1011iiii,
//       rule: Ldrh_Rule_75_A1_P154}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Case4_TestCase4) {
  Load2RegisterImm8OpTester_Case4 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrh_Rule_75_A1_P154 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1011111ttttiiii1011iiii");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'LoadBasedOffsetMemoryDouble',
//       baseline: 'Load3RegisterDoubleOp',
//       constraints: ,
//       pattern: 'cccc000pu0w0nnnntttt00001101mmmm',
//       rule: 'Ldrd_Rule_68_A1_P140'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: LoadBasedOffsetMemoryDouble,
//       baseline: Load3RegisterDoubleOp,
//       constraints: ,
//       pattern: cccc000pu0w0nnnntttt00001101mmmm,
//       rule: Ldrd_Rule_68_A1_P140}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterDoubleOpTester_Case5_TestCase5) {
  Load3RegisterDoubleOpTester_Case5 baseline_tester;
  NamedLoadBasedOffsetMemoryDouble_Ldrd_Rule_68_A1_P140 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w0nnnntttt00001101mmmm");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'LoadBasedOffsetMemory',
//       baseline: 'Load3RegisterOp',
//       constraints: ,
//       pattern: 'cccc000pu0w1nnnntttt00001101mmmm',
//       rule: 'Ldrsb_Rule_80_A1_P164'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: LoadBasedOffsetMemory,
//       baseline: Load3RegisterOp,
//       constraints: ,
//       pattern: cccc000pu0w1nnnntttt00001101mmmm,
//       rule: Ldrsb_Rule_80_A1_P164}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_Case6_TestCase6) {
  Load3RegisterOpTester_Case6 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrsb_Rule_80_A1_P164 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w1nnnntttt00001101mmmm");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=~1111
//    = {actual: 'LoadBasedImmedMemoryDouble',
//       baseline: 'Load2RegisterImm8DoubleOp',
//       constraints: ,
//       pattern: 'cccc000pu1w0nnnnttttiiii1101iiii',
//       rule: 'Ldrd_Rule_66_A1_P136'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=~1111
//    = {actual: LoadBasedImmedMemoryDouble,
//       baseline: Load2RegisterImm8DoubleOp,
//       constraints: ,
//       pattern: cccc000pu1w0nnnnttttiiii1101iiii,
//       rule: Ldrd_Rule_66_A1_P136}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8DoubleOpTester_Case7_TestCase7) {
  Load2RegisterImm8DoubleOpTester_Case7 baseline_tester;
  NamedLoadBasedImmedMemoryDouble_Ldrd_Rule_66_A1_P136 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w0nnnnttttiiii1101iiii");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x0 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: 'LoadBasedImmedMemoryDouble',
//       baseline: 'Load2RegisterImm8DoubleOp',
//       constraints: ,
//       pattern: 'cccc0001u1001111ttttiiii1101iiii',
//       rule: 'Ldrd_Rule_67_A1_P138'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x0 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: LoadBasedImmedMemoryDouble,
//       baseline: Load2RegisterImm8DoubleOp,
//       constraints: ,
//       pattern: cccc0001u1001111ttttiiii1101iiii,
//       rule: Ldrd_Rule_67_A1_P138}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8DoubleOpTester_Case8_TestCase8) {
  Load2RegisterImm8DoubleOpTester_Case8 baseline_tester;
  NamedLoadBasedImmedMemoryDouble_Ldrd_Rule_67_A1_P138 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1001111ttttiiii1101iiii");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {actual: 'LoadBasedImmedMemory',
//       baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       pattern: 'cccc000pu1w1nnnnttttiiii1101iiii',
//       rule: 'Ldrsb_Rule_78_A1_P160'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {actual: LoadBasedImmedMemory,
//       baseline: Load2RegisterImm8Op,
//       constraints: ,
//       pattern: cccc000pu1w1nnnnttttiiii1101iiii,
//       rule: Ldrsb_Rule_78_A1_P160}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Case9_TestCase9) {
  Load2RegisterImm8OpTester_Case9 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrsb_Rule_78_A1_P160 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w1nnnnttttiiii1101iiii");
}

// Neutral case:
// inst(6:5)=10 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: 'LoadBasedImmedMemory',
//       baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       pattern: 'cccc0001u1011111ttttiiii1101iiii',
//       rule: 'ldrsb_Rule_79_A1_162'}
//
// Representative case:
// op2(6:5)=10 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: LoadBasedImmedMemory,
//       baseline: Load2RegisterImm8Op,
//       constraints: ,
//       pattern: cccc0001u1011111ttttiiii1101iiii,
//       rule: ldrsb_Rule_79_A1_162}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Case10_TestCase10) {
  Load2RegisterImm8OpTester_Case10 baseline_tester;
  NamedLoadBasedImmedMemory_ldrsb_Rule_79_A1_162 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1011111ttttiiii1101iiii");
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'StoreBasedOffsetMemoryDouble',
//       baseline: 'Store3RegisterDoubleOp',
//       constraints: ,
//       pattern: 'cccc000pu0w0nnnntttt00001111mmmm',
//       rule: 'Strd_Rule_201_A1_P398'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx0x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: StoreBasedOffsetMemoryDouble,
//       baseline: Store3RegisterDoubleOp,
//       constraints: ,
//       pattern: cccc000pu0w0nnnntttt00001111mmmm,
//       rule: Strd_Rule_201_A1_P398}
TEST_F(Arm32DecoderStateTests,
       Store3RegisterDoubleOpTester_Case11_TestCase11) {
  Store3RegisterDoubleOpTester_Case11 baseline_tester;
  NamedStoreBasedOffsetMemoryDouble_Strd_Rule_201_A1_P398 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w0nnnntttt00001111mmmm");
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx0x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: 'LoadBasedOffsetMemory',
//       baseline: 'Load3RegisterOp',
//       constraints: ,
//       pattern: 'cccc000pu0w1nnnntttt00001111mmmm',
//       rule: 'Ldrsh_Rule_84_A1_P172'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx0x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx0000xxxxxxxx
//    = {actual: LoadBasedOffsetMemory,
//       baseline: Load3RegisterOp,
//       constraints: ,
//       pattern: cccc000pu0w1nnnntttt00001111mmmm,
//       rule: Ldrsh_Rule_84_A1_P172}
TEST_F(Arm32DecoderStateTests,
       Load3RegisterOpTester_Case12_TestCase12) {
  Load3RegisterOpTester_Case12 baseline_tester;
  NamedLoadBasedOffsetMemory_Ldrsh_Rule_84_A1_P172 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu0w1nnnntttt00001111mmmm");
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x0
//    = {actual: 'StoreBasedImmedMemoryDouble',
//       baseline: 'Store2RegisterImm8DoubleOp',
//       constraints: ,
//       pattern: 'cccc000pu1w0nnnnttttiiii1111iiii',
//       rule: 'Strd_Rule_200_A1_P396'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx1x0
//    = {actual: StoreBasedImmedMemoryDouble,
//       baseline: Store2RegisterImm8DoubleOp,
//       constraints: ,
//       pattern: cccc000pu1w0nnnnttttiiii1111iiii,
//       rule: Strd_Rule_200_A1_P396}
TEST_F(Arm32DecoderStateTests,
       Store2RegisterImm8DoubleOpTester_Case13_TestCase13) {
  Store2RegisterImm8DoubleOpTester_Case13 baseline_tester;
  NamedStoreBasedImmedMemoryDouble_Strd_Rule_200_A1_P396 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w0nnnnttttiiii1111iiii");
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=~1111
//    = {actual: 'LoadBasedImmedMemory',
//       baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       pattern: 'cccc000pu1w1nnnnttttiiii1111iiii',
//       rule: 'Ldrsh_Rule_82_A1_P168'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=~1111
//    = {actual: LoadBasedImmedMemory,
//       baseline: Load2RegisterImm8Op,
//       constraints: ,
//       pattern: cccc000pu1w1nnnnttttiiii1111iiii,
//       rule: Ldrsh_Rule_82_A1_P168}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Case14_TestCase14) {
  Load2RegisterImm8OpTester_Case14 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrsh_Rule_82_A1_P168 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000pu1w1nnnnttttiiii1111iiii");
}

// Neutral case:
// inst(6:5)=11 & inst(24:20)=xx1x1 & inst(19:16)=1111 & inst(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: 'LoadBasedImmedMemory',
//       baseline: 'Load2RegisterImm8Op',
//       constraints: ,
//       pattern: 'cccc0001u1011111ttttiiii1111iiii',
//       rule: 'Ldrsh_Rule_83_A1_P170'}
//
// Representative case:
// op2(6:5)=11 & op1(24:20)=xx1x1 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxx1xx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: LoadBasedImmedMemory,
//       baseline: Load2RegisterImm8Op,
//       constraints: ,
//       pattern: cccc0001u1011111ttttiiii1111iiii,
//       rule: Ldrsh_Rule_83_A1_P170}
TEST_F(Arm32DecoderStateTests,
       Load2RegisterImm8OpTester_Case15_TestCase15) {
  Load2RegisterImm8OpTester_Case15 baseline_tester;
  NamedLoadBasedImmedMemory_Ldrsh_Rule_83_A1_P170 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001u1011111ttttiiii1111iiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
