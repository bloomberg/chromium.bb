/*
 * Copyright 2013 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif


#include "gtest/gtest.h"
#include "native_client/src/trusted/validator_arm/actual_vs_baseline.h"
#include "native_client/src/trusted/validator_arm/arm_helpers.h"
#include "native_client/src/trusted/validator_arm/gen/arm32_decode_named_bases.h"

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


// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       pattern: cccc00010001nnnn0000iiiiitt0mmmm,
//       rule: TST_register,
//       uses: {Rn, Rm}}
class TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0TesterCase0
    : public Arm32DecoderTester {
 public:
  TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0TesterCase0(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0TesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~10001
  if ((inst.Bits() & 0x01F00000)  !=
          0x01100000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       pattern: cccc00010011nnnn0000iiiiitt0mmmm,
//       rule: TEQ_register,
//       uses: {Rn, Rm}}
class TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0TesterCase1
    : public Arm32DecoderTester {
 public:
  TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0TesterCase1(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0TesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~10011
  if ((inst.Bits() & 0x01F00000)  !=
          0x01300000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       pattern: cccc00010101nnnn0000iiiiitt0mmmm,
//       rule: CMP_register,
//       uses: {Rn, Rm}}
class CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0TesterCase2
    : public Arm32DecoderTester {
 public:
  CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0TesterCase2(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0TesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~10101
  if ((inst.Bits() & 0x01F00000)  !=
          0x01500000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       pattern: cccc00010111nnnn0000iiiiitt0mmmm,
//       rule: CMN_register,
//       uses: {Rn, Rm}}
class CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0TesterCase3
    : public Arm32DecoderTester {
 public:
  CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0TesterCase3(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0TesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~10111
  if ((inst.Bits() & 0x01F00000)  !=
          0x01700000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000000snnnnddddiiiiitt0mmmm,
//       rule: AND_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0TesterCase4
    : public Arm32DecoderTester {
 public:
  AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0TesterCase4(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0TesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0000x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000001snnnnddddiiiiitt0mmmm,
//       rule: EOR_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0TesterCase5
    : public Arm32DecoderTester {
 public:
  EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0TesterCase5(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0TesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0001x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00200000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0010x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000010snnnnddddiiiiitt0mmmm,
//       rule: SUB_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0TesterCase6
    : public Arm32DecoderTester {
 public:
  SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0TesterCase6(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0TesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0010x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00400000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000011snnnnddddiiiiitt0mmmm,
//       rule: RSB_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0TesterCase7
    : public Arm32DecoderTester {
 public:
  RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0TesterCase7(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0TesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0011x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00600000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000100snnnnddddiiiiitt0mmmm,
//       rule: ADD_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0TesterCase8
    : public Arm32DecoderTester {
 public:
  ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0TesterCase8(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0TesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0100x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00800000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000101snnnnddddiiiiitt0mmmm,
//       rule: ADC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0TesterCase9
    : public Arm32DecoderTester {
 public:
  ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0TesterCase9(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0TesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00A00000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000110snnnnddddiiiiitt0mmmm,
//       rule: SBC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0TesterCase10
    : public Arm32DecoderTester {
 public:
  SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0TesterCase10(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0TesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0110x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00C00000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000111snnnnddddiiiiitt0mmmm,
//       rule: RSC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0TesterCase11
    : public Arm32DecoderTester {
 public:
  RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0TesterCase11(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0TesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~0111x
  if ((inst.Bits() & 0x01E00000)  !=
          0x00E00000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001100snnnnddddiiiiitt0mmmm,
//       rule: ORR_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0TesterCase12
    : public Arm32DecoderTester {
 public:
  ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0TesterCase12(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0TesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1100x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01800000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1,
//       baseline: LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7), Rm(3:0)],
//       imm5: imm5(11:7),
//       pattern: cccc0001101s0000ddddiiiii000mmmm,
//       rule: LSL_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0TesterCase13
    : public Arm32DecoderTester {
 public:
  LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0TesterCase13(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0TesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op2(11:7)=00000
  if ((inst.Bits() & 0x00000F80)  ==
          0x00000000) return false;
  // op3(6:5)=~00
  if ((inst.Bits() & 0x00000060)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1,
//       baseline: ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7), Rm(3:0)],
//       imm5: imm5(11:7),
//       pattern: cccc0001101s0000ddddiiiii110mmmm,
//       rule: ROR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0TesterCase14
    : public Arm32DecoderTester {
 public:
  ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0TesterCase14(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0TesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op2(11:7)=00000
  if ((inst.Bits() & 0x00000F80)  ==
          0x00000000) return false;
  // op3(6:5)=~11
  if ((inst.Bits() & 0x00000060)  !=
          0x00000060) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: MOV_register_cccc0001101s0000dddd00000000mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001101s0000dddd00000000mmmm,
//       rule: MOV_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class MOV_register_cccc0001101s0000dddd00000000mmmm_case_0TesterCase15
    : public Arm32DecoderTester {
 public:
  MOV_register_cccc0001101s0000dddd00000000mmmm_case_0TesterCase15(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MOV_register_cccc0001101s0000dddd00000000mmmm_case_0TesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op2(11:7)=~00000
  if ((inst.Bits() & 0x00000F80)  !=
          0x00000000) return false;
  // op3(6:5)=~00
  if ((inst.Bits() & 0x00000060)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: RRX_cccc0001101s0000dddd00000110mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001101s0000dddd00000110mmmm,
//       rule: RRX,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class RRX_cccc0001101s0000dddd00000110mmmm_case_0TesterCase16
    : public Arm32DecoderTester {
 public:
  RRX_cccc0001101s0000dddd00000110mmmm_case_0TesterCase16(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool RRX_cccc0001101s0000dddd00000110mmmm_case_0TesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op2(11:7)=~00000
  if ((inst.Bits() & 0x00000F80)  !=
          0x00000000) return false;
  // op3(6:5)=~11
  if ((inst.Bits() & 0x00000060)  !=
          0x00000060) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op3(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001101s0000ddddiiiii010mmmm,
//       rule: LSR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0TesterCase17
    : public Arm32DecoderTester {
 public:
  LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0TesterCase17(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0TesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op3(6:5)=~01
  if ((inst.Bits() & 0x00000060)  !=
          0x00000020) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1101x & op3(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001101s0000ddddiiiii100mmmm,
//       rule: ASR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0TesterCase18
    : public Arm32DecoderTester {
 public:
  ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0TesterCase18(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0TesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1101x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01A00000) return false;
  // op3(6:5)=~10
  if ((inst.Bits() & 0x00000060)  !=
          0x00000040) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001110snnnnddddiiiiitt0mmmm,
//       rule: BIC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0TesterCase19
    : public Arm32DecoderTester {
 public:
  BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0TesterCase19(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0TesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1110x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01C00000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001111s0000ddddiiiiitt0mmmm,
//       rule: MVN_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0TesterCase20
    : public Arm32DecoderTester {
 public:
  MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0TesterCase20(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0TesterCase20
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(24:20)=~1111x
  if ((inst.Bits() & 0x01E00000)  !=
          0x01E00000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x000F0000)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       pattern: cccc00010001nnnn0000iiiiitt0mmmm,
//       rule: TST_register,
//       uses: {Rn, Rm}}
class TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0Tester_Case0
    : public TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0TesterCase0 {
 public:
  TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0Tester_Case0()
    : TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0TesterCase0(
      state_.TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0_TST_register_instance_)
  {}
};

// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       pattern: cccc00010011nnnn0000iiiiitt0mmmm,
//       rule: TEQ_register,
//       uses: {Rn, Rm}}
class TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0Tester_Case1
    : public TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0TesterCase1 {
 public:
  TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0Tester_Case1()
    : TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0TesterCase1(
      state_.TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0_TEQ_register_instance_)
  {}
};

// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       pattern: cccc00010101nnnn0000iiiiitt0mmmm,
//       rule: CMP_register,
//       uses: {Rn, Rm}}
class CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0Tester_Case2
    : public CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0TesterCase2 {
 public:
  CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0Tester_Case2()
    : CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0TesterCase2(
      state_.CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0_CMP_register_instance_)
  {}
};

// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       pattern: cccc00010111nnnn0000iiiiitt0mmmm,
//       rule: CMN_register,
//       uses: {Rn, Rm}}
class CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0Tester_Case3
    : public CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0TesterCase3 {
 public:
  CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0Tester_Case3()
    : CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0TesterCase3(
      state_.CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0_CMN_register_instance_)
  {}
};

// op1(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000000snnnnddddiiiiitt0mmmm,
//       rule: AND_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0Tester_Case4
    : public AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0TesterCase4 {
 public:
  AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0Tester_Case4()
    : AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0TesterCase4(
      state_.AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0_AND_register_instance_)
  {}
};

// op1(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000001snnnnddddiiiiitt0mmmm,
//       rule: EOR_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0Tester_Case5
    : public EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0TesterCase5 {
 public:
  EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0Tester_Case5()
    : EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0TesterCase5(
      state_.EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0_EOR_register_instance_)
  {}
};

// op1(24:20)=0010x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000010snnnnddddiiiiitt0mmmm,
//       rule: SUB_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0Tester_Case6
    : public SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0TesterCase6 {
 public:
  SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0Tester_Case6()
    : SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0TesterCase6(
      state_.SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0_SUB_register_instance_)
  {}
};

// op1(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000011snnnnddddiiiiitt0mmmm,
//       rule: RSB_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0Tester_Case7
    : public RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0TesterCase7 {
 public:
  RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0Tester_Case7()
    : RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0TesterCase7(
      state_.RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0_RSB_register_instance_)
  {}
};

// op1(24:20)=0100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000100snnnnddddiiiiitt0mmmm,
//       rule: ADD_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0Tester_Case8
    : public ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0TesterCase8 {
 public:
  ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0Tester_Case8()
    : ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0TesterCase8(
      state_.ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0_ADD_register_instance_)
  {}
};

// op1(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000101snnnnddddiiiiitt0mmmm,
//       rule: ADC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0Tester_Case9
    : public ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0TesterCase9 {
 public:
  ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0Tester_Case9()
    : ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0TesterCase9(
      state_.ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0_ADC_register_instance_)
  {}
};

// op1(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000110snnnnddddiiiiitt0mmmm,
//       rule: SBC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0Tester_Case10
    : public SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0TesterCase10 {
 public:
  SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0Tester_Case10()
    : SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0TesterCase10(
      state_.SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0_SBC_register_instance_)
  {}
};

// op1(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000111snnnnddddiiiiitt0mmmm,
//       rule: RSC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0Tester_Case11
    : public RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0TesterCase11 {
 public:
  RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0Tester_Case11()
    : RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0TesterCase11(
      state_.RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0_RSC_register_instance_)
  {}
};

// op1(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001100snnnnddddiiiiitt0mmmm,
//       rule: ORR_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0Tester_Case12
    : public ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0TesterCase12 {
 public:
  ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0Tester_Case12()
    : ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0TesterCase12(
      state_.ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0_ORR_register_instance_)
  {}
};

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1,
//       baseline: LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7), Rm(3:0)],
//       imm5: imm5(11:7),
//       pattern: cccc0001101s0000ddddiiiii000mmmm,
//       rule: LSL_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0Tester_Case13
    : public LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0TesterCase13 {
 public:
  LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0Tester_Case13()
    : LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0TesterCase13(
      state_.LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0_LSL_immediate_instance_)
  {}
};

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1,
//       baseline: ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7), Rm(3:0)],
//       imm5: imm5(11:7),
//       pattern: cccc0001101s0000ddddiiiii110mmmm,
//       rule: ROR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0Tester_Case14
    : public ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0TesterCase14 {
 public:
  ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0Tester_Case14()
    : ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0TesterCase14(
      state_.ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0_ROR_immediate_instance_)
  {}
};

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: MOV_register_cccc0001101s0000dddd00000000mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001101s0000dddd00000000mmmm,
//       rule: MOV_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class MOV_register_cccc0001101s0000dddd00000000mmmm_case_0Tester_Case15
    : public MOV_register_cccc0001101s0000dddd00000000mmmm_case_0TesterCase15 {
 public:
  MOV_register_cccc0001101s0000dddd00000000mmmm_case_0Tester_Case15()
    : MOV_register_cccc0001101s0000dddd00000000mmmm_case_0TesterCase15(
      state_.MOV_register_cccc0001101s0000dddd00000000mmmm_case_0_MOV_register_instance_)
  {}
};

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: RRX_cccc0001101s0000dddd00000110mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001101s0000dddd00000110mmmm,
//       rule: RRX,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class RRX_cccc0001101s0000dddd00000110mmmm_case_0Tester_Case16
    : public RRX_cccc0001101s0000dddd00000110mmmm_case_0TesterCase16 {
 public:
  RRX_cccc0001101s0000dddd00000110mmmm_case_0Tester_Case16()
    : RRX_cccc0001101s0000dddd00000110mmmm_case_0TesterCase16(
      state_.RRX_cccc0001101s0000dddd00000110mmmm_case_0_RRX_instance_)
  {}
};

// op1(24:20)=1101x & op3(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001101s0000ddddiiiii010mmmm,
//       rule: LSR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0Tester_Case17
    : public LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0TesterCase17 {
 public:
  LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0Tester_Case17()
    : LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0TesterCase17(
      state_.LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0_LSR_immediate_instance_)
  {}
};

// op1(24:20)=1101x & op3(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001101s0000ddddiiiii100mmmm,
//       rule: ASR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0Tester_Case18
    : public ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0TesterCase18 {
 public:
  ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0Tester_Case18()
    : ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0TesterCase18(
      state_.ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0_ASR_immediate_instance_)
  {}
};

// op1(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001110snnnnddddiiiiitt0mmmm,
//       rule: BIC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
class BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0Tester_Case19
    : public BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0TesterCase19 {
 public:
  BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0Tester_Case19()
    : BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0TesterCase19(
      state_.BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0_BIC_register_instance_)
  {}
};

// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001111s0000ddddiiiiitt0mmmm,
//       rule: MVN_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
class MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0Tester_Case20
    : public MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0TesterCase20 {
 public:
  MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0Tester_Case20()
    : MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0TesterCase20(
      state_.MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0_MVN_register_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// op1(24:20)=10001 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       pattern: cccc00010001nnnn0000iiiiitt0mmmm,
//       rule: TST_register,
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0Tester_Case0_TestCase0) {
  TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0Tester_Case0 baseline_tester;
  NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_TST_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010001nnnn0000iiiiitt0mmmm");
}

// op1(24:20)=10011 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       pattern: cccc00010011nnnn0000iiiiitt0mmmm,
//       rule: TEQ_register,
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0Tester_Case1_TestCase1) {
  TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0Tester_Case1 baseline_tester;
  NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_TEQ_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010011nnnn0000iiiiitt0mmmm");
}

// op1(24:20)=10101 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       pattern: cccc00010101nnnn0000iiiiitt0mmmm,
//       rule: CMP_register,
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0Tester_Case2_TestCase2) {
  CMP_register_cccc00010101nnnn0000iiiiitt0mmmm_case_0Tester_Case2 baseline_tester;
  NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_CMP_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010101nnnn0000iiiiitt0mmmm");
}

// op1(24:20)=10111 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000xxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
//       baseline: CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0,
//       defs: {NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rm(3:0)],
//       pattern: cccc00010111nnnn0000iiiiitt0mmmm,
//       rule: CMN_register,
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0Tester_Case3_TestCase3) {
  CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_0Tester_Case3 baseline_tester;
  NamedActual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1_CMN_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010111nnnn0000iiiiitt0mmmm");
}

// op1(24:20)=0000x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000000snnnnddddiiiiitt0mmmm,
//       rule: AND_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0Tester_Case4_TestCase4) {
  AND_register_cccc0000000snnnnddddiiiiitt0mmmm_case_0Tester_Case4 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_AND_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000000snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0001x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000001snnnnddddiiiiitt0mmmm,
//       rule: EOR_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0Tester_Case5_TestCase5) {
  EOR_register_cccc0000001snnnnddddiiiiitt0mmmm_case_0Tester_Case5 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_EOR_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000001snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0010x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000010snnnnddddiiiiitt0mmmm,
//       rule: SUB_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0Tester_Case6_TestCase6) {
  SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0Tester_Case6 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_SUB_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000010snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0011x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000011snnnnddddiiiiitt0mmmm,
//       rule: RSB_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0Tester_Case7_TestCase7) {
  RSB_register_cccc0000011snnnnddddiiiiitt0mmmm_case_0Tester_Case7 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_RSB_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000011snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000100snnnnddddiiiiitt0mmmm,
//       rule: ADD_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0Tester_Case8_TestCase8) {
  ADD_register_cccc0000100snnnnddddiiiiitt0mmmm_case_0Tester_Case8 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ADD_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000100snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0101x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000101snnnnddddiiiiitt0mmmm,
//       rule: ADC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0Tester_Case9_TestCase9) {
  ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_0Tester_Case9 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ADC_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000101snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000110snnnnddddiiiiitt0mmmm,
//       rule: SBC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0Tester_Case10_TestCase10) {
  SBC_register_cccc0000110snnnnddddiiiiitt0mmmm_case_0Tester_Case10 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_SBC_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000110snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=0111x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0000111snnnnddddiiiiitt0mmmm,
//       rule: RSC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0Tester_Case11_TestCase11) {
  RSC_register_cccc0000111snnnnddddiiiiitt0mmmm_case_0Tester_Case11 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_RSC_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0000111snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=1100x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001100snnnnddddiiiiitt0mmmm,
//       rule: ORR_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0Tester_Case12_TestCase12) {
  ORR_register_cccc0001100snnnnddddiiiiitt0mmmm_case_0Tester_Case12 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_ORR_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001100snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1,
//       baseline: LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7), Rm(3:0)],
//       imm5: imm5(11:7),
//       pattern: cccc0001101s0000ddddiiiii000mmmm,
//       rule: LSL_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0Tester_Case13_TestCase13) {
  LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_0Tester_Case13 baseline_tester;
  NamedActual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_LSL_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddiiiii000mmmm");
}

// op1(24:20)=1101x & op2(11:7)=~00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1,
//       baseline: ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), imm5(11:7), Rm(3:0)],
//       imm5: imm5(11:7),
//       pattern: cccc0001101s0000ddddiiiii110mmmm,
//       rule: ROR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         imm5(11:7)=00000 => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0Tester_Case14_TestCase14) {
  ROR_immediate_cccc0001101s0000ddddiiiii110mmmm_case_0Tester_Case14 baseline_tester;
  NamedActual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1_ROR_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddiiiii110mmmm");
}

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=00 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: MOV_register_cccc0001101s0000dddd00000000mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001101s0000dddd00000000mmmm,
//       rule: MOV_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       MOV_register_cccc0001101s0000dddd00000000mmmm_case_0Tester_Case15_TestCase15) {
  MOV_register_cccc0001101s0000dddd00000000mmmm_case_0Tester_Case15 baseline_tester;
  NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_MOV_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000dddd00000000mmmm");
}

// op1(24:20)=1101x & op2(11:7)=00000 & op3(6:5)=11 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: RRX_cccc0001101s0000dddd00000110mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001101s0000dddd00000110mmmm,
//       rule: RRX,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       RRX_cccc0001101s0000dddd00000110mmmm_case_0Tester_Case16_TestCase16) {
  RRX_cccc0001101s0000dddd00000110mmmm_case_0Tester_Case16 baseline_tester;
  NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_RRX actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000dddd00000110mmmm");
}

// op1(24:20)=1101x & op3(6:5)=01 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001101s0000ddddiiiii010mmmm,
//       rule: LSR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0Tester_Case17_TestCase17) {
  LSR_immediate_cccc0001101s0000ddddiiiii010mmmm_case_0Tester_Case17 baseline_tester;
  NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_LSR_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddiiiii010mmmm");
}

// op1(24:20)=1101x & op3(6:5)=10 & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001101s0000ddddiiiii100mmmm,
//       rule: ASR_immediate,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0Tester_Case18_TestCase18) {
  ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_0Tester_Case18 baseline_tester;
  NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_ASR_immediate actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001101s0000ddddiiiii100mmmm");
}

// op1(24:20)=1110x
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       S: S(20),
//       actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
//       baseline: BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rn(19:16), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001110snnnnddddiiiiitt0mmmm,
//       rule: BIC_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rn, Rm}}
TEST_F(Arm32DecoderStateTests,
       BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0Tester_Case19_TestCase19) {
  BIC_register_cccc0001110snnnnddddiiiiitt0mmmm_case_0Tester_Case19 baseline_tester;
  NamedActual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1_BIC_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001110snnnnddddiiiiitt0mmmm");
}

// op1(24:20)=1111x & $pattern(31:0)=xxxxxxxxxxxx0000xxxxxxxxxxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       S: S(20),
//       actual: Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1,
//       baseline: MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0,
//       defs: {Rd, NZCV
//            if S
//            else None},
//       fields: [S(20), Rd(15:12), Rm(3:0)],
//       pattern: cccc0001111s0000ddddiiiiitt0mmmm,
//       rule: MVN_register,
//       safety: [(Rd(15:12)=1111 &&
//            S(20)=1) => DECODER_ERROR,
//         Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0Tester_Case20_TestCase20) {
  MVN_register_cccc0001111s0000ddddiiiiitt0mmmm_case_0Tester_Case20 baseline_tester;
  NamedActual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1_MVN_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001111s0000ddddiiiiitt0mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
