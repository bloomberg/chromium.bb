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
#include "native_client/src/trusted/validator_arm/baseline_vs_baseline.h"
#include "native_client/src/trusted/validator_arm/baseline_classes.h"
#include "native_client/src/trusted/validator_arm/inst_classes_testers.h"
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


// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rn: Rn(3:0),
//       actual: Actual_MSR_register_cccc00010010mm00111100000000nnnn_case_1,
//       baseline: MSR_register_cccc00010010mm00111100000000nnnn_case_0,
//       constraints: ,
//       defs: {NZCV
//            if write_nzcvq
//            else None},
//       fields: [mask(19:18), Rn(3:0)],
//       mask: mask(19:18),
//       pattern: cccc00010010mm00111100000000nnnn,
//       rule: MSR_register,
//       safety: [mask(19:18)=00 => UNPREDICTABLE,
//         Rn  ==
//               Pc => UNPREDICTABLE],
//       uses: {Rn},
//       write_nzcvq: mask(1)=1}
class MSR_register_cccc00010010mm00111100000000nnnn_case_0TesterCase0
    : public Arm32DecoderTester {
 public:
  MSR_register_cccc00010010mm00111100000000nnnn_case_0TesterCase0(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MSR_register_cccc00010010mm00111100000000nnnn_case_0TesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~000
  if ((inst.Bits() & 0x00000070)  !=
          0x00000000) return false;
  // B(9)=~0
  if ((inst.Bits() & 0x00000200)  !=
          0x00000000) return false;
  // op(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // op1(19:16)=~xx00
  if ((inst.Bits() & 0x00030000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx
  if ((inst.Bits() & 0x0000FD00)  !=
          0x0000F000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm111100000000nnnn,
//       rule: MSR_register}
class ForbiddenCondDecoderTesterCase1
    : public UnsafeCondDecoderTester {
 public:
  ForbiddenCondDecoderTesterCase1(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool ForbiddenCondDecoderTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~000
  if ((inst.Bits() & 0x00000070)  !=
          0x00000000) return false;
  // B(9)=~0
  if ((inst.Bits() & 0x00000200)  !=
          0x00000000) return false;
  // op(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // op1(19:16)=~xx01
  if ((inst.Bits() & 0x00030000)  !=
          0x00010000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx
  if ((inst.Bits() & 0x0000FD00)  !=
          0x0000F000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm111100000000nnnn,
//       rule: MSR_register}
class ForbiddenCondDecoderTesterCase2
    : public UnsafeCondDecoderTester {
 public:
  ForbiddenCondDecoderTesterCase2(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool ForbiddenCondDecoderTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~000
  if ((inst.Bits() & 0x00000070)  !=
          0x00000000) return false;
  // B(9)=~0
  if ((inst.Bits() & 0x00000200)  !=
          0x00000000) return false;
  // op(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // op1(19:16)=~xx1x
  if ((inst.Bits() & 0x00020000)  !=
          0x00020000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx
  if ((inst.Bits() & 0x0000FD00)  !=
          0x0000F000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm111100000000nnnn,
//       rule: MSR_register}
class ForbiddenCondDecoderTesterCase3
    : public UnsafeCondDecoderTester {
 public:
  ForbiddenCondDecoderTesterCase3(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool ForbiddenCondDecoderTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~000
  if ((inst.Bits() & 0x00000070)  !=
          0x00000000) return false;
  // B(9)=~0
  if ((inst.Bits() & 0x00000200)  !=
          0x00000000) return false;
  // op(22:21)=~11
  if ((inst.Bits() & 0x00600000)  !=
          0x00600000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx111100x0xxxxxxxx
  if ((inst.Bits() & 0x0000FD00)  !=
          0x0000F000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = {R: R(22),
//       Rd: Rd(15:12),
//       actual: Actual_MRS_cccc00010r001111dddd000000000000_case_1,
//       baseline: MRS_cccc00010r001111dddd000000000000_case_0,
//       constraints: ,
//       defs: {Rd},
//       fields: [R(22), Rd(15:12)],
//       pattern: cccc00010r001111dddd000000000000,
//       rule: MRS,
//       safety: [R(22)=1 => FORBIDDEN_OPERANDS,
//         Rd(15:12)=1111 => UNPREDICTABLE],
//       uses: {}}
class MRS_cccc00010r001111dddd000000000000_case_0TesterCase4
    : public Arm32DecoderTester {
 public:
  MRS_cccc00010r001111dddd000000000000_case_0TesterCase4(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool MRS_cccc00010r001111dddd000000000000_case_0TesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~000
  if ((inst.Bits() & 0x00000070)  !=
          0x00000000) return false;
  // B(9)=~0
  if ((inst.Bits() & 0x00000200)  !=
          0x00000000) return false;
  // op(22:21)=~x0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx1111xxxx00x0xxxx0000
  if ((inst.Bits() & 0x000F0D0F)  !=
          0x000F0000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=000 & B(9)=1 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r00mmmmdddd001m00000000,
//       rule: MRS_Banked_register}
class ForbiddenCondDecoderTesterCase5
    : public UnsafeCondDecoderTester {
 public:
  ForbiddenCondDecoderTesterCase5(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool ForbiddenCondDecoderTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~000
  if ((inst.Bits() & 0x00000070)  !=
          0x00000000) return false;
  // B(9)=~1
  if ((inst.Bits() & 0x00000200)  !=
          0x00000200) return false;
  // op(22:21)=~x0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
  if ((inst.Bits() & 0x00000C0F)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=000 & B(9)=1 & op(22:21)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm1111001m0000nnnn,
//       rule: MSR_Banked_register}
class ForbiddenCondDecoderTesterCase6
    : public UnsafeCondDecoderTester {
 public:
  ForbiddenCondDecoderTesterCase6(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool ForbiddenCondDecoderTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~000
  if ((inst.Bits() & 0x00000070)  !=
          0x00000000) return false;
  // B(9)=~1
  if ((inst.Bits() & 0x00000200)  !=
          0x00000200) return false;
  // op(22:21)=~x1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx111100xxxxxxxxxx
  if ((inst.Bits() & 0x0000FC00)  !=
          0x0000F000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=001 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       actual: Actual_Bx_cccc000100101111111111110001mmmm_case_1,
//       baseline: Bx_cccc000100101111111111110001mmmm_case_0,
//       constraints: ,
//       defs: {Pc},
//       fields: [Rm(3:0)],
//       pattern: cccc000100101111111111110001mmmm,
//       rule: Bx,
//       safety: [Rm(3:0)=1111 => FORBIDDEN_OPERANDS],
//       target: Rm,
//       uses: {Rm}}
class Bx_cccc000100101111111111110001mmmm_case_0TesterCase7
    : public Arm32DecoderTester {
 public:
  Bx_cccc000100101111111111110001mmmm_case_0TesterCase7(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool Bx_cccc000100101111111111110001mmmm_case_0TesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~001
  if ((inst.Bits() & 0x00000070)  !=
          0x00000010) return false;
  // op(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx111111111111xxxxxxxx
  if ((inst.Bits() & 0x000FFF00)  !=
          0x000FFF00) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=001 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       actual: Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1,
//       baseline: CLZ_cccc000101101111dddd11110001mmmm_case_0,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       pattern: cccc000101101111dddd11110001mmmm,
//       rule: CLZ,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//       uses: {Rm}}
class CLZ_cccc000101101111dddd11110001mmmm_case_0TesterCase8
    : public Arm32DecoderTester {
 public:
  CLZ_cccc000101101111dddd11110001mmmm_case_0TesterCase8(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool CLZ_cccc000101101111dddd11110001mmmm_case_0TesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~001
  if ((inst.Bits() & 0x00000070)  !=
          0x00000010) return false;
  // op(22:21)=~11
  if ((inst.Bits() & 0x00600000)  !=
          0x00600000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx1111xxxx1111xxxxxxxx
  if ((inst.Bits() & 0x000F0F00)  !=
          0x000F0F00) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=010 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc000100101111111111110010mmmm,
//       rule: BXJ}
class ForbiddenCondDecoderTesterCase9
    : public UnsafeCondDecoderTester {
 public:
  ForbiddenCondDecoderTesterCase9(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool ForbiddenCondDecoderTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~010
  if ((inst.Bits() & 0x00000070)  !=
          0x00000020) return false;
  // op(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx111111111111xxxxxxxx
  if ((inst.Bits() & 0x000FFF00)  !=
          0x000FFF00) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=011 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {Lr: 14,
//       Pc: 15,
//       Rm: Rm(3:0),
//       actual: Actual_BLX_register_cccc000100101111111111110011mmmm_case_1,
//       baseline: BLX_register_cccc000100101111111111110011mmmm_case_0,
//       constraints: ,
//       defs: {Pc, Lr},
//       fields: [Rm(3:0)],
//       pattern: cccc000100101111111111110011mmmm,
//       rule: BLX_register,
//       safety: [Rm(3:0)=1111 => FORBIDDEN_OPERANDS],
//       target: Rm,
//       uses: {Rm}}
class BLX_register_cccc000100101111111111110011mmmm_case_0TesterCase10
    : public Arm32DecoderTester {
 public:
  BLX_register_cccc000100101111111111110011mmmm_case_0TesterCase10(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BLX_register_cccc000100101111111111110011mmmm_case_0TesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~011
  if ((inst.Bits() & 0x00000070)  !=
          0x00000030) return false;
  // op(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx111111111111xxxxxxxx
  if ((inst.Bits() & 0x000FFF00)  !=
          0x000FFF00) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=110 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0001011000000000000001101110,
//       rule: ERET}
class ForbiddenCondDecoderTesterCase11
    : public UnsafeCondDecoderTester {
 public:
  ForbiddenCondDecoderTesterCase11(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool ForbiddenCondDecoderTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~110
  if ((inst.Bits() & 0x00000070)  !=
          0x00000060) return false;
  // op(22:21)=~11
  if ((inst.Bits() & 0x00600000)  !=
          0x00600000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx000000000000xxxx1110
  if ((inst.Bits() & 0x000FFF0F)  !=
          0x0000000E) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=111 & op(22:21)=01
//    = {actual: Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1,
//       baseline: BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       inst: inst,
//       is_literal_pool_head: inst  ==
//               LiteralPoolHeadConstant(),
//       pattern: cccc00010010iiiiiiiiiiii0111iiii,
//       rule: BKPT,
//       safety: [cond(31:28)=~1110 => UNPREDICTABLE,
//         not IsBreakPointAndConstantPoolHead(inst) => FORBIDDEN_OPERANDS],
//       uses: {}}
class BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0TesterCase12
    : public Arm32DecoderTester {
 public:
  BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0TesterCase12(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0TesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~111
  if ((inst.Bits() & 0x00000070)  !=
          0x00000070) return false;
  // op(22:21)=~01
  if ((inst.Bits() & 0x00600000)  !=
          0x00200000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Arm32DecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=111 & op(22:21)=10
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010100iiiiiiiiiiii0111iiii,
//       rule: HVC}
class ForbiddenCondDecoderTesterCase13
    : public UnsafeCondDecoderTester {
 public:
  ForbiddenCondDecoderTesterCase13(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool ForbiddenCondDecoderTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~111
  if ((inst.Bits() & 0x00000070)  !=
          0x00000070) return false;
  // op(22:21)=~10
  if ((inst.Bits() & 0x00600000)  !=
          0x00400000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op2(6:4)=111 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc000101100000000000000111iiii,
//       rule: SMC}
class ForbiddenCondDecoderTesterCase14
    : public UnsafeCondDecoderTester {
 public:
  ForbiddenCondDecoderTesterCase14(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool ForbiddenCondDecoderTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op2(6:4)=~111
  if ((inst.Bits() & 0x00000070)  !=
          0x00000070) return false;
  // op(22:21)=~11
  if ((inst.Bits() & 0x00600000)  !=
          0x00600000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx000000000000xxxxxxxx
  if ((inst.Bits() & 0x000FFF00)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rn: Rn(3:0),
//       actual: Actual_MSR_register_cccc00010010mm00111100000000nnnn_case_1,
//       baseline: MSR_register_cccc00010010mm00111100000000nnnn_case_0,
//       constraints: ,
//       defs: {NZCV
//            if write_nzcvq
//            else None},
//       fields: [mask(19:18), Rn(3:0)],
//       mask: mask(19:18),
//       pattern: cccc00010010mm00111100000000nnnn,
//       rule: MSR_register,
//       safety: [mask(19:18)=00 => UNPREDICTABLE,
//         Rn  ==
//               Pc => UNPREDICTABLE],
//       uses: {Rn},
//       write_nzcvq: mask(1)=1}
class MSR_register_cccc00010010mm00111100000000nnnn_case_0Tester_Case0
    : public MSR_register_cccc00010010mm00111100000000nnnn_case_0TesterCase0 {
 public:
  MSR_register_cccc00010010mm00111100000000nnnn_case_0Tester_Case0()
    : MSR_register_cccc00010010mm00111100000000nnnn_case_0TesterCase0(
      state_.MSR_register_cccc00010010mm00111100000000nnnn_case_0_MSR_register_instance_)
  {}
};

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm111100000000nnnn,
//       rule: MSR_register}
class ForbiddenCondDecoderTester_Case1
    : public ForbiddenCondDecoderTesterCase1 {
 public:
  ForbiddenCondDecoderTester_Case1()
    : ForbiddenCondDecoderTesterCase1(
      state_.ForbiddenCondDecoder_MSR_register_instance_)
  {}
};

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm111100000000nnnn,
//       rule: MSR_register}
class ForbiddenCondDecoderTester_Case2
    : public ForbiddenCondDecoderTesterCase2 {
 public:
  ForbiddenCondDecoderTester_Case2()
    : ForbiddenCondDecoderTesterCase2(
      state_.ForbiddenCondDecoder_MSR_register_instance_)
  {}
};

// op2(6:4)=000 & B(9)=0 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm111100000000nnnn,
//       rule: MSR_register}
class ForbiddenCondDecoderTester_Case3
    : public ForbiddenCondDecoderTesterCase3 {
 public:
  ForbiddenCondDecoderTester_Case3()
    : ForbiddenCondDecoderTesterCase3(
      state_.ForbiddenCondDecoder_MSR_register_instance_)
  {}
};

// op2(6:4)=000 & B(9)=0 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = {R: R(22),
//       Rd: Rd(15:12),
//       actual: Actual_MRS_cccc00010r001111dddd000000000000_case_1,
//       baseline: MRS_cccc00010r001111dddd000000000000_case_0,
//       constraints: ,
//       defs: {Rd},
//       fields: [R(22), Rd(15:12)],
//       pattern: cccc00010r001111dddd000000000000,
//       rule: MRS,
//       safety: [R(22)=1 => FORBIDDEN_OPERANDS,
//         Rd(15:12)=1111 => UNPREDICTABLE],
//       uses: {}}
class MRS_cccc00010r001111dddd000000000000_case_0Tester_Case4
    : public MRS_cccc00010r001111dddd000000000000_case_0TesterCase4 {
 public:
  MRS_cccc00010r001111dddd000000000000_case_0Tester_Case4()
    : MRS_cccc00010r001111dddd000000000000_case_0TesterCase4(
      state_.MRS_cccc00010r001111dddd000000000000_case_0_MRS_instance_)
  {}
};

// op2(6:4)=000 & B(9)=1 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r00mmmmdddd001m00000000,
//       rule: MRS_Banked_register}
class ForbiddenCondDecoderTester_Case5
    : public ForbiddenCondDecoderTesterCase5 {
 public:
  ForbiddenCondDecoderTester_Case5()
    : ForbiddenCondDecoderTesterCase5(
      state_.ForbiddenCondDecoder_MRS_Banked_register_instance_)
  {}
};

// op2(6:4)=000 & B(9)=1 & op(22:21)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm1111001m0000nnnn,
//       rule: MSR_Banked_register}
class ForbiddenCondDecoderTester_Case6
    : public ForbiddenCondDecoderTesterCase6 {
 public:
  ForbiddenCondDecoderTester_Case6()
    : ForbiddenCondDecoderTesterCase6(
      state_.ForbiddenCondDecoder_MSR_Banked_register_instance_)
  {}
};

// op2(6:4)=001 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       actual: Actual_Bx_cccc000100101111111111110001mmmm_case_1,
//       baseline: Bx_cccc000100101111111111110001mmmm_case_0,
//       constraints: ,
//       defs: {Pc},
//       fields: [Rm(3:0)],
//       pattern: cccc000100101111111111110001mmmm,
//       rule: Bx,
//       safety: [Rm(3:0)=1111 => FORBIDDEN_OPERANDS],
//       target: Rm,
//       uses: {Rm}}
class Bx_cccc000100101111111111110001mmmm_case_0Tester_Case7
    : public Bx_cccc000100101111111111110001mmmm_case_0TesterCase7 {
 public:
  Bx_cccc000100101111111111110001mmmm_case_0Tester_Case7()
    : Bx_cccc000100101111111111110001mmmm_case_0TesterCase7(
      state_.Bx_cccc000100101111111111110001mmmm_case_0_Bx_instance_)
  {}
};

// op2(6:4)=001 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       actual: Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1,
//       baseline: CLZ_cccc000101101111dddd11110001mmmm_case_0,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       pattern: cccc000101101111dddd11110001mmmm,
//       rule: CLZ,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//       uses: {Rm}}
class CLZ_cccc000101101111dddd11110001mmmm_case_0Tester_Case8
    : public CLZ_cccc000101101111dddd11110001mmmm_case_0TesterCase8 {
 public:
  CLZ_cccc000101101111dddd11110001mmmm_case_0Tester_Case8()
    : CLZ_cccc000101101111dddd11110001mmmm_case_0TesterCase8(
      state_.CLZ_cccc000101101111dddd11110001mmmm_case_0_CLZ_instance_)
  {}
};

// op2(6:4)=010 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc000100101111111111110010mmmm,
//       rule: BXJ}
class ForbiddenCondDecoderTester_Case9
    : public ForbiddenCondDecoderTesterCase9 {
 public:
  ForbiddenCondDecoderTester_Case9()
    : ForbiddenCondDecoderTesterCase9(
      state_.ForbiddenCondDecoder_BXJ_instance_)
  {}
};

// op2(6:4)=011 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {Lr: 14,
//       Pc: 15,
//       Rm: Rm(3:0),
//       actual: Actual_BLX_register_cccc000100101111111111110011mmmm_case_1,
//       baseline: BLX_register_cccc000100101111111111110011mmmm_case_0,
//       constraints: ,
//       defs: {Pc, Lr},
//       fields: [Rm(3:0)],
//       pattern: cccc000100101111111111110011mmmm,
//       rule: BLX_register,
//       safety: [Rm(3:0)=1111 => FORBIDDEN_OPERANDS],
//       target: Rm,
//       uses: {Rm}}
class BLX_register_cccc000100101111111111110011mmmm_case_0Tester_Case10
    : public BLX_register_cccc000100101111111111110011mmmm_case_0TesterCase10 {
 public:
  BLX_register_cccc000100101111111111110011mmmm_case_0Tester_Case10()
    : BLX_register_cccc000100101111111111110011mmmm_case_0TesterCase10(
      state_.BLX_register_cccc000100101111111111110011mmmm_case_0_BLX_register_instance_)
  {}
};

// op2(6:4)=110 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0001011000000000000001101110,
//       rule: ERET}
class ForbiddenCondDecoderTester_Case11
    : public ForbiddenCondDecoderTesterCase11 {
 public:
  ForbiddenCondDecoderTester_Case11()
    : ForbiddenCondDecoderTesterCase11(
      state_.ForbiddenCondDecoder_ERET_instance_)
  {}
};

// op2(6:4)=111 & op(22:21)=01
//    = {actual: Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1,
//       baseline: BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       inst: inst,
//       is_literal_pool_head: inst  ==
//               LiteralPoolHeadConstant(),
//       pattern: cccc00010010iiiiiiiiiiii0111iiii,
//       rule: BKPT,
//       safety: [cond(31:28)=~1110 => UNPREDICTABLE,
//         not IsBreakPointAndConstantPoolHead(inst) => FORBIDDEN_OPERANDS],
//       uses: {}}
class BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0Tester_Case12
    : public BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0TesterCase12 {
 public:
  BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0Tester_Case12()
    : BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0TesterCase12(
      state_.BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0_BKPT_instance_)
  {}
};

// op2(6:4)=111 & op(22:21)=10
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010100iiiiiiiiiiii0111iiii,
//       rule: HVC}
class ForbiddenCondDecoderTester_Case13
    : public ForbiddenCondDecoderTesterCase13 {
 public:
  ForbiddenCondDecoderTester_Case13()
    : ForbiddenCondDecoderTesterCase13(
      state_.ForbiddenCondDecoder_HVC_instance_)
  {}
};

// op2(6:4)=111 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc000101100000000000000111iiii,
//       rule: SMC}
class ForbiddenCondDecoderTester_Case14
    : public ForbiddenCondDecoderTesterCase14 {
 public:
  ForbiddenCondDecoderTester_Case14()
    : ForbiddenCondDecoderTesterCase14(
      state_.ForbiddenCondDecoder_SMC_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx00 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {NZCV: 16,
//       None: 32,
//       Pc: 15,
//       Rn: Rn(3:0),
//       actual: Actual_MSR_register_cccc00010010mm00111100000000nnnn_case_1,
//       baseline: MSR_register_cccc00010010mm00111100000000nnnn_case_0,
//       constraints: ,
//       defs: {NZCV
//            if write_nzcvq
//            else None},
//       fields: [mask(19:18), Rn(3:0)],
//       mask: mask(19:18),
//       pattern: cccc00010010mm00111100000000nnnn,
//       rule: MSR_register,
//       safety: [mask(19:18)=00 => UNPREDICTABLE,
//         Rn  ==
//               Pc => UNPREDICTABLE],
//       uses: {Rn},
//       write_nzcvq: mask(1)=1}
TEST_F(Arm32DecoderStateTests,
       MSR_register_cccc00010010mm00111100000000nnnn_case_0Tester_Case0_TestCase0) {
  MSR_register_cccc00010010mm00111100000000nnnn_case_0Tester_Case0 baseline_tester;
  NamedActual_MSR_register_cccc00010010mm00111100000000nnnn_case_1_MSR_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010mm00111100000000nnnn");
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx01 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm111100000000nnnn,
//       rule: MSR_register}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case1_TestCase1) {
  ForbiddenCondDecoderTester_Case1 baseline_tester;
  NamedForbidden_MSR_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r10mmmm111100000000nnnn");
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=01 & op1(19:16)=xx1x & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm111100000000nnnn,
//       rule: MSR_register}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case2_TestCase2) {
  ForbiddenCondDecoderTester_Case2 baseline_tester;
  NamedForbidden_MSR_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r10mmmm111100000000nnnn");
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100x0xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm111100000000nnnn,
//       rule: MSR_register}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case3_TestCase3) {
  ForbiddenCondDecoderTester_Case3 baseline_tester;
  NamedForbidden_MSR_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r10mmmm111100000000nnnn");
}

// op2(6:4)=000 & B(9)=0 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx00x0xxxx0000
//    = {R: R(22),
//       Rd: Rd(15:12),
//       actual: Actual_MRS_cccc00010r001111dddd000000000000_case_1,
//       baseline: MRS_cccc00010r001111dddd000000000000_case_0,
//       constraints: ,
//       defs: {Rd},
//       fields: [R(22), Rd(15:12)],
//       pattern: cccc00010r001111dddd000000000000,
//       rule: MRS,
//       safety: [R(22)=1 => FORBIDDEN_OPERANDS,
//         Rd(15:12)=1111 => UNPREDICTABLE],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       MRS_cccc00010r001111dddd000000000000_case_0Tester_Case4_TestCase4) {
  MRS_cccc00010r001111dddd000000000000_case_0Tester_Case4 baseline_tester;
  NamedActual_MRS_cccc00010r001111dddd000000000000_case_1_MRS actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r001111dddd000000000000");
}

// op2(6:4)=000 & B(9)=1 & op(22:21)=x0 & $pattern(31:0)=xxxxxxxxxxxxxxxxxxxx00xxxxxx0000
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r00mmmmdddd001m00000000,
//       rule: MRS_Banked_register}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case5_TestCase5) {
  ForbiddenCondDecoderTester_Case5 baseline_tester;
  NamedForbidden_MRS_Banked_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r00mmmmdddd001m00000000");
}

// op2(6:4)=000 & B(9)=1 & op(22:21)=x1 & $pattern(31:0)=xxxxxxxxxxxxxxxx111100xxxxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010r10mmmm1111001m0000nnnn,
//       rule: MSR_Banked_register}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case6_TestCase6) {
  ForbiddenCondDecoderTester_Case6 baseline_tester;
  NamedForbidden_MSR_Banked_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010r10mmmm1111001m0000nnnn");
}

// op2(6:4)=001 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       actual: Actual_Bx_cccc000100101111111111110001mmmm_case_1,
//       baseline: Bx_cccc000100101111111111110001mmmm_case_0,
//       constraints: ,
//       defs: {Pc},
//       fields: [Rm(3:0)],
//       pattern: cccc000100101111111111110001mmmm,
//       rule: Bx,
//       safety: [Rm(3:0)=1111 => FORBIDDEN_OPERANDS],
//       target: Rm,
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       Bx_cccc000100101111111111110001mmmm_case_0Tester_Case7_TestCase7) {
  Bx_cccc000100101111111111110001mmmm_case_0Tester_Case7 baseline_tester;
  NamedActual_Bx_cccc000100101111111111110001mmmm_case_1_Bx actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110001mmmm");
}

// op2(6:4)=001 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx1111xxxx1111xxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(15:12),
//       Rm: Rm(3:0),
//       actual: Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1,
//       baseline: CLZ_cccc000101101111dddd11110001mmmm_case_0,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(15:12), Rm(3:0)],
//       pattern: cccc000101101111dddd11110001mmmm,
//       rule: CLZ,
//       safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       CLZ_cccc000101101111dddd11110001mmmm_case_0Tester_Case8_TestCase8) {
  CLZ_cccc000101101111dddd11110001mmmm_case_0Tester_Case8 baseline_tester;
  NamedActual_CLZ_cccc000101101111dddd11110001mmmm_case_1_CLZ actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000101101111dddd11110001mmmm");
}

// op2(6:4)=010 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc000100101111111111110010mmmm,
//       rule: BXJ}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case9_TestCase9) {
  ForbiddenCondDecoderTester_Case9 baseline_tester;
  NamedForbidden_BXJ actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110010mmmm");
}

// op2(6:4)=011 & op(22:21)=01 & $pattern(31:0)=xxxxxxxxxxxx111111111111xxxxxxxx
//    = {Lr: 14,
//       Pc: 15,
//       Rm: Rm(3:0),
//       actual: Actual_BLX_register_cccc000100101111111111110011mmmm_case_1,
//       baseline: BLX_register_cccc000100101111111111110011mmmm_case_0,
//       constraints: ,
//       defs: {Pc, Lr},
//       fields: [Rm(3:0)],
//       pattern: cccc000100101111111111110011mmmm,
//       rule: BLX_register,
//       safety: [Rm(3:0)=1111 => FORBIDDEN_OPERANDS],
//       target: Rm,
//       uses: {Rm}}
TEST_F(Arm32DecoderStateTests,
       BLX_register_cccc000100101111111111110011mmmm_case_0Tester_Case10_TestCase10) {
  BLX_register_cccc000100101111111111110011mmmm_case_0Tester_Case10 baseline_tester;
  NamedActual_BLX_register_cccc000100101111111111110011mmmm_case_1_BLX_register actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000100101111111111110011mmmm");
}

// op2(6:4)=110 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxx1110
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc0001011000000000000001101110,
//       rule: ERET}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case11_TestCase11) {
  ForbiddenCondDecoderTester_Case11 baseline_tester;
  NamedForbidden_ERET actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc0001011000000000000001101110");
}

// op2(6:4)=111 & op(22:21)=01
//    = {actual: Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1,
//       baseline: BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0,
//       cond: cond(31:28),
//       constraints: ,
//       defs: {},
//       fields: [cond(31:28)],
//       inst: inst,
//       is_literal_pool_head: inst  ==
//               LiteralPoolHeadConstant(),
//       pattern: cccc00010010iiiiiiiiiiii0111iiii,
//       rule: BKPT,
//       safety: [cond(31:28)=~1110 => UNPREDICTABLE,
//         not IsBreakPointAndConstantPoolHead(inst) => FORBIDDEN_OPERANDS],
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0Tester_Case12_TestCase12) {
  BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_0Tester_Case12 baseline_tester;
  NamedActual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1_BKPT actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010010iiiiiiiiiiii0111iiii");
}

// op2(6:4)=111 & op(22:21)=10
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc00010100iiiiiiiiiiii0111iiii,
//       rule: HVC}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case13_TestCase13) {
  ForbiddenCondDecoderTester_Case13 baseline_tester;
  NamedForbidden_HVC actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc00010100iiiiiiiiiiii0111iiii");
}

// op2(6:4)=111 & op(22:21)=11 & $pattern(31:0)=xxxxxxxxxxxx000000000000xxxxxxxx
//    = {actual: Forbidden,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc000101100000000000000111iiii,
//       rule: SMC}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case14_TestCase14) {
  ForbiddenCondDecoderTester_Case14 baseline_tester;
  NamedForbidden_SMC actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc000101100000000000000111iiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
