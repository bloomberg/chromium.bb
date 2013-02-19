/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Defines decoder testers for decoder classes.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_INST_CLASSES_TESTERS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_INST_CLASSES_TESTERS_H_

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif

#include "native_client/src/trusted/validator_arm/gen/arm32_decode_named_classes.h"
#include "native_client/src/trusted/validator_arm/decoder_tester.h"

namespace nacl_arm_test {

// Implements an Arm32DecoderTester with a parse precondition that
// the conditions bits (28-31) are 1111.
// +--------+--------------------------------------------------------+
// |31302918|272625242322212019181716151413121110 9 8 7 6 5 4 3 2 1 0|
// +--------+--------------------------------------------------------+
// |  cond  |                                                        |
// +--------+--------------------------------------------------------+
//
// if cond!=1111 then UNDEFINED.
class UncondDecoderTester : public Arm32DecoderTester {
 public:
  explicit UncondDecoderTester(const NamedClassDecoder& decoder)
      : Arm32DecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  // Used to get the conditional bits out of the instruction.
  nacl_arm_dec::UncondDecoder cond_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(UncondDecoderTester);
};

// Implements a decoder tester for an UnsafeUncondDecoder
class UnsafeUncondDecoderTester : public UncondDecoderTester {
 public:
  explicit UnsafeUncondDecoderTester(const NamedClassDecoder& decoder)
      : UncondDecoderTester(decoder),
        expected_decoder_(nacl_arm_dec::UNINITIALIZED) {}
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::UnsafeUncondDecoder expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(UnsafeUncondDecoderTester);
};

// Implements a decoder tester for decoder CondDecoder.
// Nop<c>
// +--------+--------------------------------------------------------+
// |31302918|272625242322212019181716151413121110 9 8 7 6 5 4 3 2 1 0|
// +--------+--------------------------------------------------------+
// |  cond  |                                                        |
// +--------+--------------------------------------------------------+
class CondDecoderTester : public Arm32DecoderTester {
 public:
  explicit CondDecoderTester(const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  // Used to get the conditional bits out of the instruction.
  nacl_arm_dec::CondDecoder cond_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(CondDecoderTester);
};

// Implements a decoder tester for an UnsafeCondDecoder
class UnsafeCondDecoderTester : public CondDecoderTester {
 public:
  explicit UnsafeCondDecoderTester(const NamedClassDecoder& decoder)
      : CondDecoderTester(decoder),
        expected_decoder_(nacl_arm_dec::UNINITIALIZED) {}
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::UnsafeCondDecoder expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(UnsafeCondDecoderTester);
};

// Implements a decoder tester for vfp operations with possible condition
// flags.
// +--------+--------------------------------+--------+----------------+
// |31302928|27262524232221201918171615141312|1110 9 8| 7 6 5 4 3 2 1 0|
// +--------+--------------------------------+--------+----------------+
// |  cond  |                                | coproc |                |
// +--------+--------------------------------+--------+----------------+
// A generic VFP instruction that (by default) only affects vector
// register banks. Hence, they do not change general purpose registers.
class CondVfpOpTester : public CondDecoderTester {
 public:
  explicit CondVfpOpTester(const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::CondVfpOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(CondVfpOpTester);
};

// Same as CondVfpOp, but with the extra restriction that ARM deprecates the
// conditional execution of any Advanced SIMD instruction encoding that is not
// also available as a VFP instruction encoding.
class CondAdvSIMDOpTester : public CondDecoderTester {
 public:
  explicit CondAdvSIMDOpTester(const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::CondAdvSIMDOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(CondAdvSIMDOpTester);
};

// Implements a decoder tester for decoder MoveImmediate12ToApsr.
// MSR<c> <spec_reg>, #<const>
// +--------+----------------+----+------------+------------------------+
// |31302928|2726252423222120|1918|171615141312|1110 9 8 7 6 5 4 3 2 1 0|
// +--------+----------------+----+------------+------------------------+
// |  cond  |                |mask|            |         imm12          |
// +--------+----------------+----+------------+------------------------+
// Definitions:
//    mask = Defines which parts of the APSR is set. When mask<1>=1,
//           the N, Z, C, V, and Q bits (31:27) are updated. When
//           mask<0>=1, the GE bits (3:0 and 19:16) are updated.
// Note: If mask=3, then N, Z, C, V, Q, and GE bits are updated.
// Note: mask=0 should not parse.
class MoveImmediate12ToApsrTester : public CondDecoderTester {
 public:
  explicit MoveImmediate12ToApsrTester(const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::MoveImmediate12ToApsr expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MoveImmediate12ToApsrTester);
};

// Implements a decoder tester for decoder Immediate16Use.
// Op #<imm16>
// +--------+----------------+------------------------+--------+-------+
// |31302928|2726252423222120|19181716151413121110 9 8| 7 6 5 4|3 2 1 0|
// +--------+----------------+------------------------+--------+-------+
// |  cond  |                |         imm12          |        |  imm4 |
// +--------+----------------+------------------------+--------+-------+
class Immediate16UseTester : public CondDecoderTester {
 public:
  explicit Immediate16UseTester(const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Immediate16Use expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Immediate16UseTester);
};

// Implements a decoder tester for BranchImmediate24.
// B{L}<c> <label>
// +--------+------+--+------------------------------------------------+
// |31302928|272625|24|2322212019181716151413121110 9 8 7 6 5 4 3 2 1 0|
// +--------+------+--+------------------------------------------------+
// |  cond  |      | P|                 imm24                          |
// +--------+------+--+------------------------------------------------+
class BranchImmediate24Tester : public CondDecoderTester {
 public:
  explicit BranchImmediate24Tester(const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::BranchImmediate24 expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BranchImmediate24Tester);
};

// Implements a decoder tester for decoder BranchToRegister.
// Op<c> <Rm>
// +--------+---------------------------------------------+--+--+--------+
// |31302928|2726252423222212019181716151413121110 9 8 7 6| 5| 4| 3 2 1 0|
// +--------+---------------------------------------------+--+--+--------+
// |  cond  |                                             | L|  |   Rm   |
// +--------+---------------------------------------------+--+--+--------+
class BranchToRegisterTester : public CondDecoderTester {
 public:
  explicit BranchToRegisterTester(const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::BranchToRegister expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BranchToRegisterTester);
};

// implements a decoder tester for decoder BranchToRegister with a
// constraint that if Rm is R15, the instruction is unpredictable.
class BranchToRegisterTesterRegsNotPc : public BranchToRegisterTester {
 public:
  explicit BranchToRegisterTesterRegsNotPc(const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BranchToRegisterTesterRegsNotPc);
};

// Implements a decoder tester for decoder Unary1RegisterImmediateOp12
// Models a 1-register assignment of a 12-bit immediate.
// Op(S)<c> Rd, #const
// +--------+--------------+--+--------+--------+------------------------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7 6 5 4 3 2 1 0|
// +--------+--------------+--+--------+--------+------------------------+
// |  cond  |              | S|        |   Rd   |         imm12          |
// +--------+--------------+--+--------+--------+------------------------+
//   setflags := S=1; imm32 := ARMExpandImm(imm12);
//   defs := {Rd, NZCV if setflags else None};
//   safety := (Rd=1111 & S=1) => DECODER_ERROR &
//             Rd=1111 => FORBIDDEN_OPERANDS;
class Unary1RegisterImmediateOp12Tester : public CondDecoderTester {
 public:
  explicit Unary1RegisterImmediateOp12Tester(
      const NamedClassDecoder& decoder)
      : CondDecoderTester(decoder) {}
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Unary1RegisterImmediateOp12 expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterImmediateOp12Tester);
};

// Implements a decoder tester for decoder Unary1RegisterImmediateOp.
// Op(S)<c> Rd, #const
// +--------+--------------+--+--------+--------+------------------------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7 6 5 4 3 2 1 0|
// +--------+--------------+--+--------+--------+------------------------+
// |  cond  |              | S|  imm4  |   Rd   |         imm12          |
// +--------+--------------+--+--------+--------+------------------------+
// Definitions:
//    Rd = The destination register.
//    const = ZeroExtend(imm4:imm12, 32)
class Unary1RegisterImmediateOpTester
    : public Unary1RegisterImmediateOp12Tester {
 public:
  explicit Unary1RegisterImmediateOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Unary1RegisterImmediateOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterImmediateOpTester);
};

// Implements a decoder tester for decoder Unary1RegisterImmediateOpPc
// Models a 1-register assignment that implicitly uses Pc.
// Op(S)<c> Rd, #const
// +--------+------------------------+--------+------------------------+
// |31302928|272625242322212019181716|15141312|1110 9 8 7 6 5 4 3 2 1 0|
// +--------+------------------------+--------+------------------------+
// |  cond  |                        |   Rd   |         imm12          |
// +--------+------------------------+--------+------------------------+
// Definitions:
//    Rd = The destination register.
//    const = ZeroExtend(imm4:imm12, 32)
//
//   imm32 := ARMExpandImm(imm12);
//   defs := {Rd};
//   safety := Rd=1111 => FORBIDDEN_OPERANDS;
//   uses := {Pc};
class Unary1RegisterImmediateOpPcTester : public CondDecoderTester {
 public:
  explicit Unary1RegisterImmediateOpPcTester(
      const NamedClassDecoder& decoder)
      : CondDecoderTester(decoder) {}
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Unary1RegisterImmediateOpPc expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterImmediateOpPcTester);
};

// Implements a decoder tester for decoder Unary1RegisterImmediateOp
// with a constraint that if Rd is R15, the instruction is unpredictable.
class Unary1RegisterImmediateOpTesterRegsNotPc
    : public Unary1RegisterImmediateOpTester {
 public:
  explicit Unary1RegisterImmediateOpTesterRegsNotPc(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterImmediateOpTesterRegsNotPc);
};

// Implements a decoder tester for decoder Unary1RegisterImmediateOp,
// and should not parse when Rd=15 and S=1.
class Unary1RegisterImmediateOpTesterNotRdIsPcAndS
    : public Unary1RegisterImmediateOpTester {
 public:
  explicit Unary1RegisterImmediateOpTesterNotRdIsPcAndS(
      const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterImmediateOpTesterNotRdIsPcAndS);
};

// Models a 1-register binary operation with two immediate 5 values.
// Op<c> Rd, #lsb, #width
// +--------+--------------+----------+--------+----------+--------------+
// |31302928|27262524232221|2019181716|15141312|1110 9 8 7| 6 5 4 3 2 1 0|
// +--------+--------------+----------+--------+----------+--------------+
// |  cond  |              |    msb   |   Rd   |   lsb    |              |
// +--------+--------------+----------+--------+----------+--------------+
// Definitions
//    Rd = The destination register.
//    lsb = The least significant bit to be modified.
//    msb = lsb + width - 1 - The most significant bit to be modified
//    width = msb - lsb + 1 - The number of bits to be modified.
//
// If Rd is R15 or msb < lsb, the instruction is unpredictable.
// NaCl disallows writing to PC to cause a jump.
// Note: Currently, only implements bfc. (A8-46).
class Unary1RegisterBitRangeMsbGeLsbTester : public CondDecoderTester {
 public:
  explicit Unary1RegisterBitRangeMsbGeLsbTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Unary1RegisterBitRangeMsbGeLsb expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterBitRangeMsbGeLsbTester);
};

// Models a 2-register binary operation with two immediate values
// defining a bit range.
// Op<c> Rd, Rn, #<lsb>, #width
// +--------+--------------+----------+--------+----------+------+--------+
// |31302928|27262524232221|2019181716|15141312|1110 9 8 7| 6 5 4| 3 2 1 0|
// +--------+--------------+----------+--------+----------+------+--------+
// |  cond  |              |    msb   |   Rd   |    lsb   |      |   Rn   |
// +--------+--------------+----------+--------+----------+------+--------+
// Definitions:
//   Rd = The destination register.
//   Rn = The first operand
//   lsb = The least significant bit to be modified.
//   msb = The most significant bit to be modified.
//
// If Rd=R15 or msb < lsb, the instruction is unpredictable.
//
// NaCl disallows writing Pc to cause a jump.
class Binary2RegisterBitRangeMsbGeLsbTester
    : public CondDecoderTester {
 public:
  explicit Binary2RegisterBitRangeMsbGeLsbTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Binary2RegisterBitRangeMsbGeLsb expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Binary2RegisterBitRangeMsbGeLsbTester);
};

// Implements a Binary2RegisterBitRangeNotRnIsPcBitfieldExtracttester.
class Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester
    : public CondDecoderTester {
 public:
  explicit Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Binary2RegisterBitRangeNotRnIsPcBitfieldExtract
  expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Binary2RegisterBitRangeNotRnIsPcBitfieldExtractTester);
};

// Implements a decoder tester for decoder BinaryRegisterImmediateTest
// Op(S)<c> Rn, #<const>
// +--------+--------------+--+--------+--------+------------------------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7 6 5 4 3 2 1 0|
// +--------+--------------+--+--------+--------+------------------------+
// |  cond  |              | S|   Rn   |        |        imm12           |
// +--------+--------------+--+--------+--------+------------------------+
// Definitions:
//    Rn 0 The operand register.
//    const = ARMExpandImm_C(imm12, APSR.C)
class BinaryRegisterImmediateTestTester : public CondDecoderTester {
 public:
  explicit BinaryRegisterImmediateTestTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::BinaryRegisterImmediateTest expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BinaryRegisterImmediateTestTester);
};

// Implements a decoder tester for decoder Unary2RegisterOp.
// Op(S)<c> <Rd>, <Rm>
// +--------+--------------+--+--------+--------+----------------+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7 6 5 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+----------------+--------+
// |  cond  |              | S|        |   Rd   |                |   Rm   |
// +--------+--------------+--+--------+--------+----------------+--------+
// Note: NaCl disallows writing to PC to cause a jump.
// Definitions:
//    Rd - The destination register.
//    Rm - The source register.
class Unary2RegisterOpTester : public CondDecoderTester {
 public:
  explicit Unary2RegisterOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Unary2RegisterOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterOpTester);
};

// Implements a decoder tester for decoder Unary2RegisterOp
// where register Rm is not PC.
class Unary2RegisterOpNotRmIsPcTester : public Unary2RegisterOpTester {
 public:
  explicit Unary2RegisterOpNotRmIsPcTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Unary2RegisterOpNotRmIsPc expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterOpNotRmIsPcTester);
};

// Implements a decoder tester for decoder Unary2RegisterOp, and
// should not parse when Rd=15 and S=1.
class Unary2RegisterOpTesterNotRdIsPcAndS : public Unary2RegisterOpTester {
 public:
  explicit Unary2RegisterOpTesterNotRdIsPcAndS(
      const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterOpTesterNotRdIsPcAndS);
};

// Implements a decoder tester for Unary2RegisterOpNotRmIsPc where
// all registers are not pc.
class Unary2RegisterOpNotRmIsPcTesterRegsNotPc
    : public Unary2RegisterOpNotRmIsPcTester {
 public:
  explicit Unary2RegisterOpNotRmIsPcTesterRegsNotPc(
      const NamedClassDecoder& decoder)
      : Unary2RegisterOpNotRmIsPcTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterOpNotRmIsPcTesterRegsNotPc);
};

// Implements a decoder tester for Unary2RegisterShiftedOp.
// Op(S)<c> <Rd>, <Rm> (, #<imm> | {, <shift> })
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7| 6 5| 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// |  cond  |              | S|        |   Rd   |   imm5   |type|  |   Rm   |
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// Definitions:
//    Rd - The destination register.
//    Rm - The source register.
//
// NaCl disallows writing to PC to cause a jump.
class Unary2RegisterShiftedOpTester : public Unary2RegisterOpTester {
 public:
  explicit Unary2RegisterShiftedOpTester(const NamedClassDecoder& decoder)
      : Unary2RegisterOpTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Unary2RegisterShiftedOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterShiftedOpTester);
};

// Implements a decoder tester for Binary3RegisterShiftedOp
// Op(S)<c> <Rd>, <Rn>, <Rm> {, <shift>}
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7| 6 5| 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// |  cond  |              | S|  Rn    |   Rd   |   imm5   |type|  |   Rm   |
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// Definitions:
//    Rd - The destination register.
//    Rn - The first source register.
//    Rm - The second source register.
class Binary3RegisterShiftedOpTester : public Unary2RegisterShiftedOpTester {
 public:
  explicit Binary3RegisterShiftedOpTester(const NamedClassDecoder& decoder)
      : Unary2RegisterShiftedOpTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Binary3RegisterShiftedOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterShiftedOpTester);
};

// Implements a decoder tester for decoder Binary2RegisterImmediateOp.
// Op(S)<c> <Rd>, <Rn>, #<const>
// +--------+--------------+--+--------+--------+------------------------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7 6 5 4 3 2 1 0|
// +--------+--------------+--+--------+--------+------------------------+
// |  cond  |              | S|   Rn   |   Rd   |          imm12         |
// +--------+--------------+--+--------+--------+------------------------+
// Definitions:
//    Rd = The destination register.
//    Rn = The first operand register.
//    const = The immediate value to be used as the second argument.
class Binary2RegisterImmediateOpTester : public  CondDecoderTester {
 public:
  explicit Binary2RegisterImmediateOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  // If true, the Rd=15 Nacl Check is applied.
  bool apply_rd_is_pc_check_;
  nacl_arm_dec::Binary2RegisterImmediateOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary2RegisterImmediateOpTester);
};

// Implements a decoder tester for decoder Binary2RegisterImmediateOp,
// and should not parse when Rd=15 and S=1.
class Binary2RegisterImmediateOpTesterNotRdIsPcAndS
    : public Binary2RegisterImmediateOpTester {
 public:
  explicit Binary2RegisterImmediateOpTesterNotRdIsPcAndS(
      const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary2RegisterImmediateOpTesterNotRdIsPcAndS);
};

// Implements a decoder tester for decoder Binary2RegisterImmediateOp,
// where it should not parse when either:
// (1) Rd=15 and S=1; or
// (2) Rn=15 and S=0;
class Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  explicit Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS(
      const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS);
};

// Implements a decoder tester for decoder Binary3RegisterOp.
// Op(S)<c> <Rd>, <Rn>, <Rm>
// +--------+--------------+--+--------+--------+--------+--------+--------+
// |31302918|27262524232221|20|19181716|15141312|1110 9 8| 7 6 4 3| 3 2 1 0|
// +--------+--------------+--+--------+--------+--------+--------+--------+
// |  cond  |              | S|        |   Rd   |   Rm   |        |   Rn   |
// +--------+--------------+--+--------+--------+--------+--------+--------+
// Definitions:
//    Rd - The destination register.
//    Rn - The first operand register.
//    Rm - The second operand register.
//    S - Defines if the flags regsiter is updated.
class Binary3RegisterOpTester : public CondDecoderTester {
 public:
  explicit Binary3RegisterOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Binary3RegisterOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpTester);
};

// Implements a decoder tester for Binary3RegisterOp with a constraint
// that if Rd, Rm, or Rn is R15, the instruction is unpredictable.
class Binary3RegisterOpTesterRegsNotPc
    : public Binary3RegisterOpTester {
 public:
  explicit Binary3RegisterOpTesterRegsNotPc(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpTesterRegsNotPc);
};

// Implements a decoder tester for decoder Binary3RegisterOpAltA
// Op(S)<c> <Rd>, <Rn>, <Rm>
// +--------+--------------+--+--------+--------+--------+--------+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8| 7 6 5 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+--------+--------+--------+
// |  cond  |              | S|   Rd   |        |   Rm   |        |   Rn   |
// +--------+--------------+--+--------+--------+--------+--------+--------+
// Definitions:
//    Rd - The destination register.
//    Rn - The first operand register.
//    Rm - The second operand register.
//    S - Defines if the flags regsiter is updated.
class Binary3RegisterOpAltATester : public CondDecoderTester {
 public:
  explicit Binary3RegisterOpAltATester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Binary3RegisterOpAltA expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpAltATester);
};

// Implements a decoder tester for Binary3RegisterOpAltA with a
// constraint that if Rd, Rm, or Rn is R15, the instruction is
// unpredictable.
class Binary3RegisterOpAltATesterRegsNotPc
    : public Binary3RegisterOpAltATester {
 public:
  explicit Binary3RegisterOpAltATesterRegsNotPc(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpAltATesterRegsNotPc);
};

// Models a 3-register binary operation of the form:
// Op(S)<c> <Rd>, <Rn>, <Rm>
// +--------+--------------+--+--------+--------+----------------+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7 6 5 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+----------------+--------+
// |  cond  |              | S|   Rn   |   Rd   |                |   Rm   |
// +--------+--------------+--+--------+--------+----------------+--------+
// Definitions:
//    Rd - The destination register.
//    Rn - The first operand
//    Rm - The second operand
//    S - Defines if the flags regsiter is updated.
class Binary3RegisterOpAltBNoCondUpdatesTester : public CondDecoderTester {
 public:
  explicit Binary3RegisterOpAltBNoCondUpdatesTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  // When true, tester should test conditions flag S.
  bool test_conditions_;
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpAltBNoCondUpdatesTester);
};

// Implements a decoder tester for Binary3RegisterOpAltBNoCondUpdates with a
// constraint that if Rd, Rm, or Rn is R15, the instruction is
// unpredictable.
class Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  explicit Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc);
};

// Implements a decoder tester for Binary3RegisterOpAltBNoCondUpdates with a
// parse constraint that Rn!=R15, and if Rm or Rd is R15, the instruction is
// unpredictable.
class Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPcAndRegsNotPc
    : public Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc {
 public:
  explicit Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPcAndRegsNotPc(
      const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPcAndRegsNotPc);
};

// Implements a decoder tester for Binary3RegisterOpAltBNoCondUpdates
// with a parse constraint that Rn!=R15.
class Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc
    : public Binary3RegisterOpAltBNoCondUpdatesTester {
 public:
  explicit Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc(
      const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Binary3RegisterOpAltBNoCondUpdatesTesterNotRnIsPc);
};

// Implements a decoder tester for decoder Binary4RegisterDualOp
// Op(S)<c> <Rd>, <Rn>, <Rm>, <Ra>
// +--------+--------------+--+--------+--------+--------+--------+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8| 7 6 5 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+--------+--------+--------+
// |  cond  |              | S|   Rd   |   Ra   |   Rm   |        |   Rn   |
// +--------+--------------+--+--------+--------+--------+--------+--------+
// Definitions:
//    Rd - The destination register (of the operation on the inner operation
//         and Ra).
//    Rn - The first operand to the inner operation.
//    Rm - The second operand to the inner operation.
//    Ra - The second operand to the outer operation.
class Binary4RegisterDualOpTester : public CondDecoderTester {
 public:
  explicit Binary4RegisterDualOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Binary4RegisterDualOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary4RegisterDualOpTester);
};

// Implements a decoder tester for Binary4RegisterDualOp with a constraint
// that if Rd, Ra, Rm, or Rn is R15, the instruction is unpredictable.
class Binary4RegisterDualOpTesterRegsNotPc
    : public Binary4RegisterDualOpTester {
 public:
  explicit Binary4RegisterDualOpTesterRegsNotPc(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary4RegisterDualOpTesterRegsNotPc);
};

// Implements a Binary4RegisterDualOpTesterRegsNotPc with the parse
// precondition that Ra!=Pc.
class Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc
    : public Binary4RegisterDualOpTesterRegsNotPc {
 public:
  explicit Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc(
      const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Binary4RegisterDualOpTesterNotRaIsPcAndRegsNotPc);
};

// Implements a decoder tester for decoder Binary4RegisterDualResult
// Op(S)<c> <RdLo>, <RdHi>, <Rn>, <Rm>
// +--------+----------------+--------+--------+--------+--------+--------+
// |31302928|2726252423222120|19181716|15141312|1110 9 8| 7 6 5 4| 3 2 1 0|
// +--------+----------------+--------+--------+--------+--------+--------+
// |  cond  |                |  RdHi  |  RdLo  |   Rm   |        |   Rn   |
// +--------+----------------+--------+--------+--------+--------+--------+
// Definitions:
//    RdHi - Input to the outer binary operation, and the upper 32-bit result
//           of that operation.
//    RdLo - Input to the outer bianry operation, and the lower 32-bit result
//           of that operation.
//    Rn - The first operand to the inner binary operation.
//    Rm - The second operand to the inner binary operation.
// Note: The result of the inner operation is a 64-bit value used as an
//    argument to the outer operation.
class Binary4RegisterDualResultTester : public CondDecoderTester {
 public:
  explicit Binary4RegisterDualResultTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Binary4RegisterDualResult expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary4RegisterDualResultTester);
};

// Implements a decoder tester for Binary4RegisterDualResult with a constraint
// that if RdHi, RdLo, Rm, or Rn is R15, the instruction is unpredictable.
class Binary4RegisterDualResultTesterRegsNotPc
    : public Binary4RegisterDualResultTester {
 public:
  explicit Binary4RegisterDualResultTesterRegsNotPc(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary4RegisterDualResultTesterRegsNotPc);
};

// Tests a 2-register load (exclusive) operation.
// Op<c> <Rt>, [<Rn>]
// +--------+----------------+--------+--------+------------------------+
// |31302928|2726252423222120|19181716|15141312|1110 9 8 7 6 5 4 3 2 1 0|
// +--------+----------------+--------+--------+------------------------+
// |  cond  |                |   Rn   |   Rt   |                        |
// +--------+----------------+--------+--------+------------------------+
// Definitions:
//    Rn - The base register.
//    Rt - The destination register.
//
// if Rt or Rn is R15, then unpredictable.
// NaCl disallows writing to PC to cause a jump.
class LoadExclusive2RegisterOpTester : public CondDecoderTester {
 public:
  explicit LoadExclusive2RegisterOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::LoadExclusive2RegisterOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadExclusive2RegisterOpTester);
};

// Tests a 2-register load (exclusive) operation where the source is double
// wide (i.e. Rt and Rt2).
//
// Additional ARM constraints:
//    Rt<0>=1 then unpredictable.
//    Rt=14, then unpredictable (i.e. Rt2=R15).
//    Rn=Rt2, then unpredictable.
class LoadExclusive2RegisterDoubleOpTester
    : public LoadExclusive2RegisterOpTester {
 public:
  explicit LoadExclusive2RegisterDoubleOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::LoadExclusive2RegisterDoubleOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadExclusive2RegisterDoubleOpTester);
};

// Defines a tester for class LoadRegisterImm8Op.
// Op<c> <Rt>, <label>
// +--------+------+--+--+--+--+----------+--------+--------+--------+--------+
// |31302928|272625|24|23|22|21|2019181716|15141312|1110 9 8| 7 6 5 4| 3 2 1 0|
// +--------+------+--+--+--+--+----------+--------+--------+--------+--------+
// |  cond  |      | P| U|  | W|          |   Rt   |  imm4H |        |  imm4L |
// +--------+------+--+--+--+--+----------+--------+--------+--------+--------+
//  imm32 := ZeroExtend(imm4H:imm4L, 32);
//  add := U=1;
//  base := Align(PC, 4);
//  address := base + imm32 if add else base - imm32;
//  defs := {Rt};
//  uses := {Pc};
//  safety := P=0 & W=1 => DECODER_ERROR &
//            P == W => UNPREDICTABLE &
//            Rt == Pc => UNPREDICTABLE &
//            # TODO(karl) Missing check:
//            # (ArchVersion() < 7) &
//            # not (UnalignedSupport() | address(0)=0) => UNKNOWN
class LoadRegisterImm8OpTester : public CondDecoderTester {
 public:
  explicit LoadRegisterImm8OpTester(const NamedClassDecoder& decoder)
      : CondDecoderTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::LoadRegisterImm8Op expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadRegisterImm8OpTester);
};

// Defines a tester for class LoadRegisterImm8DoubleOp
// Defines a (PC-relative) load 8-bit immediate operation that
// has destination registers Rt and Rt2.
// Rt2 := Rt + 1;
// base := Align(PC, 4);
// address := base + imm32 if add else base - imm32;
// base := Pc;
// defs := {Rt, Rt2};
// uses := {Pc};
// is_literal_load := true;
// safety := Rt(0)=1 => UNPREDICTABLE &
//           Rt2 == Pc => UNPREDICTABLE;
class LoadRegisterImm8DoubleOpTester : public LoadRegisterImm8OpTester {
 public:
  explicit LoadRegisterImm8DoubleOpTester(const NamedClassDecoder& decoder)
      : LoadRegisterImm8OpTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::LoadRegisterImm8DoubleOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadRegisterImm8DoubleOpTester);
};

// Models a 2-register load/store immediate operation of the forms:
// Op<c> <Rt>, [<Rn>{, #+/-<imm8>}]
// Op<c> <Rt>, [<Rn>], #+/-<imm8>
// Op<c> <Rt>, [<Rn>, #+/-<imm8>]!
// +--------+------+--+--+--+--+--+--------+--------+--------+--------+--------+
// |31302928|272625|24|23|22|21|20|19181716|15141312|1110 9 8| 7 6 5 4| 3 2 1 0|
// +--------+------+--+--+--+--+--+--------+--------+--------+--------+--------+
// |  cond  |      | P| U|  | W|  |   Rn   |   Rt   |  imm4H |        |  imm4L |
// +--------+------+--+--+--+--+--+--------+--------+--------+--------+--------+
// wback = (P=0 || W=1)
//
// if P=0 and W=1, should not parse as this instruction.
// if Rt=15 then Unpredictable
// if wback && (Rn=15 or Rn=Rt) then unpredictable.
// NaCl disallows writing to PC.
class LoadStore2RegisterImm8OpTester : public CondDecoderTester {
 public:
  explicit LoadStore2RegisterImm8OpTester(
      const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::LoadStore2RegisterImm8Op expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore2RegisterImm8OpTester);
};

// Defines a LoadStore2RegisterImm8OpTester with the added constraint
// that it doesn't parse when Rn=15
class LoadStore2RegisterImm8OpTesterNotRnIsPc
    : public LoadStore2RegisterImm8OpTester {
 public:
  explicit LoadStore2RegisterImm8OpTesterNotRnIsPc(
      const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore2RegisterImm8OpTesterNotRnIsPc);
};

// Models a 2-register load/store immediate operation where the source/target
// is double wide (i.e. Rt and Rt2).
class LoadStore2RegisterImm8DoubleOpTester
    : public LoadStore2RegisterImm8OpTester {
 public:
  explicit LoadStore2RegisterImm8DoubleOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::LoadStore2RegisterImm8DoubleOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore2RegisterImm8DoubleOpTester);
};

// Defines a LoadStore2RegisterImm8DoubleOpTester with the added constraint
// that it doesn't parse when Rn=15.
class LoadStore2RegisterImm8DoubleOpTesterNotRnIsPc
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  explicit LoadStore2RegisterImm8DoubleOpTesterNotRnIsPc(
      const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      LoadStore2RegisterImm8DoubleOpTesterNotRnIsPc);
};

// Models a prefetch load/store 12-bit immediate operation.
// Op [<Rn>, #+/-<imm12>]
// +--------+--------+--+--+----+--------+--------+------------------------+
// |31302928|27262524|23|22|2120|19181716|15141312|1110 9 8 7 6 5 4 3 2 1 0|
// +--------+--------+--+--+----+--------+--------+------------------------+
// |  cond  |        | U| R|    |   Rn   |        |         imm12          |
// +--------+--------+--+--+----+--------+--------+------------------------+
// cond=1111
// U - defines direction(+/-)
// R - 1 if memory access is a read (otherwise write).
// Rn - The base register.
// imm12 - The offset from the address.
//
// Note: Currently we don't mask addresses for preload instructions,
// since an actual load doesn't occur, and it doesn't fault the processor.
// Hence, we do not define virtual base_address_register.
// TODO(karl): Verify that we don't want to mask preload addresses.
class PreloadRegisterImm12OpTester : public UncondDecoderTester {
 public:
  explicit PreloadRegisterImm12OpTester(const NamedClassDecoder& decoder)
      : UncondDecoderTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::PreloadRegisterImm12Op expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PreloadRegisterImm12OpTester);
};


// Models a prefetch register pair load/store tester.
// Op [<Rn>, #+/-<Rm>{, <shift>}]
// +--------+--------+--+--+----+--------+--------+----------+----+--+--------+
// |31302928|27262524|23|22|2120|19181716|15141312|1110 9 8 7| 6 5| 4| 3 2 1 0|
// +--------+--------+--+--+----+--------+--------+----------+----+--+--------+
// |  cond  |        | U| R|    |   Rn   |        |   imm5   |type|  |        |
// +--------+--------+--+--+----+--------+--------+---------------------------+
// cond=1111
// U - defines direction(+/-)
// R - 1 if memory access is a read (otherwise write).
// Rn - The base register.
// Rm - The offset that is optionally shifted and applied to the value of <Rn>
//      to form the address.
//
// Note: Currently we don't mask addresses for preload instructions,
// since an actual load doesn't occur, and it doesn't fault the processor.
// Hence, we do not define virtual base_address_register.
// Note: We assume that we don't care if the conditions flags are set.
// TODO(karl): Verify that we don't want to mask preload addresses.
class PreloadRegisterPairOpTester : public UncondDecoderTester {
 public:
  explicit PreloadRegisterPairOpTester(const NamedClassDecoder& decoder)
      : UncondDecoderTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::PreloadRegisterPairOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PreloadRegisterPairOpTester);
};

// Implements a tester for PreloadRegisterPairOpWAndRnNotPc
class PreloadRegisterPairOpWAndRnNotPcTester
    : public PreloadRegisterPairOpTester {
 public:
  explicit PreloadRegisterPairOpWAndRnNotPcTester(
      const NamedClassDecoder& decoder)
      : PreloadRegisterPairOpTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PreloadRegisterPairOpWAndRnNotPcTester);
};

// Models a 2-register load/store 12-bit immediate operation of the forms:
// Op<c> <Rt>, [<Rn> {, #+/-<imm12>}]
// Op<c> <Rt>, [<Rn>], #+/-<imm12>
// Op<c> <Rt>, [<Rn>, #+/-<imm12>]
// +--------+------+--+--+--+--+--+--------+--------+------------------------+
// |31302928|272625|24|23|22|21|20|19181716|15141312|1110 9 8 7 6 5 4 3 2 1 0|
// +--------+------+--+--+--+--+--+--------+--------+------------------------+
// |  conds |      | P| U|  | w|  |   Rn   |   Rt   |        imm12           |
// +--------+------+--+--+--+--+--+--------+--------+------------------------+
// wback = (P=0 || W==1)
//
// if P=0 and W=1, should not parse as this instruction.
// if Rn=Sp && P=1 && U=0 && W=1 && imm12=4, then PUSH
// if wback && (Rn=15 or Rn=Rt) then unpredictable.
// NaCl disallows writing to PC.
//
// Note: We NaCl disallow Rt=PC for stores (not just loads), even
// though it isn't a requirement of the corresponding baseline
// classees. This is done so that StrImmediate (in the actual class
// decoders) behave the same as instances of this. This simplifies
// what we need to model in actual classes.
class LoadStore2RegisterImm12OpTester : public CondDecoderTester {
 public:
  explicit LoadStore2RegisterImm12OpTester(
      const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::LoadStore2RegisterImm12Op expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore2RegisterImm12OpTester);
};

// Defines a LoadStore2RegisterImm12OpTester with the added constraint
// that it doesn't parse when Rn=15.
class LoadStore2RegisterImm12OpTesterNotRnIsPc
    : public LoadStore2RegisterImm12OpTester {
 public:
  explicit LoadStore2RegisterImm12OpTesterNotRnIsPc(
      const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore2RegisterImm12OpTesterNotRnIsPc);
};

// Tests decoder class LoadStoreRegisterList.
// Models a load/store of  multiple registers into/out of memory.
// Op<c> <Rn>{!}, <registers>
// +--------+------------+--+--+--------+--------------------------------+
// |31302928|272625242322|21|20|19181716|151413121110 9 8 7 6 5 4 3 2 1 0|
// +--------+------------+--+--+--------+--------------------------------+
// |  cond  |            | W|  |   Rn   |         register_list          |
// +--------+------------+--+--+--------+--------------------------------+
// if n=15 || BitCount(registers) < 1 then UNPREDICTABLE.
class LoadStoreRegisterListTester : public CondDecoderTester {
 public:
  explicit LoadStoreRegisterListTester(const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::LoadStoreRegisterList expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStoreRegisterListTester);
};

// Tests decoder class LoadStoreVectorOp
// Models a vector load/store of vector registers into/out of memory.
// Op{mode}<c><q> {,<size>} <Rn>!, <register list>
// Op<c> <Dd>, [<Rn>{, #+/-<imm8>}]
// Op<c> <Sd>, [<Rn>{, #+/-<imm8>}]
// +--------+------+--+--+--+--+--+--------+--------+--------+----------------+
// |31302928|272625|24|23|22|21|20|19181716|15141312|1110 9 8| 7 6 5 4 3 2 1 0|
// +--------+------+--+--+--+--+--+--------+--------+--------+----------------+
// |  cond  |      | P| U| D| W|  |   Rn   |   Vd   | coproc |      imm8      |
// +--------+------+--+--+--+--+--+--------+--------+--------+----------------+
// coproc=1010 implies register size is 32-bit.
// coproc=1011 implies register size is 64-bit.
// <Rn> Is the base register defining the memory to use.
// <Vd>:D defines the first D (vector) register in this operation if
//     coproc=1010. coproc=1010 implies the register size is 64-bit.
// D:<Vd> defines the first D (vector) register in this operation if
//    coproc=1011. coproc=1011 implies the register size is 32-bit
//
// NaCl Constraints:
//    Rn!=Pc.
class LoadStoreVectorOpTester : public CondVfpOpTester {
 public:
  explicit LoadStoreVectorOpTester(const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::LoadStoreVectorOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStoreVectorOpTester);
};

// Tests decoder class LoadStoreVectorRegisterList.
// Models a vector load/store of multiple vector registers into/out of memory.
// Op{mode}<c><q> {,<size>} <Rn>!, <register list>
//
// See base class LoadStoreVectorOpTester for layout of this instruction.
//
// <mode> in { IA , DB } where IA implies P=0 and U=1,
//                       and DB implies P=1 and U=0
// <size> in { 32 , 64 } defines the number of registers in the register list.
// <Rn> Is the base register defining the memory to use.
// <register list> is a list of consecutively numbered registers defining
//        the vector registers to use (the list is defined by <Vd>, D,
//        and <imm8>).
// <Vd>:D defines the first register in the register list.
// <imm8> defines how many registers are to be updated.
//
// Constraints:
//    P=U && W=1 then undefined.
//    if Rn=15  && (W=1 || CurrentInstSet() != ARM) then UNPREDICTABLE.
//    if imm8=0 || (Vd+imm8) > 32 then UNPREDICTABLE.
//    if coproc=1011 and imm8 > 16 then UNPREDICTABLE.
// NaCl Constraints:
//    Rn!=Pc.
// Note: Legal combinations of PUW: { 010 , 011, 101 }
class LoadStoreVectorRegisterListTester : public LoadStoreVectorOpTester {
 public:
  explicit LoadStoreVectorRegisterListTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::LoadStoreVectorRegisterList expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStoreVectorRegisterListTester);
};

// Implements a decoder tester for decoder LoadStoreVectorRegisterList
// where register Rn is not SP.
class LoadStoreVectorRegisterListTesterNotRnIsSp
    : public LoadStoreVectorRegisterListTester {
 public:
  explicit LoadStoreVectorRegisterListTesterNotRnIsSp(
      const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStoreVectorRegisterListTesterNotRnIsSp);
};

// Implements a decoder tester for StoreVectorRegisterList, which adds
// the following constraint:
//
// Adds NaCl Constraint:
//    Rn != PC  (i.e. don't change code space).
class StoreVectorRegisterListTester : public LoadStoreVectorRegisterListTester {
 public:
  explicit StoreVectorRegisterListTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreVectorRegisterListTester);
};

// Implements a decoder tester for decoder StoreVectorRegisterList
// where register Rn is not SP.
class StoreVectorRegisterListTesterNotRnIsSp
    : public StoreVectorRegisterListTester {
 public:
  explicit StoreVectorRegisterListTesterNotRnIsSp(
      const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreVectorRegisterListTesterNotRnIsSp);
};


// Implements a decoder tester for StoreVectorRegister, which adds the
// following constraint:
//
// Adds NaCl Constraint:
//    Rn != PC  (i.e. don't change code space).
class StoreVectorRegisterTester : public LoadStoreVectorOpTester {
 public:
  explicit StoreVectorRegisterTester(const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreVectorRegisterTester);
};

// Models a 3-register load/store operation of the forms:
// Op<c> <Rt>, [<Rn>, +/-<Rm>]{!}
// Op<c> <Rt>, [<Rn>], +/-<Rm>
// +--------+------+--+--+--+--+--+--------+--------+----------------+--------+
// |31302918|272625|24|23|22|21|20|19181716|15141312|1110 9 8 7 6 5 4| 3 2 1 0|
// +--------+------+--+--+--+--+--+--------+--------+----------------+--------+
// |  cond  |      | P| U|  | W|  |   Rn   |   Rt   |                |   Rm   |
// +--------+------+--+--+--+--+--+--------+--------+----------------+--------+
// wback = (P=0 || W=1)
//
// If P=0 and W=1, should not parse as this instruction.
// If Rt=15 || Rm=15 then unpredictable.
// If wback && (Rn=15 or Rn=Rt) then unpredictable.
// if ArchVersion() < 6 && wback && Rm=Rn then unpredictable.
// NaCl Disallows writing to PC.
class LoadStore3RegisterOpTester : public CondDecoderTester {
 public:
  explicit LoadStore3RegisterOpTester(
      const NamedClassDecoder& decoder);
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::LoadStore3RegisterOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore3RegisterOpTester);
};

// Models a 3-register load/store operation where the source/target is double
// wide (i.e. Rt and Rt2).
class LoadStore3RegisterDoubleOpTester : public LoadStore3RegisterOpTester {
 public:
  explicit LoadStore3RegisterDoubleOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::LoadStore3RegisterDoubleOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore3RegisterDoubleOpTester);
};

// Models a 2-register store operation with a register to hold the
// status of the update.
// Op<c><q> <Rd>, <Rt>, [<Rn>]
// +--------+----------------+--------+--------+-----------------+--------+
// |31302928|2726252423222120|19181716|15141312|1110 9 8 7 6 5 4 | 3 2 1 0|
// +--------+----------------+--------+--------+-----------------+--------+
// |  cond  |                |   Rn   |   Rd   |                 |   Rt   |
// +--------+----------------+--------+--------+-----------------+--------+
// Definitions:
//    Rd - The destination register for the returned status value.
//    Rt - The source register
//    Rn - The base register
//
// If Rd, Rt, or Rn is R15, then unpredictable.
// If Rd=Rn || Rd==Rt, then unpredictable.
// NaCl disallows writing to PC to cause a jump.
class StoreExclusive3RegisterOpTester : public CondDecoderTester {
 public:
  explicit StoreExclusive3RegisterOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::StoreExclusive3RegisterOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreExclusive3RegisterOpTester);
};

// Models a 2-register store (exclusive) operation with a register to hold the
// status of the update, and the source is double wide (i.e. Rt and Rt2).
class StoreExclusive3RegisterDoubleOpTester
    : public  StoreExclusive3RegisterOpTester {
 public:
  explicit StoreExclusive3RegisterDoubleOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::StoreExclusive3RegisterDoubleOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreExclusive3RegisterDoubleOpTester);
};

// Models a 3-register with (shifted) immediate 5 load/store operation of
// the forms:
// Op<c> <Rt>, [<Rn>, +/-<Rm> {, <shift>}]{!}
// Op<c> <Rt>, [<Rn>], +-<Rm> {, <shift>}
// +------+------+--+--+--+--+--+--------+--------+----------+----+--+---------+
// |31..28|272625|24|23|22|21|20|19181716|15141312|1110 9 8 7| 6 5| 4| 3 2 1 0 |
// +------+------+--+--+--+--+--+--------+--------+----------+----+--+---------+
// | cond |      | P| U|  | W|  |   Rm   |   Rt   |   imm5   |type|  |    Rm   |
// +------+------+--+--+--+--+--+--------+--------+----------+----+--+---------+
// wback = (P=0 || W=1)
//
// If P=0 and W=1, should not parse as this instruction.
// If Rm=15 then unpredicatble.
// If wback && (Rn=15 or Rn=Rt) then unpredictable.
// if ArchVersion() < 6 && wback && Rm=Rn then unpredictable.
// NaCl Disallows writing to PC.
//
// Note: We NaCl disallow Rt=PC for stores (not just loads), even
// though it isn't a requirement of the corresponding baseline
// classes. This is done so that StrRegister (in the actual class
// decoders) behave the same as this. This simplifies what we need to
// model in actual classes.
class LoadStore3RegisterImm5OpTester : public CondDecoderTester {
 public:
  explicit LoadStore3RegisterImm5OpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::LoadStore3RegisterImm5Op expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore3RegisterImm5OpTester);
};

// Implements a decoder tester for decoder Unary2RegisterImmedShiftedOp.
// Op(S)<c> <Rd>, <Rm> {,<shift>}
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7| 6 5| 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// |  cond  |              | S|        |   Rd   |   imm5   |type|  |   Rm   |
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// Definitions:
//    Rd - The destination register.
//    Rm - The source operand that is (optionally) shifted.
//    shift = DecodeImmShift(type, imm5) is the amount to shift.
class Unary2RegisterImmedShiftedOpTester : public CondDecoderTester {
 public:
  explicit Unary2RegisterImmedShiftedOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Unary2RegisterImmedShiftedOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterImmedShiftedOpTester);
};

// Implements a decoder tester for decoder Unary2RegisterSatImmedShiftedOp.
// Op(S)<c> <Rd>, #<imm>, <Rn> {,<shift>}
// +--------+--------------+----------+--------+----------+----+--+--------+
// |31302928|27262524232221|2019181716|15141312|1110 9 8 7| 6 5| 4| 3 2 1 0|
// +--------+--------------+----------+--------+----------+----+--+--------+
// |  cond  |              | sat_immed|   Rd   |   imm5   |type|  |   Rn   |
// +--------+--------------+----------+--------+----------+----+--+--------+
// Definitions:
//    Rd - The destination register.
//    Rn - The register that contains the value to be saturated.
//    sat_immed+1 - The bit position for saturation, in the range 1 to 32.
//    shift - DecodeImmShift(type, imm5) is the amount to shift.
//
// If Rd or Rn is R15, the instruction is unpredictable.
//
// NaCl disallows writing to PC to cause a jump.
class Unary2RegisterSatImmedShiftedOpTester
    : public CondDecoderTester {
 public:
  explicit Unary2RegisterSatImmedShiftedOpTester(
      const NamedClassDecoder& decoder)
      : CondDecoderTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Unary2RegisterSatImmedShiftedOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterSatImmedShiftedOpTester);
};

class Unary2RegisterSatImmedShiftedOpTesterRegsNotPc
    : public Unary2RegisterSatImmedShiftedOpTester {
 public:
  explicit Unary2RegisterSatImmedShiftedOpTesterRegsNotPc(
      const NamedClassDecoder& decoder)
      : Unary2RegisterSatImmedShiftedOpTester(decoder) {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterSatImmedShiftedOpTesterRegsNotPc);
};

// Implements a decoder tester for decoder Unary3RegisterShiftedOp.
// Op(S)<c> <Rd>, <Rm>,  <type> <Rs>
// Definitions:
//    Rd - The destination register.
//    Rm - The register that is shifted and used as the operand.
//    Rs - The regsiter whose bottom byte contains the amount to shift by.
//    type - The type of shift to apply (not modeled).
//    S - Defines if the flags regsiter is updated.
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8| 7| 6 5| 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// |  cond  |              | S|        |   Rd   |   Rs   |  |type|  |   Rm   |
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
class Unary3RegisterShiftedOpTester : public CondDecoderTester {
 public:
  explicit Unary3RegisterShiftedOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Unary3RegisterShiftedOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary3RegisterShiftedOpTester);
};

// Implements a decoder tester for decoder Unary3RegisterShiftedOp with
// a constraint that if Rd, Rs, or Rm is R15, the instruction is unpredictable.
class Unary3RegisterShiftedOpTesterRegsNotPc
    : public Unary3RegisterShiftedOpTester {
 public:
  explicit Unary3RegisterShiftedOpTesterRegsNotPc(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary3RegisterShiftedOpTesterRegsNotPc);
};

// Implements a decoder tester for decoder Binary4RegisterShiftedOp.
// Op(S)<c> <Rd>, <Rn>, <Rm>,  <type> <Rs>
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8| 7| 6 5| 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// |  cond  |              | S|   Rn   |   Rd   |   Rs   |  |type|  |   Rm   |
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// Definitions:
//    Rd - The destination register.
//    Rn - The first operand register.
//    Rm - The register that is shifted and used as the second operand.
//    Rs - The regsiter whose bottom byte contains the amount to shift by.
//    type - The type of shift to apply (not modeled).
//    S - Defines if the flags regsiter is updated.
class Binary4RegisterShiftedOpTester : public CondDecoderTester {
 public:
  explicit Binary4RegisterShiftedOpTester(
      const NamedClassDecoder &decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Binary4RegisterShiftedOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary4RegisterShiftedOpTester);
};

// Implements a decoder tester for decoder Binary4RegisterShiftedOp
// with the constraint that if Rn, Rd, Rs, or Rm is R15, the instruction
// is unpredictable.
class Binary4RegisterShiftedOpTesterRegsNotPc
    : public Binary4RegisterShiftedOpTester {
 public:
  explicit Binary4RegisterShiftedOpTesterRegsNotPc(
      const NamedClassDecoder &decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary4RegisterShiftedOpTesterRegsNotPc);
};

// Implements a decoder tester for decoder Binary2RegisterImmedShiftedTest.
// Op(S)<c. Rn, Rm {,<shift>}
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7| 6 5| 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// |  cond  |              | S|   Rn   |        |   imm5   |type|  |   Rm   |
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// Definitions:
//    Rn - The first operand register.
//    Rm - The second operand that is (optionally) shifted.
//    shift = DecodeImmShift(type, imm5) is the amount to shift.
class Binary2RegisterImmedShiftedTestTester : public CondDecoderTester {
 public:
  explicit Binary2RegisterImmedShiftedTestTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Binary2RegisterImmedShiftedTest expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary2RegisterImmedShiftedTestTester);
};

// Implements a decoder tester for decoder Binary3RegisterShiftedTest.
// OpS<c> <Rn>, <Rm>, <type> <Rs>
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8| 7| 6 5| 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// |  cond  |              | S|   Rn   |        |   Rs   |  |type|  |   Rm   |
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// Definitions:
//    Rn - The first operand register.
//    Rm - The register that is shifted and used as the second operand.
//    Rs - The regsiter whose bottom byte contains the amount to shift by.
//    type - The type of shift to apply (not modeled).
//    S - Defines if the flags regsiter is updated.
class Binary3RegisterShiftedTestTester : public CondDecoderTester {
 public:
  explicit Binary3RegisterShiftedTestTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Binary3RegisterShiftedTest expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterShiftedTestTester);
};

// Implements a decoder tester for decoder Binary3RegisterShiftedTest, and
// should not parse when Rn, Rm, or Rs is 15.
class Binary3RegisterShiftedTestTesterRegsNotPc
    : public Binary3RegisterShiftedTestTester {
 public:
  explicit Binary3RegisterShiftedTestTesterRegsNotPc(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterShiftedTestTesterRegsNotPc);
};

// Implements a decoder tester for VectorUnary2RegisterOpBase
// Op<c> Rd, Rm, ...
// +--------+----------+--+----+--------+--------+------------+--+--+--------+
// |31302928|2726252423|22|2120|19181716|15141312|1110 9 8 7 6| 5| 4| 3 2 1 0|
// +--------+----------+--+----+--------+--------+------------+--+--+--------+
// |  cond  |          | D|    |        |   Vd   |            | M|  |   Vm   |
// +--------+----------+--+----+--------+--------+----+-------+--+--+--------+
// Rd - The destination register.
// Rm - The operand.
//
// d = D:Vd, m = M:Vm
// cond=1111
//
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled, other than thier index.
class VectorUnary2RegisterOpBaseTester : public UncondDecoderTester {
 public:
  explicit VectorUnary2RegisterOpBaseTester(const NamedClassDecoder& decoder)
      : UncondDecoderTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::VectorUnary2RegisterOpBase expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorUnary2RegisterOpBaseTester);
};

// Implements a decoder tester for Vector2RegisterMiscellaneous
// Vector 2 register SIMD miscellaneous operation.
//
// +--------+------+--+----+----+----+--------+--+--+--+----+--+--+--+--------+
// |31302928|27..23|22|2120|1918|1716|15141312|11|10| 9| 8 7| 6| 5| 4| 3 2 1 0|
// +--------+------+--+----+----+----+--------+--+--+--+----+--+--+--+--------+
// |  cond  |      | D|    |size|    |   Vd   |  | F|  | op | Q| M|  |   Vm   |
// +--------+- ----+--+----+----+----+--------+--+--+--+----+--+--+--+--------+
// Rd - The destination register.
// Rm - The operand.
//
// d = D:Vd, m = M:Vm
class Vector2RegisterMiscellaneousTester
    : public VectorUnary2RegisterOpBaseTester {
 public:
  explicit Vector2RegisterMiscellaneousTester(const NamedClassDecoder& decoder)
      : VectorUnary2RegisterOpBaseTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Vector2RegisterMiscellaneous expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector2RegisterMiscellaneousTester);
};

// Implements a decoder tester for Vector2RegisterMiscellaneous_I16_32_64N.
// Vector 2 register SIMD miscellaneous operation that narrows 16,
// 32 or 64-bit signed/unsigned values.
//
// +--------+------+--+----+----+----+--------+--------+----+--+--+--------+
// |31302928|27..23|22|2120|1918|1716|15141312|1110 9 8| 7 6| 5| 4| 3 2 1 0|
// +--------+------+--+----+----+----+--------+--------+----+--+--+--------+
// |  cond  |      | D|    |size|    |   Vd   |        | op | M|  |   Vm   |
// +--------+- ----+--+----+----+----+--------+--------+----+--+--+--------+
// Rd - The destination register.
// Rm - The operand.
//
// d = D:Vd, m = M:Vm
// safety := op=00 => DECODER_ERROR &
//           size=11 | Vm(0)=1 => UNDEFINED;
class Vector2RegisterMiscellaneous_I16_32_64NTester
    : public VectorUnary2RegisterOpBaseTester {
 public:
  explicit Vector2RegisterMiscellaneous_I16_32_64NTester(
      const NamedClassDecoder& decoder)
      : VectorUnary2RegisterOpBaseTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Vector2RegisterMiscellaneous_I16_32_64N expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Vector2RegisterMiscellaneous_I16_32_64NTester);
};

// Implements a decoder tester for Vector2RegisterMiscellaneous_CVT_H2S.
// Vector 2 register SIMD miscellaneous operation that coverts between
// half-precision and single-precision.
//
// +--------+------+--+----+----+----+--------+------+--+----+--+--+--------+
// |31302928|27..23|22|2120|1918|1716|15141312|1110 9| 8| 7 6| 5| 4| 3 2 1 0|
// +--------+------+--+----+----+----+--------+------+--+----+--+--+--------+
// |  cond  |      | D|    |size|    |   Vd   |      |op|    | M|  |   Vm   |
// +--------+- ----+--+----+----+----+--------+------+--+----+--+--+--------+
// Rd - The destination register.
// Rm - The operand.
//
// d = D:Vd, m = M:Vm
// half_to_single := op=1;
// safety := size=~01 => UNDEFINED &
//           half_to_single & Vd(0)=1 => UNDEFINED &
//           not half_to_single & Vm(0)=1 => UNDEFINED;
class Vector2RegisterMiscellaneous_CVT_H2STester
    : public VectorUnary2RegisterOpBaseTester {
 public:
  explicit Vector2RegisterMiscellaneous_CVT_H2STester(
      const NamedClassDecoder& decoder)
      : VectorUnary2RegisterOpBaseTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Vector2RegisterMiscellaneous_CVT_H2S expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector2RegisterMiscellaneous_CVT_H2STester);
};

// Implements a decoder tester for VectorUnary2RegisterDup
// Vector duplication (scalar)
// Op<c> Rd, Rm[x]
// +--------+----------+--+----+--------+--------+----------+--+--+--+--------+
// |31302928|2726252423|22|2120|19181716|15141312|1110 9 8 7| 6| 5| 4| 3 2 1 0|
// +--------+----------+--+----+--------+--------+----------+--+--+--+--------+
// |  cond  |          | D|    |  imm4  |   Vd   |          | Q| M|  |   Vm   |
// +--------+----------+--+----+--------+--------+----+-----+--+--+--+--------+
// Rd - The destination register.
// Rm - The scalar operand.
// Q=1 implies quadword operation. Otherwise doubleword.
//
// d = D:Vd, m = M:Vm
// cond=1111
//
// If Q=1 && Vd<0>=1 then UNDEFINED.
// if imm4='0000' then UNDEFINED.
// If imm4 not in {'xxx1', 'xx10', 'x100' } then UNDEFINED.
//
// Note: Until proven otherwise, we are going to assume that only
// { '0001', '0010', '0100'} is allowed for imm4. Since the three bit
// positions correspond to which size we should use in the operation.
//
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled, other than thier index.
class VectorUnary2RegisterDupTester : public VectorUnary2RegisterOpBaseTester {
 public:
  explicit VectorUnary2RegisterDupTester(const NamedClassDecoder& decoder)
      : VectorUnary2RegisterOpBaseTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::VectorUnary2RegisterDup expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorUnary2RegisterDupTester);
};


// Implements a decoder tester for VectorBinary3RegisterOpBase
// Op<c> Rd, Rn, Rm,...
// +--------+----------+--+----+--------+--------+--------+--+--+--+--+--------+
// |31302928|2726252423|22|2120|19181716|15141312|1110 9 8| 7| 6| 5| 4| 3 2 1 0|
// +--------+----------+--+----+--------+--------+--------+--+--+--+--+--------+
// |  cond  |          | D|    |   Vn   |   Vd   |        | N|  | M|  |   Vm   |
// +--------+----------+--+----+--------+--------+----+---+--+--+--+--+--------+
// Rd - The destination register.
// Rn - The first operand.
// Rm - The second operand.
//
// d = D:Vd, n = N:Vn, m = M:Vm
// cond=1111
//
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled, other than thier index.
class VectorBinary3RegisterOpBaseTester : public UncondDecoderTester {
 public:
  explicit VectorBinary3RegisterOpBaseTester(const NamedClassDecoder& decoder)
      : UncondDecoderTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::VectorBinary3RegisterOpBase expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterOpBaseTester);
};

// Implements a decoder tester for VectorBinary2RegisterShiftAmount.
// Vector binary operator, 2 registers and a shift amount
// op<c>.<type<size>> Rd, Rm, #<imm>
// +--------+--+--+--+--+------------+--------+--+----+--+--+--+--+--+------+
// |31..28|27..|24|23|22|212019181716|15141312|11|10 9| 8| 7| 6| 5| 4| 3.. 0|
// +------+----+--+--+--+------------+--------+--+----+--+--+--+--+--+------+
// | cond |    | U|  | D|     imm6   |   Vd   |  |    |op| L| Q| M|  |  Vm  |
// +--------+--+--+--+--+------------+--------+--+----+--+--+--+--+--+------+
// Rd - The destination register.
// Rm - The source register.
//
// d = D:Vd, m = M:Vm
//
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled, other than their index.
class VectorBinary2RegisterShiftAmountTester
    : public VectorBinary3RegisterOpBaseTester {
 public:
  explicit VectorBinary2RegisterShiftAmountTester(
      const NamedClassDecoder& decoder)
      : VectorBinary3RegisterOpBaseTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::VectorBinary2RegisterShiftAmount expected_decoder_;
};

// Implements a decoder tester for VectorBinary3RegisterSameLength.
// Op<c> Rd, Rn, Rm,...
// +--------+-------+--+----+--------+--------+----+--+--+--+--+--+--+--------+
// |31302928|27...23|22|2120|19181716|15141312|1110| 9| 8| 7| 6| 5| 4| 3 2 1 0|
// +--------+-------+--+----+--------+--------+----+--+--+--+--+--+--+--------+
// |  cond  |       | D|size|   Vn   |   Vd   |    |op|  | N| Q| M|  |   Vm   |
// +--------+-------+--+----+--------+--------+----+--+--+--+--+--+--+--------+
// Rd - The destination register.
// Rn - The first operand.
// Rm - The second operand.
//
// d = D:Vd, n = N:Vn, m = M:Vm
class VectorBinary3RegisterSameLengthTester
    : public VectorBinary3RegisterOpBaseTester {
 public:
  explicit VectorBinary3RegisterSameLengthTester(
      const NamedClassDecoder& decoder)
      : VectorBinary3RegisterOpBaseTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::VectorBinary3RegisterSameLength expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterSameLengthTester);
};

// Implements a decoder tester for VectorBinary3RegisterDifferntLength.
// Vector binary operator, 3 registers different length (One Q and two D, or
// two D and one Q). Which register is Q (vs D) is instruction dependent.
// Op<c> Rd, Rn, Rm,...
// +--------+--+--+--+--+----+--------+--------+----+--+--+--+--+--+--+--------+
// |31..28|27..|24|23|22|2120|19181716|15141312|1110| 9| 8| 7| 6| 5| 4| 3 2 1 0|
// +------+----+--+--+--+----+--------+--------+----+--+--+--+--+--+--+--------+
// | cond |    | U|  | D|size|   Vn   |   Vd   |    |  |op| N| Q| M|  |   Vm   |
// +--------+--+--+--+--+----+--------+--------+----+--+--+--+--+--+--+--------+
// Rd - The destination register.
// Rn - The first operand.
// Rm - The second operand.
//
// d = D:Vd, n = N:Vn, m = M:Vm
class VectorBinary3RegisterDifferentLengthTester
    : public VectorBinary3RegisterOpBaseTester {
 public:
  explicit VectorBinary3RegisterDifferentLengthTester(
      const NamedClassDecoder& decoder)
      : VectorBinary3RegisterOpBaseTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::VectorBinary3RegisterDifferentLength expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterDifferentLengthTester);
};

// Implements a decoder tester for VectorBinary2RegisterScalar.
// Vector binary operator, 2 registers and a scalar (which is encoded in M:Vm).
// op<c>.<dt> Rd, Rn, <Rm[x]>
// +--------+--+--+--+--+----+--------+--------+--+--+--+--+--+--+--+--+------+
// |31..28|27..|24|23|22|2120|19181716|15141312|11|10| 9| 8| 7| 6| 5| 4| 3.. 0|
// +------+----+--+--+--+----+--------+--------+--+--+--+--+--+--+--+--+------+
// | cond |    | Q|  | D|size|   Vn   |   Vd   |  |op|  | F| N|  | M|  |  Vm  |
// +--------+--+--+--+--+----+--------+--------+--+--+--+--+--+--+--+--+------+
// Rd - The destination register.
// Rn - The first operand.
// M:Vm - The incoding of the scalar.
//
// d = D:Vd, n = N:Vn
//
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled, other than their index.
class VectorBinary2RegisterScalarTester
    : public VectorBinary3RegisterOpBaseTester {
 public:
  explicit VectorBinary2RegisterScalarTester(const NamedClassDecoder& decoder)
      : VectorBinary3RegisterOpBaseTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::VectorBinary2RegisterScalar expected_decoder_;
};

// Implements a decoder tester for VectorBinary3RegisterImmOp
// Op<c> Rd, Rn, Rm, #<imm>
// +--------+----------+--+----+--------+--------+--------+--+--+--+--+--------+
// |31302928|2726252423|22|2120|19181716|15141312|1110 9 8| 7| 6| 5| 4| 3 2 1 0|
// +--------+----------+--+----+--------+--------+--------+--+--+--+--+--------+
// |  cond  |          | D|    |   Vn   |   Vd   |  imm4  | N| Q| M|  |   Vm   |
// +--------+----------+--+----+--------+--------+----+---+--+--+--+--+--------+
// Rd - The destination register.
// Rn - The first operand.
// Rm - The second operand.
//
// d = D:Vd, n = N:Vn, m = M:Vm
// cond=1111
//
// Q=1 implies quadword operation. Otherwise doubleword.
//
// if Q=1 && (Vd<0>=1 || Vn<0>==1 || Vm<0>==1) then UNDEFINED;
// if Q=0 && imm4<3>==1 then UNDEFINED:
class VectorBinary3RegisterImmOpTester
    : public VectorBinary3RegisterOpBaseTester {
 public:
  explicit VectorBinary3RegisterImmOpTester(const NamedClassDecoder& decoder)
      : VectorBinary3RegisterOpBaseTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::VectorBinary3RegisterImmOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterImmOpTester);
};

// Implements a decoder tester for Vector1RegisterImmediate.
// Vector 1 register immediate SIMD instruction.
// +--------------+--+--+--+------+------+--------+--------+--+--+--+--+-----+
// |31302928272625|24|23|22|212019|181716|15141312|1110 9 8| 7| 6| 5| 4| 3..0|
// +--------------+--+--+--+------+------+--------+--------+--+--+--+--+-----+
// |              | i|  | D|      | imm3 |   Vd   |  cmode |  | Q|op|  | imm4|
// +--------------+--+--+--+------+------+--------+----+---+--+--+--+--+-----+
// d := D:Vd;
// imm64 := AdvSIMDExpandImm(op, cmode, i:imm3:imm4);
class Vector1RegisterImmediateTester : public UncondDecoderTester {
 public:
  explicit Vector1RegisterImmediateTester(const NamedClassDecoder& decoder)
      : UncondDecoderTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::Vector1RegisterImmediate expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector1RegisterImmediateTester);
};

// Implements a decoder tester for VectorBinary3RegisterLookupOp
// Op<c> <Dd>, <list>, <Dm>
// +--------+----------+--+----+--------+--------+----+----+--+--+--+--+--------
// |31302928|2726252423|22|2120|19181716|15141312|1110| 9 8| 7| 6| 5| 4| 3 2 1 0
// +--------+----------+--+----+--------+--------+----+----+--+--+--+--+--------
// |  cond  |          | D|    |   Vn   |   Vd   |    | len| N|op| M|  |   Vm
// +--------+----------+--+----+--------+--------+----+----+--+--+--+--+--------
// <Dd> - The destination register.
// <list> - The list of up to 4 consecutive registers starting at <Dn>
// len - The number of registers (minus 1).
// op - defines additional info about which operation (lookup vs zero) to apply.
// <Dm> - The index register.
//
// d = D:Vd, n = N:Vn, m = M:Vm
// length = len+1
// cond=1111
//
// if n+length > 32 then UNPREDICTABLE
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled.
class VectorBinary3RegisterLookupOpTester
    : public VectorBinary3RegisterOpBaseTester {
 public:
  explicit VectorBinary3RegisterLookupOpTester(const NamedClassDecoder& decoder)
      : VectorBinary3RegisterOpBaseTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::VectorBinary3RegisterLookupOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterLookupOpTester);
};

// Implements a decoder tester for VfpUsesRegOp
// Op<c> ..., <Rt>, ...
// +--------+------------------------+--------+--------+---------------+
// |31302928|272625242322212019181716|15141312|1120 9 8|7 6 5 4 3 2 1 0|
// +--------+------------------------+--------+--------+---------------+
// |  cond  |                        |   Rt   | coproc |               |
// +--------+------------------------+--------+--------+---------------+
//
// If t=15 then UNPREDICTABLE
class VfpUsesRegOpTester : public CondVfpOpTester {
 public:
  explicit VfpUsesRegOpTester(const NamedClassDecoder& decoder)
      : CondVfpOpTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::VfpUsesRegOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VfpUsesRegOpTester);
};

// Implements a decoder tester for VfpMrsOp.
// Vmrs<c> ..., <Rt>, ...
// +--------+------------------------+--------+--------+---------------+
// |31302928|272625242322212019181716|15141312|1120 9 8|7 6 5 4 3 2 1 0|
// +--------+------------------------+--------+--------+---------------+
// |  cond  |                        |   Rt   | coproc |               |
// +--------+------------------------+--------+--------+---------------+
//
// TODO(jfb) The following restriction is ignored for now:
// if Rt=13 && CurrentInstrSet() != ARM then UNPREDICTABLE.
//
// Note: if Rt=PC, then it doesn't update PC. Rather, it updates the
// conditions flags APSR.{N, Z, C, V} from corresponding conditions
// in FPSCR.
class VfpMrsOpTester : public CondVfpOpTester {
 public:
  explicit VfpMrsOpTester(const NamedClassDecoder& decoder)
      : CondVfpOpTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::VfpMrsOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VfpMrsOpTester);
};

// Implements a decoder tester for MoveVfpRegisterOp
// Op<c> <Sn>, <Rt>
// Op<c> <Rt>, <Sn>
// +--------+--------------+--+--------+--------+--------+--+--------------+
// |31302928|27262524232221|20|19181726|15141312|1110 9 8| 7| 6 5 4 3 2 1 0|
// +--------+--------------+--+--------+--------+--------+--+--------------+
// |  cond  |              |op|   Vn   |   Rt   | coproc | N|              |
// +--------+--------------+--+--------+--------+--------+--+--------------+
// S[Vn:N] = The vfp register to use.
// Rt = The core register to use.
// op=1 => Move S[Vn:N] to Rt.
// op=0 => Move Rt to S[Vn:N]
//
// If t=15 then UNPREDICTABLE
//
// Note: We don't model Register S[Vn:N] since it can't effect NaCl validation.
class MoveVfpRegisterOpTester : public CondVfpOpTester {
 public:
  explicit MoveVfpRegisterOpTester(const NamedClassDecoder& decoder)
      : CondVfpOpTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::MoveVfpRegisterOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MoveVfpRegisterOpTester);
};

// Implements a decoder tester for MoveVfpRegisterOpWithTypeSel.
// Added constraint to MoveVfpRegisterOp:
// When bits(23:21):bits(6:5) = '10x00' or 'x0x10', the instruction is
// undefined. That is, these bits are used to define 8, 16, and 32 bit selectors
// within Vn.
class MoveVfpRegisterOpWithTypeSelTester : public MoveVfpRegisterOpTester {
 public:
  explicit MoveVfpRegisterOpWithTypeSelTester(
      const NamedClassDecoder& decoder)
      : MoveVfpRegisterOpTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::MoveVfpRegisterOpWithTypeSel expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MoveVfpRegisterOpWithTypeSelTester);
};


// Implements a decoder tester for MoveDoubleVfpRegisterOp.
// Added constraint to MoveVfpRegisterOp:
// if t == 15 || t2 == 15 then UNPREDICTABLE;
// if IsSinglePrecision() && m == 31 then UNPREDICTABLE;
// if to_arm_registers && t == t2 then UNPREDICTABLE;
class MoveDoubleVfpRegisterOpTester : public MoveVfpRegisterOpTester {
 public:
  explicit MoveDoubleVfpRegisterOpTester(
      const NamedClassDecoder& decoder)
      : MoveVfpRegisterOpTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::MoveDoubleVfpRegisterOp expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MoveDoubleVfpRegisterOpTester);
};

// Implements a decoder tester for DuplicateToAdvSIMDRegisters.
// Op<c>.size <Dd>, <Rt>
// Op<c>.size <Qd>, <Rt>
// +--------+----------+--+--+--+--------+--------+--------+--+--+--+----------+
// |31302928|2726252423|22|21|20|19181716|15141312|1110 9 8| 7| 6| 5| 4 3 2 1 0|
// +--------+----------+--+--+--+--------+--------+--------+--+--+--+----------+
// |  cond  |          | b| Q|  |   Vd   |   Rt   | coproc | D|  | e|          |
// +--------+----------+--+--+--+--------+--------+--------+--+--+--+----------+
// d = D:Vd
// # D registers = (Q=0 ? 1 : 2)
//
// if cond != AL then DEPRECATED
// if Q=1 and Vd<0>=1 then UNDEFINED
// if t=15 then UNPREDICTABLE.
// if b:e=11 then UNDEFINED.
class DuplicateToAdvSIMDRegistersTester : public CondAdvSIMDOpTester {
 public:
  explicit DuplicateToAdvSIMDRegistersTester(
      const NamedClassDecoder& decoder)
      : CondAdvSIMDOpTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::DuplicateToAdvSIMDRegisters expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(DuplicateToAdvSIMDRegistersTester);
};

// Implements a decoder tester for VectorLoadStoreMultiple.
// Models an advanced SIMD vector store multiple elements.
// +------------------+--+----+--------+--------+--------+----+----+--------+
// |313029282726252423|22|2120|19181716|15141312|1110 9 8| 7 6| 5 4| 3 2 1 0|
// +------------------+--+----+--------+--------+--------+----+----+--------+
// |                  | D|    |   Rn   |   Vd   |  type  |size|align|  Rm   |
// +------------------+--+----+--------+--------+--------+----+----+--------+
// alignment := 1 if align=00 else 4 << align;
// ebytes := 1 << size; esize := 8 * ebytes; elements := 8 / ebytes;
// d := D:Vd; n := Rn; m := Rm;
// wback := (m != Pc); register_index := (m != Pc & m != Sp);
// base := n;
// # defs ignores FPRs. It only models GPRs and conditions.
// defs := { base } if wback else {};
// # Note: register_index defines if Rm is used (rather than a small constant).
// small_imm_base_wb := not register_index;
// # uses ignores FPRs. It only models GPRs.
// uses := { m if wback else None , n };
// arch := ASIMD;
class VectorLoadStoreMultipleTester : public UncondDecoderTester {
 public:
  explicit VectorLoadStoreMultipleTester(const NamedClassDecoder& decoder)
      : UncondDecoderTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::VectorLoadStoreMultiple expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadStoreMultipleTester);
};

// Implements a decoder tester for VectorLoadStoreSingle.
// Models an advanced SIMD vector store single elements.
// +------------------+--+----+--------+--------+----+----+-----------+--------+
// |313029282726252423|22|2120|19181716|15141312|1110| 9 8| 7 6 5 4   | 3 2 1 0|
// +------------------+--+----+--------+--------+----+----+-----------+--------+
// |                  | D|    |   Rn   |   Vd   |size|    |index_align|   Rm   |
// +------------------+--+----+--------+--------+----+----+-----------+--------+
//   ebytes := 1 << size; esize := 8 * ebytes;
//   index := index_align(3:1) if size=00 else
//            index_align(3:2) if size=01 else
//            index_align(3)   if size=10 else
//            0;  # error value.
//   inc := 1 if size=00 else
//          (1 if index_align(1)=0 else 2) if size=01 else
//          (1 if index_align(2)=0 else 2) if size=10 else
//          0;  # error value.
//   d := D:Vd; n := Rn; m := Rm;
//   wback := (m != Pc); register_index := (m != Pc & m != Sp);
//   base := n;
//   # defs ignores FPRs. It only models GPRs and conditions.
//   defs := { base } if wback else {};
//   # Note: register_index defines if Rm is used (rather than a small
//   # constant).
//   small_imm_base_wb := not register_index;
//   # uses ignores FPRs. It only models GPRs.
//   uses := { m if wback else None , n };
//   arch := ASIMD;
class VectorLoadStoreSingleTester : public UncondDecoderTester {
 public:
  explicit VectorLoadStoreSingleTester(const NamedClassDecoder& decoder)
      : UncondDecoderTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::VectorLoadStoreSingle expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadStoreSingleTester);
};

// Implements a decoder tester for VectorLoadSingleAllLanes.
// Implements a VLD1, VLD2, VLD3, VLD4 single instruction to all langes.
// +------------------+--+----+--------+--------+--------+----+--+--+--------+
// |313029282726252423|22|2120|19181716|15141312|1110 9 8| 7 6| 5| 4| 3 2 1 0|
// +------------------+--+----+--------+--------+--------+----+--+--+--------+
// |                  | D|    |   Rn   |   Vd   |        |size| T| a|   Rm   |
// +------------------+--+----+--------+--------+--------+----+--+--+--------+
//   ebytes := 1 << size; elements = 8 / ebytes;
//   d := D:Vd; n := Rn; m := Rm;
//   wback := (m != Pc); register_index := (m != Pc & m != Sp);
//   base := n;
//   # defs ignores FPRs. It only models GPRs and conditions.
//   defs := { base } if wback else {};
//   # Note: register_index defines if Rm is used (rather than a small
//   # constant).
//   small_imm_base_wb := not register_index;
//   # uses ignores FPRs. It only models GPRs.
//   uses := { m if wback else None , n };
//   arch := ASIMD;
class VectorLoadSingleAllLanesTester : public UncondDecoderTester {
 public:
  explicit VectorLoadSingleAllLanesTester(const NamedClassDecoder& decoder)
      : UncondDecoderTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::VectorLoadSingleAllLanes expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadSingleAllLanesTester);
};

// Implements a decoder tester for BarrierInst
// Op #<option>
// +--------+------------------------------------------------+--------+
// |31302928|272625242322212019181716151413121110 9 8 7 6 5 4| 3 2 1 0|
// +--------+------------------------------------------------+--------+
// |  cond  |                                                |  option|
// +--------+------------------------------------------------+--------+
// cond=1111
// option in {1111, 1110, 1011, 1010, 0111, 0110, 0011, 0010}
//
// Note: NaCl disallow all other values other than 1111, since is
// IMPLEMENTATION DEFINED whether option values other than 1111 are implemented.
class BarrierInstTester : public UncondDecoderTester {
 public:
  explicit BarrierInstTester(const NamedClassDecoder& decoder)
      : UncondDecoderTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  nacl_arm_dec::BarrierInst expected_decoder_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BarrierInstTester);
};

// Implements a decoder tester for a DataBarrier.
//
// option in {1111, 1110, 1011, 1010, 0111, 0110, 0011, 0010}
class DataBarrierTester : public BarrierInstTester {
 public:
  explicit DataBarrierTester(const NamedClassDecoder& decoder)
      : BarrierInstTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(DataBarrierTester);
};

// Implements a decoder tester for an InstructionBarrier
//
// option = 1111 (All other values are reserved).
class InstructionBarrierTester : public BarrierInstTester {
 public:
  explicit InstructionBarrierTester(const NamedClassDecoder& decoder)
      : BarrierInstTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(InstructionBarrierTester);
};

// Implements a decoder tester for a PermanentlyUndefined.
class PermanentlyUndefinedTester : public CondDecoderTester {
 public:
  explicit PermanentlyUndefinedTester(const NamedClassDecoder& decoder)
      : CondDecoderTester(decoder) {}
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PermanentlyUndefinedTester);
};

}  // namespace nacl_arm_test

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_INST_CLASSES_TESTERS_H_
