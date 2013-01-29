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
#include "native_client/src/trusted/validator_arm/actual_classes.h"
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


// op1(26:20)=0010000 & op2(7:4)=0000 & Rn(19:16)=xxx1 & $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: SETEND_1111000100000001000000i000000000_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase0
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase0(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~0010000
  if ((inst.Bits() & 0x07F00000)  !=
          0x01000000) return false;
  // op2(7:4)=~0000
  if ((inst.Bits() & 0x000000F0)  !=
          0x00000000) return false;
  // Rn(19:16)=~xxx1
  if ((inst.Bits() & 0x00010000)  !=
          0x00010000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx000x000000x0xxxx0000
  if ((inst.Bits() & 0x000EFD0F)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=0010000 & op2(7:4)=xx0x & Rn(19:16)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: CPS_111100010000iii00000000iii0iiiii_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase1
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase1(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~0010000
  if ((inst.Bits() & 0x07F00000)  !=
          0x01000000) return false;
  // op2(7:4)=~xx0x
  if ((inst.Bits() & 0x00000020)  !=
          0x00000000) return false;
  // Rn(19:16)=~xxx0
  if ((inst.Bits() & 0x00010000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx0000000xxxxxxxxx
  if ((inst.Bits() & 0x0000FE00)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=1010011
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101010011xxxxxxxxxxxxxxxxxxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase2
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase2(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~1010011
  if ((inst.Bits() & 0x07F00000)  !=
          0x05300000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => UNPREDICTABLE
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=1010111 & op2(7:4)=0000
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101010111xxxxxxxxxxxx0000xxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase3
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase3(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~1010111
  if ((inst.Bits() & 0x07F00000)  !=
          0x05700000) return false;
  // op2(7:4)=~0000
  if ((inst.Bits() & 0x000000F0)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => UNPREDICTABLE
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=1010111 & op2(7:4)=0001 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: CLREX_11110101011111111111000000011111_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase4
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase4(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~1010111
  if ((inst.Bits() & 0x07F00000)  !=
          0x05700000) return false;
  // op2(7:4)=~0001
  if ((inst.Bits() & 0x000000F0)  !=
          0x00000010) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx111111110000xxxx1111
  if ((inst.Bits() & 0x000FFF0F)  !=
          0x000FF00F) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=1010111 & op2(7:4)=0100 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {actual: DataBarrier,
//       baseline: DataBarrier,
//       constraints: ,
//       fields: [option(3:0)],
//       generated_baseline: DSB_1111010101111111111100000100xxxx_case_0,
//       option: option(3:0),
//       safety: [not option in {'1111'(3:0), '1110'(3:0), '1011'(3:0), '1010'(3:0), '0111'(3:0), '0110'(3:0), '0011'(3:0), '0010'(3:0)} => FORBIDDEN_OPERANDS]}
class DataBarrierTesterCase5
    : public DataBarrierTester {
 public:
  DataBarrierTesterCase5(const NamedClassDecoder& decoder)
    : DataBarrierTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool DataBarrierTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~1010111
  if ((inst.Bits() & 0x07F00000)  !=
          0x05700000) return false;
  // op2(7:4)=~0100
  if ((inst.Bits() & 0x000000F0)  !=
          0x00000040) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx111111110000xxxxxxxx
  if ((inst.Bits() & 0x000FFF00)  !=
          0x000FF000) return false;

  // Check other preconditions defined for the base decoder.
  return DataBarrierTester::
      PassesParsePreconditions(inst, decoder);
}

bool DataBarrierTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(DataBarrierTester::
               ApplySanityChecks(inst, decoder));

  // safety: not option in {'1111'(3:0), '1110'(3:0), '1011'(3:0), '1010'(3:0), '0111'(3:0), '0110'(3:0), '0011'(3:0), '0010'(3:0)} => FORBIDDEN_OPERANDS
  EXPECT_TRUE(((((inst.Bits() & 0x0000000F)) == ((15 & 0x0000000F)))) ||
       ((((inst.Bits() & 0x0000000F)) == ((14 & 0x0000000F)))) ||
       ((((inst.Bits() & 0x0000000F)) == ((11 & 0x0000000F)))) ||
       ((((inst.Bits() & 0x0000000F)) == ((10 & 0x0000000F)))) ||
       ((((inst.Bits() & 0x0000000F)) == ((7 & 0x0000000F)))) ||
       ((((inst.Bits() & 0x0000000F)) == ((6 & 0x0000000F)))) ||
       ((((inst.Bits() & 0x0000000F)) == ((3 & 0x0000000F)))) ||
       ((((inst.Bits() & 0x0000000F)) == ((2 & 0x0000000F)))));

  return true;
}

// op1(26:20)=1010111 & op2(7:4)=0101 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {actual: DataBarrier,
//       baseline: DataBarrier,
//       constraints: ,
//       fields: [option(3:0)],
//       generated_baseline: DMB_1111010101111111111100000101xxxx_case_0,
//       option: option(3:0),
//       safety: [not option in {'1111'(3:0), '1110'(3:0), '1011'(3:0), '1010'(3:0), '0111'(3:0), '0110'(3:0), '0011'(3:0), '0010'(3:0)} => FORBIDDEN_OPERANDS]}
class DataBarrierTesterCase6
    : public DataBarrierTester {
 public:
  DataBarrierTesterCase6(const NamedClassDecoder& decoder)
    : DataBarrierTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool DataBarrierTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~1010111
  if ((inst.Bits() & 0x07F00000)  !=
          0x05700000) return false;
  // op2(7:4)=~0101
  if ((inst.Bits() & 0x000000F0)  !=
          0x00000050) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx111111110000xxxxxxxx
  if ((inst.Bits() & 0x000FFF00)  !=
          0x000FF000) return false;

  // Check other preconditions defined for the base decoder.
  return DataBarrierTester::
      PassesParsePreconditions(inst, decoder);
}

bool DataBarrierTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(DataBarrierTester::
               ApplySanityChecks(inst, decoder));

  // safety: not option in {'1111'(3:0), '1110'(3:0), '1011'(3:0), '1010'(3:0), '0111'(3:0), '0110'(3:0), '0011'(3:0), '0010'(3:0)} => FORBIDDEN_OPERANDS
  EXPECT_TRUE(((((inst.Bits() & 0x0000000F)) == ((15 & 0x0000000F)))) ||
       ((((inst.Bits() & 0x0000000F)) == ((14 & 0x0000000F)))) ||
       ((((inst.Bits() & 0x0000000F)) == ((11 & 0x0000000F)))) ||
       ((((inst.Bits() & 0x0000000F)) == ((10 & 0x0000000F)))) ||
       ((((inst.Bits() & 0x0000000F)) == ((7 & 0x0000000F)))) ||
       ((((inst.Bits() & 0x0000000F)) == ((6 & 0x0000000F)))) ||
       ((((inst.Bits() & 0x0000000F)) == ((3 & 0x0000000F)))) ||
       ((((inst.Bits() & 0x0000000F)) == ((2 & 0x0000000F)))));

  return true;
}

// op1(26:20)=1010111 & op2(7:4)=0110 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {actual: InstructionBarrier,
//       baseline: InstructionBarrier,
//       constraints: ,
//       fields: [option(3:0)],
//       generated_baseline: ISB_1111010101111111111100000110xxxx_case_0,
//       option: option(3:0),
//       safety: [option(3:0)=~1111 => FORBIDDEN_OPERANDS]}
class InstructionBarrierTesterCase7
    : public InstructionBarrierTester {
 public:
  InstructionBarrierTesterCase7(const NamedClassDecoder& decoder)
    : InstructionBarrierTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool InstructionBarrierTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~1010111
  if ((inst.Bits() & 0x07F00000)  !=
          0x05700000) return false;
  // op2(7:4)=~0110
  if ((inst.Bits() & 0x000000F0)  !=
          0x00000060) return false;
  // $pattern(31:0)=~xxxxxxxxxxxx111111110000xxxxxxxx
  if ((inst.Bits() & 0x000FFF00)  !=
          0x000FF000) return false;

  // Check other preconditions defined for the base decoder.
  return InstructionBarrierTester::
      PassesParsePreconditions(inst, decoder);
}

bool InstructionBarrierTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(InstructionBarrierTester::
               ApplySanityChecks(inst, decoder));

  // safety: option(3:0)=~1111 => FORBIDDEN_OPERANDS
  EXPECT_TRUE((inst.Bits() & 0x0000000F)  ==
          0x0000000F);

  return true;
}

// op1(26:20)=1010111 & op2(7:4)=0111
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101010111xxxxxxxxxxxx0111xxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase8
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase8(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~1010111
  if ((inst.Bits() & 0x07F00000)  !=
          0x05700000) return false;
  // op2(7:4)=~0111
  if ((inst.Bits() & 0x000000F0)  !=
          0x00000070) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => UNPREDICTABLE
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=1010111 & op2(7:4)=001x
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101010111xxxxxxxxxxxx001xxxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase9
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase9(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~1010111
  if ((inst.Bits() & 0x07F00000)  !=
          0x05700000) return false;
  // op2(7:4)=~001x
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000020) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => UNPREDICTABLE
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=1010111 & op2(7:4)=1xxx
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101010111xxxxxxxxxxxx1xxxxxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase10
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase10(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~1010111
  if ((inst.Bits() & 0x07F00000)  !=
          0x05700000) return false;
  // op2(7:4)=~1xxx
  if ((inst.Bits() & 0x00000080)  !=
          0x00000080) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => UNPREDICTABLE
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=100x001
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_11110100x001xxxxxxxxxxxxxxxxxxxx_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase11
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase11(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~100x001
  if ((inst.Bits() & 0x07700000)  !=
          0x04100000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=100x101 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       Rn: Rn(19:16),
//       actual: PreloadRegisterImm12Op,
//       base: Rn,
//       baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16)],
//       generated_baseline: PLI_immediate_literal_11110100u101nnnn1111iiiiiiiiiiii_case_0,
//       is_literal_load: Rn  ==
//               Pc,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {Rn}}
class PreloadRegisterImm12OpTesterCase12
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterCase12(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~100x101
  if ((inst.Bits() & 0x07700000)  !=
          0x04500000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool PreloadRegisterImm12OpTesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(PreloadRegisterImm12OpTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => MAY_BE_SAFE
  EXPECT_TRUE(true);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=101x001 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Rn: Rn(19:16),
//       actual: PreloadRegisterImm12Op,
//       base: Rn,
//       baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16)],
//       generated_baseline: PLD_PLDW_immediate_11110101ur01nnnn1111iiiiiiiiiiii_case_0,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR],
//       uses: {Rn}}
class PreloadRegisterImm12OpTesterCase13
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterCase13(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~101x001
  if ((inst.Bits() & 0x07700000)  !=
          0x05100000) return false;
  // Rn(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool PreloadRegisterImm12OpTesterCase13
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(PreloadRegisterImm12OpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn(19:16)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000F0000)  !=
          0x000F0000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=101x001 & Rn(19:16)=1111
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_11110101x001xxxxxxxxxxxxxxxxxxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase14
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase14(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~101x001
  if ((inst.Bits() & 0x07700000)  !=
          0x05100000) return false;
  // Rn(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase14
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => UNPREDICTABLE
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=101x101 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Rn: Rn(19:16),
//       actual: PreloadRegisterImm12Op,
//       base: Rn,
//       baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16)],
//       generated_baseline: PLD_PLDW_immediate_11110101ur01nnnn1111iiiiiiiiiiii_case_1,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR],
//       uses: {Rn}}
class PreloadRegisterImm12OpTesterCase15
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterCase15(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~101x101
  if ((inst.Bits() & 0x07700000)  !=
          0x05500000) return false;
  // Rn(19:16)=1111
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool PreloadRegisterImm12OpTesterCase15
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(PreloadRegisterImm12OpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn(19:16)=1111 => DECODER_ERROR
  EXPECT_TRUE((inst.Bits() & 0x000F0000)  !=
          0x000F0000);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=101x101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       actual: PreloadRegisterImm12Op,
//       base: Pc,
//       baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       defs: {},
//       generated_baseline: PLD_literal_11110101u10111111111iiiiiiiiiiii_case_0,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {Pc}}
class PreloadRegisterImm12OpTesterCase16
    : public PreloadRegisterImm12OpTester {
 public:
  PreloadRegisterImm12OpTesterCase16(const NamedClassDecoder& decoder)
    : PreloadRegisterImm12OpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool PreloadRegisterImm12OpTesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~101x101
  if ((inst.Bits() & 0x07700000)  !=
          0x05500000) return false;
  // Rn(19:16)=~1111
  if ((inst.Bits() & 0x000F0000)  !=
          0x000F0000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterImm12OpTester::
      PassesParsePreconditions(inst, decoder);
}

bool PreloadRegisterImm12OpTesterCase16
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(PreloadRegisterImm12OpTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => MAY_BE_SAFE
  EXPECT_TRUE(true);

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=110x001 & op2(7:4)=xxx0
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_11110110x001xxxxxxxxxxxxxxx0xxxx_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase17
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase17(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~110x001
  if ((inst.Bits() & 0x07700000)  !=
          0x06100000) return false;
  // op2(7:4)=~xxx0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase17
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => FORBIDDEN
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=110x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: PreloadRegisterPairOp,
//       base: Rn,
//       baseline: PreloadRegisterPairOp,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16), Rm(3:0)],
//       generated_baseline: PLI_register_11110110u101nnnn1111iiiiitt0mmmm_case_0,
//       safety: [Rm  ==
//               Pc => UNPREDICTABLE,
//         true => FORBIDDEN_OPERANDS],
//       true: true,
//       uses: {Rm, Rn}}
class PreloadRegisterPairOpTesterCase18
    : public PreloadRegisterPairOpTester {
 public:
  PreloadRegisterPairOpTesterCase18(const NamedClassDecoder& decoder)
    : PreloadRegisterPairOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool PreloadRegisterPairOpTesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~110x101
  if ((inst.Bits() & 0x07700000)  !=
          0x06500000) return false;
  // op2(7:4)=~xxx0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterPairOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool PreloadRegisterPairOpTesterCase18
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(PreloadRegisterPairOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rm  ==
  //          Pc => UNPREDICTABLE
  EXPECT_TRUE((((inst.Bits() & 0x0000000F)) != (15)));

  // safety: true => FORBIDDEN_OPERANDS
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=111x001 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       R: R(22),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: PreloadRegisterPairOp,
//       base: Rn,
//       baseline: PreloadRegisterPairOp,
//       constraints: ,
//       defs: {},
//       fields: [R(22), Rn(19:16), Rm(3:0)],
//       generated_baseline: PLD_PLDW_register_11110111u001nnnn1111iiiiitt0mmmm_case_0,
//       is_pldw: R(22)=1,
//       safety: [Rm  ==
//               Pc ||
//            (Rn  ==
//               Pc &&
//            is_pldw) => UNPREDICTABLE,
//         true => FORBIDDEN_OPERANDS],
//       true: true,
//       uses: {Rm, Rn}}
class PreloadRegisterPairOpTesterCase19
    : public PreloadRegisterPairOpTester {
 public:
  PreloadRegisterPairOpTesterCase19(const NamedClassDecoder& decoder)
    : PreloadRegisterPairOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool PreloadRegisterPairOpTesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~111x001
  if ((inst.Bits() & 0x07700000)  !=
          0x07100000) return false;
  // op2(7:4)=~xxx0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterPairOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool PreloadRegisterPairOpTesterCase19
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(PreloadRegisterPairOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rm  ==
  //          Pc ||
  //       (Rn  ==
  //          Pc &&
  //       is_pldw) => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x0000000F)) == (15))) ||
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00400000)  ==
          0x00400000)))));

  // safety: true => FORBIDDEN_OPERANDS
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=111x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       R: R(22),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: PreloadRegisterPairOp,
//       base: Rn,
//       baseline: PreloadRegisterPairOp,
//       constraints: ,
//       defs: {},
//       fields: [R(22), Rn(19:16), Rm(3:0)],
//       generated_baseline: PLD_PLDW_register_11110111u101nnnn1111iiiiitt0mmmm_case_0,
//       is_pldw: R(22)=1,
//       safety: [Rm  ==
//               Pc ||
//            (Rn  ==
//               Pc &&
//            is_pldw) => UNPREDICTABLE,
//         true => FORBIDDEN_OPERANDS],
//       true: true,
//       uses: {Rm, Rn}}
class PreloadRegisterPairOpTesterCase20
    : public PreloadRegisterPairOpTester {
 public:
  PreloadRegisterPairOpTesterCase20(const NamedClassDecoder& decoder)
    : PreloadRegisterPairOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool PreloadRegisterPairOpTesterCase20
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~111x101
  if ((inst.Bits() & 0x07700000)  !=
          0x07500000) return false;
  // op2(7:4)=~xxx0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // Check other preconditions defined for the base decoder.
  return PreloadRegisterPairOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool PreloadRegisterPairOpTesterCase20
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(PreloadRegisterPairOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rm  ==
  //          Pc ||
  //       (Rn  ==
  //          Pc &&
  //       is_pldw) => UNPREDICTABLE
  EXPECT_TRUE(!(((((inst.Bits() & 0x0000000F)) == (15))) ||
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00400000)  ==
          0x00400000)))));

  // safety: true => FORBIDDEN_OPERANDS
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=1011x11
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101011x11xxxxxxxxxxxxxxxxxxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase21
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase21(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase21
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~1011x11
  if ((inst.Bits() & 0x07B00000)  !=
          0x05B00000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase21
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => UNPREDICTABLE
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=100xx11
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_11110100xx11xxxxxxxxxxxxxxxxxxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase22
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase22(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase22
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~100xx11
  if ((inst.Bits() & 0x07300000)  !=
          0x04300000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase22
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => UNPREDICTABLE
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// op1(26:20)=11xxx11 & op2(7:4)=xxx0
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_1111011xxx11xxxxxxxxxxxxxxx0xxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnsafeCondDecoderTesterCase23
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase23(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase23
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(26:20)=~11xxx11
  if ((inst.Bits() & 0x06300000)  !=
          0x06300000) return false;
  // op2(7:4)=~xxx0
  if ((inst.Bits() & 0x00000010)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

bool UnsafeCondDecoderTesterCase23
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(UnsafeCondDecoderTester::
               ApplySanityChecks(inst, decoder));

  // safety: true => UNPREDICTABLE
  EXPECT_TRUE(!(true));

  // defs: {};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList()));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// op1(26:20)=0010000 & op2(7:4)=0000 & Rn(19:16)=xxx1 & $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: SETEND_1111000100000001000000i000000000_case_0,
//       rule: SETEND,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case0
    : public UnsafeCondDecoderTesterCase0 {
 public:
  ForbiddenTester_Case0()
    : UnsafeCondDecoderTesterCase0(
      state_.Forbidden_SETEND_instance_)
  {}
};

// op1(26:20)=0010000 & op2(7:4)=xx0x & Rn(19:16)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: CPS_111100010000iii00000000iii0iiiii_case_0,
//       rule: CPS,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case1
    : public UnsafeCondDecoderTesterCase1 {
 public:
  ForbiddenTester_Case1()
    : UnsafeCondDecoderTesterCase1(
      state_.Forbidden_CPS_instance_)
  {}
};

// op1(26:20)=1010011
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101010011xxxxxxxxxxxxxxxxxxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnpredictableTester_Case2
    : public UnsafeCondDecoderTesterCase2 {
 public:
  UnpredictableTester_Case2()
    : UnsafeCondDecoderTesterCase2(
      state_.Unpredictable_None_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=0000
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101010111xxxxxxxxxxxx0000xxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnpredictableTester_Case3
    : public UnsafeCondDecoderTesterCase3 {
 public:
  UnpredictableTester_Case3()
    : UnsafeCondDecoderTesterCase3(
      state_.Unpredictable_None_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=0001 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: CLREX_11110101011111111111000000011111_case_0,
//       rule: CLREX,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case4
    : public UnsafeCondDecoderTesterCase4 {
 public:
  ForbiddenTester_Case4()
    : UnsafeCondDecoderTesterCase4(
      state_.Forbidden_CLREX_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=0100 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {actual: DataBarrier,
//       baseline: DataBarrier,
//       constraints: ,
//       fields: [option(3:0)],
//       generated_baseline: DSB_1111010101111111111100000100xxxx_case_0,
//       option: option(3:0),
//       rule: DSB,
//       safety: [not option in {'1111'(3:0), '1110'(3:0), '1011'(3:0), '1010'(3:0), '0111'(3:0), '0110'(3:0), '0011'(3:0), '0010'(3:0)} => FORBIDDEN_OPERANDS]}
class DataBarrierTester_Case5
    : public DataBarrierTesterCase5 {
 public:
  DataBarrierTester_Case5()
    : DataBarrierTesterCase5(
      state_.DataBarrier_DSB_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=0101 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {actual: DataBarrier,
//       baseline: DataBarrier,
//       constraints: ,
//       fields: [option(3:0)],
//       generated_baseline: DMB_1111010101111111111100000101xxxx_case_0,
//       option: option(3:0),
//       rule: DMB,
//       safety: [not option in {'1111'(3:0), '1110'(3:0), '1011'(3:0), '1010'(3:0), '0111'(3:0), '0110'(3:0), '0011'(3:0), '0010'(3:0)} => FORBIDDEN_OPERANDS]}
class DataBarrierTester_Case6
    : public DataBarrierTesterCase6 {
 public:
  DataBarrierTester_Case6()
    : DataBarrierTesterCase6(
      state_.DataBarrier_DMB_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=0110 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {actual: InstructionBarrier,
//       baseline: InstructionBarrier,
//       constraints: ,
//       fields: [option(3:0)],
//       generated_baseline: ISB_1111010101111111111100000110xxxx_case_0,
//       option: option(3:0),
//       rule: ISB,
//       safety: [option(3:0)=~1111 => FORBIDDEN_OPERANDS]}
class InstructionBarrierTester_Case7
    : public InstructionBarrierTesterCase7 {
 public:
  InstructionBarrierTester_Case7()
    : InstructionBarrierTesterCase7(
      state_.InstructionBarrier_ISB_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=0111
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101010111xxxxxxxxxxxx0111xxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnpredictableTester_Case8
    : public UnsafeCondDecoderTesterCase8 {
 public:
  UnpredictableTester_Case8()
    : UnsafeCondDecoderTesterCase8(
      state_.Unpredictable_None_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=001x
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101010111xxxxxxxxxxxx001xxxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnpredictableTester_Case9
    : public UnsafeCondDecoderTesterCase9 {
 public:
  UnpredictableTester_Case9()
    : UnsafeCondDecoderTesterCase9(
      state_.Unpredictable_None_instance_)
  {}
};

// op1(26:20)=1010111 & op2(7:4)=1xxx
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101010111xxxxxxxxxxxx1xxxxxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnpredictableTester_Case10
    : public UnsafeCondDecoderTesterCase10 {
 public:
  UnpredictableTester_Case10()
    : UnsafeCondDecoderTesterCase10(
      state_.Unpredictable_None_instance_)
  {}
};

// op1(26:20)=100x001
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_11110100x001xxxxxxxxxxxxxxxxxxxx_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case11
    : public UnsafeCondDecoderTesterCase11 {
 public:
  ForbiddenTester_Case11()
    : UnsafeCondDecoderTesterCase11(
      state_.Forbidden_None_instance_)
  {}
};

// op1(26:20)=100x101 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       Rn: Rn(19:16),
//       actual: PreloadRegisterImm12Op,
//       base: Rn,
//       baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16)],
//       generated_baseline: PLI_immediate_literal_11110100u101nnnn1111iiiiiiiiiiii_case_0,
//       is_literal_load: Rn  ==
//               Pc,
//       rule: PLI_immediate_literal,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {Rn}}
class PreloadRegisterImm12OpTester_Case12
    : public PreloadRegisterImm12OpTesterCase12 {
 public:
  PreloadRegisterImm12OpTester_Case12()
    : PreloadRegisterImm12OpTesterCase12(
      state_.PreloadRegisterImm12Op_PLI_immediate_literal_instance_)
  {}
};

// op1(26:20)=101x001 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Rn: Rn(19:16),
//       actual: PreloadRegisterImm12Op,
//       base: Rn,
//       baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16)],
//       generated_baseline: PLD_PLDW_immediate_11110101ur01nnnn1111iiiiiiiiiiii_case_0,
//       rule: PLD_PLDW_immediate,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR],
//       uses: {Rn}}
class PreloadRegisterImm12OpTester_Case13
    : public PreloadRegisterImm12OpTesterCase13 {
 public:
  PreloadRegisterImm12OpTester_Case13()
    : PreloadRegisterImm12OpTesterCase13(
      state_.PreloadRegisterImm12Op_PLD_PLDW_immediate_instance_)
  {}
};

// op1(26:20)=101x001 & Rn(19:16)=1111
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_11110101x001xxxxxxxxxxxxxxxxxxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnpredictableTester_Case14
    : public UnsafeCondDecoderTesterCase14 {
 public:
  UnpredictableTester_Case14()
    : UnsafeCondDecoderTesterCase14(
      state_.Unpredictable_None_instance_)
  {}
};

// op1(26:20)=101x101 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Rn: Rn(19:16),
//       actual: PreloadRegisterImm12Op,
//       base: Rn,
//       baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16)],
//       generated_baseline: PLD_PLDW_immediate_11110101ur01nnnn1111iiiiiiiiiiii_case_1,
//       rule: PLD_PLDW_immediate,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR],
//       uses: {Rn}}
class PreloadRegisterImm12OpTester_Case15
    : public PreloadRegisterImm12OpTesterCase15 {
 public:
  PreloadRegisterImm12OpTester_Case15()
    : PreloadRegisterImm12OpTesterCase15(
      state_.PreloadRegisterImm12Op_PLD_PLDW_immediate_instance_)
  {}
};

// op1(26:20)=101x101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       actual: PreloadRegisterImm12Op,
//       base: Pc,
//       baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       defs: {},
//       generated_baseline: PLD_literal_11110101u10111111111iiiiiiiiiiii_case_0,
//       rule: PLD_literal,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {Pc}}
class PreloadRegisterImm12OpTester_Case16
    : public PreloadRegisterImm12OpTesterCase16 {
 public:
  PreloadRegisterImm12OpTester_Case16()
    : PreloadRegisterImm12OpTesterCase16(
      state_.PreloadRegisterImm12Op_PLD_literal_instance_)
  {}
};

// op1(26:20)=110x001 & op2(7:4)=xxx0
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_11110110x001xxxxxxxxxxxxxxx0xxxx_case_0,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
class ForbiddenTester_Case17
    : public UnsafeCondDecoderTesterCase17 {
 public:
  ForbiddenTester_Case17()
    : UnsafeCondDecoderTesterCase17(
      state_.Forbidden_None_instance_)
  {}
};

// op1(26:20)=110x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: PreloadRegisterPairOp,
//       base: Rn,
//       baseline: PreloadRegisterPairOp,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16), Rm(3:0)],
//       generated_baseline: PLI_register_11110110u101nnnn1111iiiiitt0mmmm_case_0,
//       rule: PLI_register,
//       safety: [Rm  ==
//               Pc => UNPREDICTABLE,
//         true => FORBIDDEN_OPERANDS],
//       true: true,
//       uses: {Rm, Rn}}
class PreloadRegisterPairOpTester_Case18
    : public PreloadRegisterPairOpTesterCase18 {
 public:
  PreloadRegisterPairOpTester_Case18()
    : PreloadRegisterPairOpTesterCase18(
      state_.PreloadRegisterPairOp_PLI_register_instance_)
  {}
};

// op1(26:20)=111x001 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       R: R(22),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: PreloadRegisterPairOp,
//       base: Rn,
//       baseline: PreloadRegisterPairOp,
//       constraints: ,
//       defs: {},
//       fields: [R(22), Rn(19:16), Rm(3:0)],
//       generated_baseline: PLD_PLDW_register_11110111u001nnnn1111iiiiitt0mmmm_case_0,
//       is_pldw: R(22)=1,
//       rule: PLD_PLDW_register,
//       safety: [Rm  ==
//               Pc ||
//            (Rn  ==
//               Pc &&
//            is_pldw) => UNPREDICTABLE,
//         true => FORBIDDEN_OPERANDS],
//       true: true,
//       uses: {Rm, Rn}}
class PreloadRegisterPairOpTester_Case19
    : public PreloadRegisterPairOpTesterCase19 {
 public:
  PreloadRegisterPairOpTester_Case19()
    : PreloadRegisterPairOpTesterCase19(
      state_.PreloadRegisterPairOp_PLD_PLDW_register_instance_)
  {}
};

// op1(26:20)=111x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       R: R(22),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: PreloadRegisterPairOp,
//       base: Rn,
//       baseline: PreloadRegisterPairOp,
//       constraints: ,
//       defs: {},
//       fields: [R(22), Rn(19:16), Rm(3:0)],
//       generated_baseline: PLD_PLDW_register_11110111u101nnnn1111iiiiitt0mmmm_case_0,
//       is_pldw: R(22)=1,
//       rule: PLD_PLDW_register,
//       safety: [Rm  ==
//               Pc ||
//            (Rn  ==
//               Pc &&
//            is_pldw) => UNPREDICTABLE,
//         true => FORBIDDEN_OPERANDS],
//       true: true,
//       uses: {Rm, Rn}}
class PreloadRegisterPairOpTester_Case20
    : public PreloadRegisterPairOpTesterCase20 {
 public:
  PreloadRegisterPairOpTester_Case20()
    : PreloadRegisterPairOpTesterCase20(
      state_.PreloadRegisterPairOp_PLD_PLDW_register_instance_)
  {}
};

// op1(26:20)=1011x11
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101011x11xxxxxxxxxxxxxxxxxxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnpredictableTester_Case21
    : public UnsafeCondDecoderTesterCase21 {
 public:
  UnpredictableTester_Case21()
    : UnsafeCondDecoderTesterCase21(
      state_.Unpredictable_None_instance_)
  {}
};

// op1(26:20)=100xx11
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_11110100xx11xxxxxxxxxxxxxxxxxxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnpredictableTester_Case22
    : public UnsafeCondDecoderTesterCase22 {
 public:
  UnpredictableTester_Case22()
    : UnsafeCondDecoderTesterCase22(
      state_.Unpredictable_None_instance_)
  {}
};

// op1(26:20)=11xxx11 & op2(7:4)=xxx0
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_1111011xxx11xxxxxxxxxxxxxxx0xxxx_case_0,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
class UnpredictableTester_Case23
    : public UnsafeCondDecoderTesterCase23 {
 public:
  UnpredictableTester_Case23()
    : UnsafeCondDecoderTesterCase23(
      state_.Unpredictable_None_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// op1(26:20)=0010000 & op2(7:4)=0000 & Rn(19:16)=xxx1 & $pattern(31:0)=xxxxxxxxxxxx000x000000x0xxxx0000
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: SETEND_1111000100000001000000i000000000_case_0,
//       pattern: 1111000100000001000000i000000000,
//       rule: SETEND,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case0_TestCase0) {
  ForbiddenTester_Case0 tester;
  tester.Test("1111000100000001000000i000000000");
}

// op1(26:20)=0010000 & op2(7:4)=xx0x & Rn(19:16)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx0000000xxxxxxxxx
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: CPS_111100010000iii00000000iii0iiiii_case_0,
//       pattern: 111100010000iii00000000iii0iiiii,
//       rule: CPS,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case1_TestCase1) {
  ForbiddenTester_Case1 tester;
  tester.Test("111100010000iii00000000iii0iiiii");
}

// op1(26:20)=1010011
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101010011xxxxxxxxxxxxxxxxxxxx_case_0,
//       pattern: 111101010011xxxxxxxxxxxxxxxxxxxx,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       UnpredictableTester_Case2_TestCase2) {
  UnpredictableTester_Case2 tester;
  tester.Test("111101010011xxxxxxxxxxxxxxxxxxxx");
}

// op1(26:20)=1010111 & op2(7:4)=0000
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101010111xxxxxxxxxxxx0000xxxx_case_0,
//       pattern: 111101010111xxxxxxxxxxxx0000xxxx,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       UnpredictableTester_Case3_TestCase3) {
  UnpredictableTester_Case3 tester;
  tester.Test("111101010111xxxxxxxxxxxx0000xxxx");
}

// op1(26:20)=1010111 & op2(7:4)=0001 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxx1111
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: CLREX_11110101011111111111000000011111_case_0,
//       pattern: 11110101011111111111000000011111,
//       rule: CLREX,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case4_TestCase4) {
  ForbiddenTester_Case4 tester;
  tester.Test("11110101011111111111000000011111");
}

// op1(26:20)=1010111 & op2(7:4)=0100 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {actual: DataBarrier,
//       baseline: DataBarrier,
//       constraints: ,
//       fields: [option(3:0)],
//       generated_baseline: DSB_1111010101111111111100000100xxxx_case_0,
//       option: option(3:0),
//       pattern: 1111010101111111111100000100xxxx,
//       rule: DSB,
//       safety: [not option in {'1111'(3:0), '1110'(3:0), '1011'(3:0), '1010'(3:0), '0111'(3:0), '0110'(3:0), '0011'(3:0), '0010'(3:0)} => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       DataBarrierTester_Case5_TestCase5) {
  DataBarrierTester_Case5 tester;
  tester.Test("1111010101111111111100000100xxxx");
}

// op1(26:20)=1010111 & op2(7:4)=0101 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {actual: DataBarrier,
//       baseline: DataBarrier,
//       constraints: ,
//       fields: [option(3:0)],
//       generated_baseline: DMB_1111010101111111111100000101xxxx_case_0,
//       option: option(3:0),
//       pattern: 1111010101111111111100000101xxxx,
//       rule: DMB,
//       safety: [not option in {'1111'(3:0), '1110'(3:0), '1011'(3:0), '1010'(3:0), '0111'(3:0), '0110'(3:0), '0011'(3:0), '0010'(3:0)} => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       DataBarrierTester_Case6_TestCase6) {
  DataBarrierTester_Case6 tester;
  tester.Test("1111010101111111111100000101xxxx");
}

// op1(26:20)=1010111 & op2(7:4)=0110 & $pattern(31:0)=xxxxxxxxxxxx111111110000xxxxxxxx
//    = {actual: InstructionBarrier,
//       baseline: InstructionBarrier,
//       constraints: ,
//       fields: [option(3:0)],
//       generated_baseline: ISB_1111010101111111111100000110xxxx_case_0,
//       option: option(3:0),
//       pattern: 1111010101111111111100000110xxxx,
//       rule: ISB,
//       safety: [option(3:0)=~1111 => FORBIDDEN_OPERANDS]}
TEST_F(Arm32DecoderStateTests,
       InstructionBarrierTester_Case7_TestCase7) {
  InstructionBarrierTester_Case7 tester;
  tester.Test("1111010101111111111100000110xxxx");
}

// op1(26:20)=1010111 & op2(7:4)=0111
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101010111xxxxxxxxxxxx0111xxxx_case_0,
//       pattern: 111101010111xxxxxxxxxxxx0111xxxx,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       UnpredictableTester_Case8_TestCase8) {
  UnpredictableTester_Case8 tester;
  tester.Test("111101010111xxxxxxxxxxxx0111xxxx");
}

// op1(26:20)=1010111 & op2(7:4)=001x
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101010111xxxxxxxxxxxx001xxxxx_case_0,
//       pattern: 111101010111xxxxxxxxxxxx001xxxxx,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       UnpredictableTester_Case9_TestCase9) {
  UnpredictableTester_Case9 tester;
  tester.Test("111101010111xxxxxxxxxxxx001xxxxx");
}

// op1(26:20)=1010111 & op2(7:4)=1xxx
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101010111xxxxxxxxxxxx1xxxxxxx_case_0,
//       pattern: 111101010111xxxxxxxxxxxx1xxxxxxx,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       UnpredictableTester_Case10_TestCase10) {
  UnpredictableTester_Case10 tester;
  tester.Test("111101010111xxxxxxxxxxxx1xxxxxxx");
}

// op1(26:20)=100x001
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_11110100x001xxxxxxxxxxxxxxxxxxxx_case_0,
//       pattern: 11110100x001xxxxxxxxxxxxxxxxxxxx,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case11_TestCase11) {
  ForbiddenTester_Case11 tester;
  tester.Test("11110100x001xxxxxxxxxxxxxxxxxxxx");
}

// op1(26:20)=100x101 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       Rn: Rn(19:16),
//       actual: PreloadRegisterImm12Op,
//       base: Rn,
//       baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16)],
//       generated_baseline: PLI_immediate_literal_11110100u101nnnn1111iiiiiiiiiiii_case_0,
//       is_literal_load: Rn  ==
//               Pc,
//       pattern: 11110100u101nnnn1111iiiiiiiiiiii,
//       rule: PLI_immediate_literal,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_Case12_TestCase12) {
  PreloadRegisterImm12OpTester_Case12 tester;
  tester.Test("11110100u101nnnn1111iiiiiiiiiiii");
}

// op1(26:20)=101x001 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Rn: Rn(19:16),
//       actual: PreloadRegisterImm12Op,
//       base: Rn,
//       baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16)],
//       generated_baseline: PLD_PLDW_immediate_11110101ur01nnnn1111iiiiiiiiiiii_case_0,
//       pattern: 11110101ur01nnnn1111iiiiiiiiiiii,
//       rule: PLD_PLDW_immediate,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR],
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_Case13_TestCase13) {
  PreloadRegisterImm12OpTester_Case13 tester;
  tester.Test("11110101ur01nnnn1111iiiiiiiiiiii");
}

// op1(26:20)=101x001 & Rn(19:16)=1111
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_11110101x001xxxxxxxxxxxxxxxxxxxx_case_0,
//       pattern: 11110101x001xxxxxxxxxxxxxxxxxxxx,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       UnpredictableTester_Case14_TestCase14) {
  UnpredictableTester_Case14 tester;
  tester.Test("11110101x001xxxxxxxxxxxxxxxxxxxx");
}

// op1(26:20)=101x101 & Rn(19:16)=~1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Rn: Rn(19:16),
//       actual: PreloadRegisterImm12Op,
//       base: Rn,
//       baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16)],
//       generated_baseline: PLD_PLDW_immediate_11110101ur01nnnn1111iiiiiiiiiiii_case_1,
//       pattern: 11110101ur01nnnn1111iiiiiiiiiiii,
//       rule: PLD_PLDW_immediate,
//       safety: [Rn(19:16)=1111 => DECODER_ERROR],
//       uses: {Rn}}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_Case15_TestCase15) {
  PreloadRegisterImm12OpTester_Case15 tester;
  tester.Test("11110101ur01nnnn1111iiiiiiiiiiii");
}

// op1(26:20)=101x101 & Rn(19:16)=1111 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       actual: PreloadRegisterImm12Op,
//       base: Pc,
//       baseline: PreloadRegisterImm12Op,
//       constraints: ,
//       defs: {},
//       generated_baseline: PLD_literal_11110101u10111111111iiiiiiiiiiii_case_0,
//       pattern: 11110101u10111111111iiiiiiiiiiii,
//       rule: PLD_literal,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {Pc}}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterImm12OpTester_Case16_TestCase16) {
  PreloadRegisterImm12OpTester_Case16 tester;
  tester.Test("11110101u10111111111iiiiiiiiiiii");
}

// op1(26:20)=110x001 & op2(7:4)=xxx0
//    = {actual: Forbidden,
//       baseline: Forbidden,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_11110110x001xxxxxxxxxxxxxxx0xxxx_case_0,
//       pattern: 11110110x001xxxxxxxxxxxxxxx0xxxx,
//       safety: [true => FORBIDDEN],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       ForbiddenTester_Case17_TestCase17) {
  ForbiddenTester_Case17 tester;
  tester.Test("11110110x001xxxxxxxxxxxxxxx0xxxx");
}

// op1(26:20)=110x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: PreloadRegisterPairOp,
//       base: Rn,
//       baseline: PreloadRegisterPairOp,
//       constraints: ,
//       defs: {},
//       fields: [Rn(19:16), Rm(3:0)],
//       generated_baseline: PLI_register_11110110u101nnnn1111iiiiitt0mmmm_case_0,
//       pattern: 11110110u101nnnn1111iiiiitt0mmmm,
//       rule: PLI_register,
//       safety: [Rm  ==
//               Pc => UNPREDICTABLE,
//         true => FORBIDDEN_OPERANDS],
//       true: true,
//       uses: {Rm, Rn}}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterPairOpTester_Case18_TestCase18) {
  PreloadRegisterPairOpTester_Case18 tester;
  tester.Test("11110110u101nnnn1111iiiiitt0mmmm");
}

// op1(26:20)=111x001 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       R: R(22),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: PreloadRegisterPairOp,
//       base: Rn,
//       baseline: PreloadRegisterPairOp,
//       constraints: ,
//       defs: {},
//       fields: [R(22), Rn(19:16), Rm(3:0)],
//       generated_baseline: PLD_PLDW_register_11110111u001nnnn1111iiiiitt0mmmm_case_0,
//       is_pldw: R(22)=1,
//       pattern: 11110111u001nnnn1111iiiiitt0mmmm,
//       rule: PLD_PLDW_register,
//       safety: [Rm  ==
//               Pc ||
//            (Rn  ==
//               Pc &&
//            is_pldw) => UNPREDICTABLE,
//         true => FORBIDDEN_OPERANDS],
//       true: true,
//       uses: {Rm, Rn}}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterPairOpTester_Case19_TestCase19) {
  PreloadRegisterPairOpTester_Case19 tester;
  tester.Test("11110111u001nnnn1111iiiiitt0mmmm");
}

// op1(26:20)=111x101 & op2(7:4)=xxx0 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       R: R(22),
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       actual: PreloadRegisterPairOp,
//       base: Rn,
//       baseline: PreloadRegisterPairOp,
//       constraints: ,
//       defs: {},
//       fields: [R(22), Rn(19:16), Rm(3:0)],
//       generated_baseline: PLD_PLDW_register_11110111u101nnnn1111iiiiitt0mmmm_case_0,
//       is_pldw: R(22)=1,
//       pattern: 11110111u101nnnn1111iiiiitt0mmmm,
//       rule: PLD_PLDW_register,
//       safety: [Rm  ==
//               Pc ||
//            (Rn  ==
//               Pc &&
//            is_pldw) => UNPREDICTABLE,
//         true => FORBIDDEN_OPERANDS],
//       true: true,
//       uses: {Rm, Rn}}
TEST_F(Arm32DecoderStateTests,
       PreloadRegisterPairOpTester_Case20_TestCase20) {
  PreloadRegisterPairOpTester_Case20 tester;
  tester.Test("11110111u101nnnn1111iiiiitt0mmmm");
}

// op1(26:20)=1011x11
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_111101011x11xxxxxxxxxxxxxxxxxxxx_case_0,
//       pattern: 111101011x11xxxxxxxxxxxxxxxxxxxx,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       UnpredictableTester_Case21_TestCase21) {
  UnpredictableTester_Case21 tester;
  tester.Test("111101011x11xxxxxxxxxxxxxxxxxxxx");
}

// op1(26:20)=100xx11
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_11110100xx11xxxxxxxxxxxxxxxxxxxx_case_0,
//       pattern: 11110100xx11xxxxxxxxxxxxxxxxxxxx,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       UnpredictableTester_Case22_TestCase22) {
  UnpredictableTester_Case22 tester;
  tester.Test("11110100xx11xxxxxxxxxxxxxxxxxxxx");
}

// op1(26:20)=11xxx11 & op2(7:4)=xxx0
//    = {actual: Unpredictable,
//       baseline: Unpredictable,
//       constraints: ,
//       defs: {},
//       generated_baseline: Unnamed_1111011xxx11xxxxxxxxxxxxxxx0xxxx_case_0,
//       pattern: 1111011xxx11xxxxxxxxxxxxxxx0xxxx,
//       safety: [true => UNPREDICTABLE],
//       true: true,
//       uses: {}}
TEST_F(Arm32DecoderStateTests,
       UnpredictableTester_Case23_TestCase23) {
  UnpredictableTester_Case23 tester;
  tester.Test("1111011xxx11xxxxxxxxxxxxxxx0xxxx");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
