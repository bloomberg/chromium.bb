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
#include "native_client/src/trusted/validator_arm/arm_helpers.h"

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
// inst(19:16)=0000 & inst(7:6)=01
//    = {baseline: 'CondVfpOp',
//       constraints: }
//
// Representaive case:
// opc2(19:16)=0000 & opc3(7:6)=01
//    = {baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase0
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase0(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~0000
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;
  // opc3(7:6)=~01
  if ((inst.Bits() & 0x000000C0)  !=
          0x00000040) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(19:16)=0000 & inst(7:6)=11
//    = {baseline: 'CondVfpOp',
//       constraints: }
//
// Representaive case:
// opc2(19:16)=0000 & opc3(7:6)=11
//    = {baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase1
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase1(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~0000
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;
  // opc3(7:6)=~11
  if ((inst.Bits() & 0x000000C0)  !=
          0x000000C0) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(19:16)=0001 & inst(7:6)=01
//    = {baseline: 'CondVfpOp',
//       constraints: }
//
// Representaive case:
// opc2(19:16)=0001 & opc3(7:6)=01
//    = {baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase2
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase2(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~0001
  if ((inst.Bits() & 0x000F0000)  !=
          0x00010000) return false;
  // opc3(7:6)=~01
  if ((inst.Bits() & 0x000000C0)  !=
          0x00000040) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(19:16)=0001 & inst(7:6)=11
//    = {baseline: 'CondVfpOp',
//       constraints: }
//
// Representaive case:
// opc2(19:16)=0001 & opc3(7:6)=11
//    = {baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase3
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase3(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~0001
  if ((inst.Bits() & 0x000F0000)  !=
          0x00010000) return false;
  // opc3(7:6)=~11
  if ((inst.Bits() & 0x000000C0)  !=
          0x000000C0) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(19:16)=0100 & inst(7:6)=x1
//    = {baseline: 'CondVfpOp',
//       constraints: }
//
// Representaive case:
// opc2(19:16)=0100 & opc3(7:6)=x1
//    = {baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase4
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase4(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~0100
  if ((inst.Bits() & 0x000F0000)  !=
          0x00040000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(19:16)=0101 & inst(7:6)=x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
//    = {baseline: 'CondVfpOp',
//       constraints: }
//
// Representaive case:
// opc2(19:16)=0101 & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
//    = {baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase5
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase5(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~0101
  if ((inst.Bits() & 0x000F0000)  !=
          0x00050000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
  if ((inst.Bits() & 0x0000002F)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(19:16)=0111 & inst(7:6)=11
//    = {baseline: 'CondVfpOp',
//       constraints: }
//
// Representaive case:
// opc2(19:16)=0111 & opc3(7:6)=11
//    = {baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase6
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase6(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~0111
  if ((inst.Bits() & 0x000F0000)  !=
          0x00070000) return false;
  // opc3(7:6)=~11
  if ((inst.Bits() & 0x000000C0)  !=
          0x000000C0) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(19:16)=1000 & inst(7:6)=x1
//    = {baseline: 'CondVfpOp',
//       constraints: }
//
// Representaive case:
// opc2(19:16)=1000 & opc3(7:6)=x1
//    = {baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase7
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase7(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~1000
  if ((inst.Bits() & 0x000F0000)  !=
          0x00080000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(19:16)=001x & inst(7:6)=x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
//    = {baseline: 'CondVfpOp',
//       constraints: }
//
// Representaive case:
// opc2(19:16)=001x & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
//    = {baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase8
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase8(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~001x
  if ((inst.Bits() & 0x000E0000)  !=
          0x00020000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
  if ((inst.Bits() & 0x00000100)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(19:16)=101x & inst(7:6)=x1
//    = {baseline: 'CondVfpOp',
//       constraints: }
//
// Representaive case:
// opc2(19:16)=101x & opc3(7:6)=x1
//    = {baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase9
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase9(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~101x
  if ((inst.Bits() & 0x000E0000)  !=
          0x000A0000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(19:16)=110x & inst(7:6)=x1
//    = {baseline: 'CondVfpOp',
//       constraints: }
//
// Representaive case:
// opc2(19:16)=110x & opc3(7:6)=x1
//    = {baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase10
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase10(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~110x
  if ((inst.Bits() & 0x000E0000)  !=
          0x000C0000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(19:16)=111x & inst(7:6)=x1
//    = {baseline: 'CondVfpOp',
//       constraints: }
//
// Representaive case:
// opc2(19:16)=111x & opc3(7:6)=x1
//    = {baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase11
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase11(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc2(19:16)=~111x
  if ((inst.Bits() & 0x000E0000)  !=
          0x000E0000) return false;
  // opc3(7:6)=~x1
  if ((inst.Bits() & 0x00000040)  !=
          0x00000040) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// Neutral case:
// inst(7:6)=x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
//    = {baseline: 'CondVfpOp',
//       constraints: }
//
// Representaive case:
// opc3(7:6)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
//    = {baseline: CondVfpOp,
//       constraints: }
class CondVfpOpTesterCase12
    : public CondVfpOpTester {
 public:
  CondVfpOpTesterCase12(const NamedClassDecoder& decoder)
    : CondVfpOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CondVfpOpTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // opc3(7:6)=~x0
  if ((inst.Bits() & 0x00000040)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
  if ((inst.Bits() & 0x000000A0)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return CondVfpOpTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// Neutral case:
// inst(19:16)=0000 & inst(7:6)=01
//    = {baseline: 'CondVfpOp',
//       constraints: ,
//       rule: 'Vmov_Rule_327_A2_P642'}
//
// Representative case:
// opc2(19:16)=0000 & opc3(7:6)=01
//    = {baseline: CondVfpOp,
//       constraints: ,
//       rule: Vmov_Rule_327_A2_P642}
class CondVfpOpTester_Case0
    : public CondVfpOpTesterCase0 {
 public:
  CondVfpOpTester_Case0()
    : CondVfpOpTesterCase0(
      state_.CondVfpOp_Vmov_Rule_327_A2_P642_instance_)
  {}
};

// Neutral case:
// inst(19:16)=0000 & inst(7:6)=11
//    = {baseline: 'CondVfpOp',
//       constraints: ,
//       rule: 'Vabs_Rule_269_A2_P532'}
//
// Representative case:
// opc2(19:16)=0000 & opc3(7:6)=11
//    = {baseline: CondVfpOp,
//       constraints: ,
//       rule: Vabs_Rule_269_A2_P532}
class CondVfpOpTester_Case1
    : public CondVfpOpTesterCase1 {
 public:
  CondVfpOpTester_Case1()
    : CondVfpOpTesterCase1(
      state_.CondVfpOp_Vabs_Rule_269_A2_P532_instance_)
  {}
};

// Neutral case:
// inst(19:16)=0001 & inst(7:6)=01
//    = {baseline: 'CondVfpOp',
//       constraints: ,
//       rule: 'Vneg_Rule_342_A2_P672'}
//
// Representative case:
// opc2(19:16)=0001 & opc3(7:6)=01
//    = {baseline: CondVfpOp,
//       constraints: ,
//       rule: Vneg_Rule_342_A2_P672}
class CondVfpOpTester_Case2
    : public CondVfpOpTesterCase2 {
 public:
  CondVfpOpTester_Case2()
    : CondVfpOpTesterCase2(
      state_.CondVfpOp_Vneg_Rule_342_A2_P672_instance_)
  {}
};

// Neutral case:
// inst(19:16)=0001 & inst(7:6)=11
//    = {baseline: 'CondVfpOp',
//       constraints: ,
//       rule: 'Vsqrt_Rule_388_A1_P762'}
//
// Representative case:
// opc2(19:16)=0001 & opc3(7:6)=11
//    = {baseline: CondVfpOp,
//       constraints: ,
//       rule: Vsqrt_Rule_388_A1_P762}
class CondVfpOpTester_Case3
    : public CondVfpOpTesterCase3 {
 public:
  CondVfpOpTester_Case3()
    : CondVfpOpTesterCase3(
      state_.CondVfpOp_Vsqrt_Rule_388_A1_P762_instance_)
  {}
};

// Neutral case:
// inst(19:16)=0100 & inst(7:6)=x1
//    = {baseline: 'CondVfpOp',
//       constraints: ,
//       rule: 'Vcmp_Vcmpe_Rule_A1'}
//
// Representative case:
// opc2(19:16)=0100 & opc3(7:6)=x1
//    = {baseline: CondVfpOp,
//       constraints: ,
//       rule: Vcmp_Vcmpe_Rule_A1}
class CondVfpOpTester_Case4
    : public CondVfpOpTesterCase4 {
 public:
  CondVfpOpTester_Case4()
    : CondVfpOpTesterCase4(
      state_.CondVfpOp_Vcmp_Vcmpe_Rule_A1_instance_)
  {}
};

// Neutral case:
// inst(19:16)=0101 & inst(7:6)=x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
//    = {baseline: 'CondVfpOp',
//       constraints: ,
//       rule: 'Vcmp_Vcmpe_Rule_A2'}
//
// Representative case:
// opc2(19:16)=0101 & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
//    = {baseline: CondVfpOp,
//       constraints: ,
//       rule: Vcmp_Vcmpe_Rule_A2}
class CondVfpOpTester_Case5
    : public CondVfpOpTesterCase5 {
 public:
  CondVfpOpTester_Case5()
    : CondVfpOpTesterCase5(
      state_.CondVfpOp_Vcmp_Vcmpe_Rule_A2_instance_)
  {}
};

// Neutral case:
// inst(19:16)=0111 & inst(7:6)=11
//    = {baseline: 'CondVfpOp',
//       constraints: ,
//       rule: 'Vcvt_Rule_298_A1_P584'}
//
// Representative case:
// opc2(19:16)=0111 & opc3(7:6)=11
//    = {baseline: CondVfpOp,
//       constraints: ,
//       rule: Vcvt_Rule_298_A1_P584}
class CondVfpOpTester_Case6
    : public CondVfpOpTesterCase6 {
 public:
  CondVfpOpTester_Case6()
    : CondVfpOpTesterCase6(
      state_.CondVfpOp_Vcvt_Rule_298_A1_P584_instance_)
  {}
};

// Neutral case:
// inst(19:16)=1000 & inst(7:6)=x1
//    = {baseline: 'CondVfpOp',
//       constraints: ,
//       rule: 'Vcvt_Vcvtr_Rule_295_A1_P578'}
//
// Representative case:
// opc2(19:16)=1000 & opc3(7:6)=x1
//    = {baseline: CondVfpOp,
//       constraints: ,
//       rule: Vcvt_Vcvtr_Rule_295_A1_P578}
class CondVfpOpTester_Case7
    : public CondVfpOpTesterCase7 {
 public:
  CondVfpOpTester_Case7()
    : CondVfpOpTesterCase7(
      state_.CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578_instance_)
  {}
};

// Neutral case:
// inst(19:16)=001x & inst(7:6)=x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
//    = {baseline: 'CondVfpOp',
//       constraints: ,
//       rule: 'Vcvtb_Vcvtt_Rule_300_A1_P588'}
//
// Representative case:
// opc2(19:16)=001x & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
//    = {baseline: CondVfpOp,
//       constraints: ,
//       rule: Vcvtb_Vcvtt_Rule_300_A1_P588}
class CondVfpOpTester_Case8
    : public CondVfpOpTesterCase8 {
 public:
  CondVfpOpTester_Case8()
    : CondVfpOpTesterCase8(
      state_.CondVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588_instance_)
  {}
};

// Neutral case:
// inst(19:16)=101x & inst(7:6)=x1
//    = {baseline: 'CondVfpOp',
//       constraints: ,
//       rule: 'Vcvt_Rule_297_A1_P582'}
//
// Representative case:
// opc2(19:16)=101x & opc3(7:6)=x1
//    = {baseline: CondVfpOp,
//       constraints: ,
//       rule: Vcvt_Rule_297_A1_P582}
class CondVfpOpTester_Case9
    : public CondVfpOpTesterCase9 {
 public:
  CondVfpOpTester_Case9()
    : CondVfpOpTesterCase9(
      state_.CondVfpOp_Vcvt_Rule_297_A1_P582_instance_)
  {}
};

// Neutral case:
// inst(19:16)=110x & inst(7:6)=x1
//    = {baseline: 'CondVfpOp',
//       constraints: ,
//       rule: 'Vcvt_Vcvtr_Rule_295_A1_P578'}
//
// Representative case:
// opc2(19:16)=110x & opc3(7:6)=x1
//    = {baseline: CondVfpOp,
//       constraints: ,
//       rule: Vcvt_Vcvtr_Rule_295_A1_P578}
class CondVfpOpTester_Case10
    : public CondVfpOpTesterCase10 {
 public:
  CondVfpOpTester_Case10()
    : CondVfpOpTesterCase10(
      state_.CondVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578_instance_)
  {}
};

// Neutral case:
// inst(19:16)=111x & inst(7:6)=x1
//    = {baseline: 'CondVfpOp',
//       constraints: ,
//       rule: 'Vcvt_Rule_297_A1_P582'}
//
// Representative case:
// opc2(19:16)=111x & opc3(7:6)=x1
//    = {baseline: CondVfpOp,
//       constraints: ,
//       rule: Vcvt_Rule_297_A1_P582}
class CondVfpOpTester_Case11
    : public CondVfpOpTesterCase11 {
 public:
  CondVfpOpTester_Case11()
    : CondVfpOpTesterCase11(
      state_.CondVfpOp_Vcvt_Rule_297_A1_P582_instance_)
  {}
};

// Neutral case:
// inst(7:6)=x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
//    = {baseline: 'CondVfpOp',
//       constraints: ,
//       rule: 'Vmov_Rule_326_A2_P640'}
//
// Representative case:
// opc3(7:6)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
//    = {baseline: CondVfpOp,
//       constraints: ,
//       rule: Vmov_Rule_326_A2_P640}
class CondVfpOpTester_Case12
    : public CondVfpOpTesterCase12 {
 public:
  CondVfpOpTester_Case12()
    : CondVfpOpTesterCase12(
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
// inst(19:16)=0000 & inst(7:6)=01
//    = {actual: 'VfpOp',
//       baseline: 'CondVfpOp',
//       constraints: ,
//       pattern: 'cccc11101d110000dddd101s01m0mmmm',
//       rule: 'Vmov_Rule_327_A2_P642'}
//
// Representative case:
// opc2(19:16)=0000 & opc3(7:6)=01
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d110000dddd101s01m0mmmm,
//       rule: Vmov_Rule_327_A2_P642}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case0_TestCase0) {
  CondVfpOpTester_Case0 baseline_tester;
  NamedVfpOp_Vmov_Rule_327_A2_P642 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110000dddd101s01m0mmmm");
}

// Neutral case:
// inst(19:16)=0000 & inst(7:6)=11
//    = {actual: 'VfpOp',
//       baseline: 'CondVfpOp',
//       constraints: ,
//       pattern: 'cccc11101d110000dddd101s11m0mmmm',
//       rule: 'Vabs_Rule_269_A2_P532'}
//
// Representative case:
// opc2(19:16)=0000 & opc3(7:6)=11
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d110000dddd101s11m0mmmm,
//       rule: Vabs_Rule_269_A2_P532}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case1_TestCase1) {
  CondVfpOpTester_Case1 baseline_tester;
  NamedVfpOp_Vabs_Rule_269_A2_P532 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110000dddd101s11m0mmmm");
}

// Neutral case:
// inst(19:16)=0001 & inst(7:6)=01
//    = {actual: 'VfpOp',
//       baseline: 'CondVfpOp',
//       constraints: ,
//       pattern: 'cccc11101d110001dddd101s01m0mmmm',
//       rule: 'Vneg_Rule_342_A2_P672'}
//
// Representative case:
// opc2(19:16)=0001 & opc3(7:6)=01
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d110001dddd101s01m0mmmm,
//       rule: Vneg_Rule_342_A2_P672}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case2_TestCase2) {
  CondVfpOpTester_Case2 baseline_tester;
  NamedVfpOp_Vneg_Rule_342_A2_P672 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110001dddd101s01m0mmmm");
}

// Neutral case:
// inst(19:16)=0001 & inst(7:6)=11
//    = {actual: 'VfpOp',
//       baseline: 'CondVfpOp',
//       constraints: ,
//       pattern: 'cccc11101d110001dddd101s11m0mmmm',
//       rule: 'Vsqrt_Rule_388_A1_P762'}
//
// Representative case:
// opc2(19:16)=0001 & opc3(7:6)=11
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d110001dddd101s11m0mmmm,
//       rule: Vsqrt_Rule_388_A1_P762}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case3_TestCase3) {
  CondVfpOpTester_Case3 baseline_tester;
  NamedVfpOp_Vsqrt_Rule_388_A1_P762 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110001dddd101s11m0mmmm");
}

// Neutral case:
// inst(19:16)=0100 & inst(7:6)=x1
//    = {actual: 'VfpOp',
//       baseline: 'CondVfpOp',
//       constraints: ,
//       pattern: 'cccc11101d110100dddd101se1m0mmmm',
//       rule: 'Vcmp_Vcmpe_Rule_A1'}
//
// Representative case:
// opc2(19:16)=0100 & opc3(7:6)=x1
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d110100dddd101se1m0mmmm,
//       rule: Vcmp_Vcmpe_Rule_A1}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case4_TestCase4) {
  CondVfpOpTester_Case4 baseline_tester;
  NamedVfpOp_Vcmp_Vcmpe_Rule_A1 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110100dddd101se1m0mmmm");
}

// Neutral case:
// inst(19:16)=0101 & inst(7:6)=x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
//    = {actual: 'VfpOp',
//       baseline: 'CondVfpOp',
//       constraints: ,
//       pattern: 'cccc11101d110101dddd101se1000000',
//       rule: 'Vcmp_Vcmpe_Rule_A2'}
//
// Representative case:
// opc2(19:16)=0101 & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxxxx0x0000
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d110101dddd101se1000000,
//       rule: Vcmp_Vcmpe_Rule_A2}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case5_TestCase5) {
  CondVfpOpTester_Case5 baseline_tester;
  NamedVfpOp_Vcmp_Vcmpe_Rule_A2 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110101dddd101se1000000");
}

// Neutral case:
// inst(19:16)=0111 & inst(7:6)=11
//    = {actual: 'VfpOp',
//       baseline: 'CondVfpOp',
//       constraints: ,
//       pattern: 'cccc11101d110111dddd101s11m0mmmm',
//       rule: 'Vcvt_Rule_298_A1_P584'}
//
// Representative case:
// opc2(19:16)=0111 & opc3(7:6)=11
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d110111dddd101s11m0mmmm,
//       rule: Vcvt_Rule_298_A1_P584}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case6_TestCase6) {
  CondVfpOpTester_Case6 baseline_tester;
  NamedVfpOp_Vcvt_Rule_298_A1_P584 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d110111dddd101s11m0mmmm");
}

// Neutral case:
// inst(19:16)=1000 & inst(7:6)=x1
//    = {actual: 'VfpOp',
//       baseline: 'CondVfpOp',
//       constraints: ,
//       pattern: 'cccc11101d111000dddd101sp1m0mmmm',
//       rule: 'Vcvt_Vcvtr_Rule_295_A1_P578'}
//
// Representative case:
// opc2(19:16)=1000 & opc3(7:6)=x1
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d111000dddd101sp1m0mmmm,
//       rule: Vcvt_Vcvtr_Rule_295_A1_P578}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case7_TestCase7) {
  CondVfpOpTester_Case7 baseline_tester;
  NamedVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d111000dddd101sp1m0mmmm");
}

// Neutral case:
// inst(19:16)=001x & inst(7:6)=x1 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
//    = {actual: 'VfpOp',
//       baseline: 'CondVfpOp',
//       constraints: ,
//       pattern: 'cccc11101d11001odddd1010t1m0mmmm',
//       rule: 'Vcvtb_Vcvtt_Rule_300_A1_P588'}
//
// Representative case:
// opc2(19:16)=001x & opc3(7:6)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxx0xxxxxxxx
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d11001odddd1010t1m0mmmm,
//       rule: Vcvtb_Vcvtt_Rule_300_A1_P588}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case8_TestCase8) {
  CondVfpOpTester_Case8 baseline_tester;
  NamedVfpOp_Vcvtb_Vcvtt_Rule_300_A1_P588 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11001odddd1010t1m0mmmm");
}

// Neutral case:
// inst(19:16)=101x & inst(7:6)=x1
//    = {actual: 'VfpOp',
//       baseline: 'CondVfpOp',
//       constraints: ,
//       pattern: 'cccc11101d11101udddd101fx1i0iiii',
//       rule: 'Vcvt_Rule_297_A1_P582'}
//
// Representative case:
// opc2(19:16)=101x & opc3(7:6)=x1
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d11101udddd101fx1i0iiii,
//       rule: Vcvt_Rule_297_A1_P582}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case9_TestCase9) {
  CondVfpOpTester_Case9 baseline_tester;
  NamedVfpOp_Vcvt_Rule_297_A1_P582 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11101udddd101fx1i0iiii");
}

// Neutral case:
// inst(19:16)=110x & inst(7:6)=x1
//    = {actual: 'VfpOp',
//       baseline: 'CondVfpOp',
//       constraints: ,
//       pattern: 'cccc11101d11110xdddd101sp1m0mmmm',
//       rule: 'Vcvt_Vcvtr_Rule_295_A1_P578'}
//
// Representative case:
// opc2(19:16)=110x & opc3(7:6)=x1
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d11110xdddd101sp1m0mmmm,
//       rule: Vcvt_Vcvtr_Rule_295_A1_P578}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case10_TestCase10) {
  CondVfpOpTester_Case10 baseline_tester;
  NamedVfpOp_Vcvt_Vcvtr_Rule_295_A1_P578 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11110xdddd101sp1m0mmmm");
}

// Neutral case:
// inst(19:16)=111x & inst(7:6)=x1
//    = {actual: 'VfpOp',
//       baseline: 'CondVfpOp',
//       constraints: ,
//       pattern: 'cccc11101d11111udddd101fx1i0iiii',
//       rule: 'Vcvt_Rule_297_A1_P582'}
//
// Representative case:
// opc2(19:16)=111x & opc3(7:6)=x1
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d11111udddd101fx1i0iiii,
//       rule: Vcvt_Rule_297_A1_P582}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case11_TestCase11) {
  CondVfpOpTester_Case11 baseline_tester;
  NamedVfpOp_Vcvt_Rule_297_A1_P582 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11111udddd101fx1i0iiii");
}

// Neutral case:
// inst(7:6)=x0 & inst(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
//    = {actual: 'VfpOp',
//       baseline: 'CondVfpOp',
//       constraints: ,
//       pattern: 'cccc11101d11iiiidddd101s0000iiii',
//       rule: 'Vmov_Rule_326_A2_P640'}
//
// Representative case:
// opc3(7:6)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxxxxxx0x0xxxxx
//    = {actual: VfpOp,
//       baseline: CondVfpOp,
//       constraints: ,
//       pattern: cccc11101d11iiiidddd101s0000iiii,
//       rule: Vmov_Rule_326_A2_P640}
TEST_F(Arm32DecoderStateTests,
       CondVfpOpTester_Case12_TestCase12) {
  CondVfpOpTester_Case12 baseline_tester;
  NamedVfpOp_Vmov_Rule_326_A2_P640 actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc11101d11iiiidddd101s0000iiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
