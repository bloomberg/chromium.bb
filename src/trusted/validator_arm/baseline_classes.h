/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_BASELINE_CLASSES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_BASELINE_CLASSES_H_

#include "native_client/src/trusted/validator_arm/inst_classes.h"

// TODO(jfb) Remove these forward declatations? They're only needed for
//           friend classes which shouldn't be needed themselves.
namespace nacl_arm_test {
class LoadStore2RegisterImm8OpTester;
class LoadStore2RegisterImm8DoubleOpTester;
class LoadStore2RegisterImm12OpTester;
class LoadStore3RegisterOpTester;
class LoadStore3RegisterDoubleOpTester;
class LoadStore3RegisterImm5OpTester;
class UnsafeUncondDecoderTester;
class UnsafeCondDecoderTester;
class VectorLoadStoreMultipleTester;
class VectorLoadStoreSingleTester;
class VectorLoadSingleAllLanesTester;
}  // namespace nacl_arm_test

/*
 * Models the baseline "instruction classes" that capture what the
 * decoder produces.
 *
 * The baseline model is intentionally very close to the data formats
 * used in "ARM Architecture Reference Manual ARM*v7-A and ARM*v7-R
 * edition, Errata markup". For each data layout, there is a separate
 * class. In addition, the test infrastructure is based on testing if
 * the baseline class decoders do as expected (in terms of the
 * manual).
 *
 * Note: This file is currently under construction. It reflects the
 * current known set of baseline class decoders. More will be added as
 * we continue to test the Arm32 decoder.
 *
 * TODO(karl): Finish updating this file to match what we want for
 *             the arm validator.
 */
namespace nacl_arm_dec {

// Models an unconditional instruction.
// Op<c>
// +--------+--------------------------------------------------------+
// |31302918|272625242322212019181716151413121110 9 8 7 6 5 4 3 2 1 0|
// +--------+--------------------------------------------------------+
// |  cond  |                                                        |
// +--------+--------------------------------------------------------+
//
// if cond!=1111 then UNDEFINED.
class UncondDecoder : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  UncondDecoder() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(UncondDecoder);
};


// Models an unconditional instruction that is always unsafe (i.e. one of:
// Forbidden, Undefined, Deprecated, and Unpredictable).
class UnsafeUncondDecoder : public UncondDecoder {
 public:
  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return safety_;
  }

 protected:
  explicit UnsafeUncondDecoder(SafetyLevel safety)
      : UncondDecoder(), safety_(safety) {}

 private:
  SafetyLevel safety_;  // The unsafe value to return.
  NACL_DISALLOW_COPY_AND_ASSIGN(UnsafeUncondDecoder);
  friend class nacl_arm_test::UnsafeUncondDecoderTester;
};

// Models an unconditional forbidden unsafe decoder.
class ForbiddenUncondDecoder : public UnsafeUncondDecoder {
 public:
  explicit ForbiddenUncondDecoder() : UnsafeUncondDecoder(FORBIDDEN) {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ForbiddenUncondDecoder);
};

// Models a (conditional) instruction.
// Op<c>
// +--------+--------------------------------------------------------+
// |31302918|272625242322212019181716151413121110 9 8 7 6 5 4 3 2 1 0|
// +--------+--------------------------------------------------------+
// |  cond  |                                                        |
// +--------+--------------------------------------------------------+
class CondDecoder : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  CondDecoder() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(CondDecoder);
};

// Models a (conditional) instruction that is always unsafe (i.e. one of:
// Forbidden, Undefined, Deprecated, and Unpredictable).
class UnsafeCondDecoder : public CondDecoder {
 public:
  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return safety_;
  }

 protected:
  explicit UnsafeCondDecoder(SafetyLevel safety)
      : CondDecoder(), safety_(safety) {}

 private:
  SafetyLevel safety_;  // The unsafe value to return.
  NACL_DISALLOW_COPY_AND_ASSIGN(UnsafeCondDecoder);
  friend class nacl_arm_test::UnsafeCondDecoderTester;
};

// Models a (conditional) forbidden UnsafeCondDecoder.
class ForbiddenCondDecoder : public UnsafeCondDecoder {
 public:
  explicit ForbiddenCondDecoder() : UnsafeCondDecoder(FORBIDDEN) {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ForbiddenCondDecoder);
};

// Models a (conditional) undefined UnsafeCondDecoder.
class UndefinedCondDecoder : public UnsafeCondDecoder {
 public:
  explicit UndefinedCondDecoder() : UnsafeCondDecoder(UNDEFINED) {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(UndefinedCondDecoder);
};

// Models an (unconditional) unpredictable UnsafeUncondDecoder.
class UnpredictableUncondDecoder : public UnsafeUncondDecoder {
 public:
  explicit UnpredictableUncondDecoder() : UnsafeUncondDecoder(UNPREDICTABLE) {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(UnpredictableUncondDecoder);
};

// Implements a VFP operation on instructions of the form;
// +--------+--------------------------------+--------+----------------+
// |31302928|27262524232221201918171615141312|1110 9 8| 7 6 5 4 3 2 1 0|
// +--------+--------------------------------+--------+----------------+
// |  cond  |                                | coproc |                |
// +--------+--------------------------------+--------+----------------+
// A generic VFP instruction that (by default) only affects vector
// register banks. Hence, they do not change general purpose registers.
class CondVfpOp : public CoprocessorOp {
 public:
  // Accessor to non-vector register fields.
  static const ConditionBits28To31Interface cond;

  CondVfpOp() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(CondVfpOp);
};

// Same as CondVfpOp, but with the extra restriction that ARM deprecates the
// conditional execution of any Advanced SIMD instruction encoding that is not
// also available as a VFP instruction encoding.
class CondAdvSIMDOp : public CoprocessorOp {
 public:
  // Accessor to non-vector register fields.
  static const ConditionBits28To31Interface cond;

  CondAdvSIMDOp() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(CondAdvSIMDOp);
};

// Models a move of an immediate 12 value to the corresponding
// bits in the APSR.
// MSR<c> <spec_reg>, #<const>
// +--------+----------------+----+------------+------------------------+
// |31302928|2726252423222120|1918|171615141312|1110 9 8 7 6 5 4 3 2 1 0|
// +--------+----------------+----+------------+------------------------+
// |  cond  |                |mask|            |         imm12          |
// +--------+----------------+----+------------+------------------------+
// Definitions:
//    mask = Defines which parts of the APSR is set. When mask<1>=1,
//           the N, Z, C, V, and Q bits (31:27) are updated. When
//           mask<0>=1, the GE bits (19:16) are updated.
// Note: If mask=3, then N, Z, C, V, Q, and GE bits are updated.
// Note: mask=0 should not parse.
class MoveImmediate12ToApsr : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const Imm12Bits0To11Interface imm12;
  static const Imm2Bits18To19Interface mask;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  MoveImmediate12ToApsr() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

  // Defines when condition flags N, Z, C, V, and Q are updated.
  bool UpdatesConditions(const Instruction i) const {
    return (mask.value(i) & 0x02) == 0x2;
  }
  // Defines when GE bits are updated.
  bool UpdatesApsrGe(const Instruction i) const {
    return (mask.value(i) & 0x1) == 0x1;
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MoveImmediate12ToApsr);
};

// Models the use of a 16-bit immediate constant
// Op #<imm16>
// +--------+----------------+------------------------+--------+-------+
// |31302928|2726252423222120|19181716151413121110 9 8| 7 6 5 4|3 2 1 0|
// +--------+----------------+------------------------+--------+-------+
// |  cond  |                |         imm12          |        |  imm4 |
// +--------+----------------+------------------------+--------+-------+
class Immediate16Use : public ClassDecoder {
 public:
  static const Imm4Bits0To3Interface imm4;
  static const Imm12Bits8To19Interface imm12;
  static const ConditionBits28To31Interface cond;

  static uint32_t value(const Instruction& i) {
    return (imm12.value(i) << 4) | imm4.value(i);
  }

  // Methods for class
  Immediate16Use() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Immediate16Use);
};

// Models a branch to a 24-bit (left-shifted two bits) address.
// B{L}<c> <label>
// +--------+------+--+------------------------------------------------+
// |31302928|272625|24|2322212019181716151413121110 9 8 7 6 5 4 3 2 1 0|
// +--------+------+--+------------------------------------------------+
// |  cond  |      | P|                 imm24                          |
// +--------+------+--+------------------------------------------------+
class BranchImmediate24 : public ClassDecoder {
 public:
  static const Imm24AddressBits0To23Interface imm24;
  static const PrePostIndexingBit24Interface link_flag;
  static const ConditionBits28To31Interface cond;

  BranchImmediate24() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual bool is_relative_branch(Instruction i) const;
  virtual int32_t branch_target_offset(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BranchImmediate24);
};

// Defines a break point, which is also used to act as a constant pool header
// if the constant is the special pool head value.
class BreakPointAndConstantPoolHead : public Immediate16Use {
 public:
  BreakPointAndConstantPoolHead() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool is_literal_pool_head(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BreakPointAndConstantPoolHead);
};

// Models a branch to address in Rm.
// Op<c> <Rm>
// +--------+---------------------------------------------+--+--+--------+
// |31302928|2726252423222212019181716151413121110 9 8 7 6| 5| 4| 3 2 1 0|
// +--------+---------------------------------------------+--+--+--------+
// |  cond  |                                             | L|  |   Rm   |
// +--------+---------------------------------------------+--+--+--------+
// If L=1 then updates LR register.
// If L=1 and Rm=Pc, then UNPREDICTABLE.
class BranchToRegister : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface m;
  static const UpdatesLinkRegisterBit5Interface link_register;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  BranchToRegister() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
  virtual Register branch_target_register(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BranchToRegister);
};

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
class Unary1RegisterImmediateOp12 : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const Imm12Bits0To11Interface imm12;
  static const RegBits12To15Interface d;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  Unary1RegisterImmediateOp12() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterImmediateOp12);
};

// Special case of Unary1RegisterImmediateOp12, where dynamic code replace
// can replace the immediate value.
class Unary1RegisterImmediateOp12DynCodeReplace
    : public Unary1RegisterImmediateOp12 {
 public:
  Unary1RegisterImmediateOp12DynCodeReplace() {}
  Instruction dynamic_code_replacement_sentinel(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterImmediateOp12DynCodeReplace);
};

// Models a 1-register assignment of a 16-bit immediate.
// Op(S)<c> Rd, #const
// +--------+--------------+--+--------+--------+------------------------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7 6 5 4 3 2 1 0|
// +--------+--------------+--+--------+--------+------------------------+
// |  cond  |              | S|  imm4  |   Rd   |         imm12          |
// +--------+--------------+--+--------+--------+------------------------+
// Definitions:
//    Rd = The destination register.
//    const = ZeroExtend(imm4:imm12, 32)
//
// If Rd is R15, the instruction is unpredictable.
// NaCl disallows writing to PC to cause a jump.
//
// Implements:
//    MOV (immediate) A2 A8-194
class Unary1RegisterImmediateOp : public Unary1RegisterImmediateOp12 {
 public:
  // Interfaces for components in the instruction.
  static const Imm4Bits16To19Interface imm4;

  // Methods for class.
  Unary1RegisterImmediateOp() {}
  virtual SafetyLevel safety(Instruction i) const;

  // The immediate value stored in the instruction.
  uint32_t ImmediateValue(const Instruction& i) const {
    return (imm4.value(i) << 12) | imm12.value(i);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterImmediateOp);
};

// Special case of Unary1RegisterImmediateOp, where dynamic code replacement
// can replace the immediate value.
class Unary1RegisterImmediateOpDynCodeReplace
    : public Unary1RegisterImmediateOp {
 public:
  Unary1RegisterImmediateOpDynCodeReplace() {}
  Instruction dynamic_code_replacement_sentinel(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterImmediateOpDynCodeReplace);
};

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
class Unary1RegisterImmediateOpPc : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const Imm12Bits0To11Interface imm12;
  static const RegBits12To15Interface d;
  static const ConditionBits28To31Interface cond;

  Unary1RegisterImmediateOpPc() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterImmediateOpPc);
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
//   lsb = The least significant bit to be used.
//   msb = The most significant bit to be used.
//
// If Rd=R15 or msb < lsb, the instruction is unpredictable.
//
// NaCl disallows writing Pc to cause a jump.
class Binary2RegisterBitRangeMsbGeLsb : public ClassDecoder {
 public:
  // Interface for components of the instruction.
  static const RegBits0To3Interface n;
  static const Imm5Bits7To11Interface lsb;
  static const RegBits12To15Interface d;
  static const Imm5Bits16To20Interface msb;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary2RegisterBitRangeMsbGeLsb() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary2RegisterBitRangeMsbGeLsb);
};

// Models a 2-register binary operation with two immediate values
// defining a bit range.
// Op<c> Rd, Rn, #<lsb>, #width
// +--------+--------------+----------+--------+----------+------+--------+
// |31302928|27262524232221|2019181716|15141312|1110 9 8 7| 6 5 4| 3 2 1 0|
// +--------+--------------+----------+--------+----------+------+--------+
// |  cond  |              | widthm1  |   Rd   |    lsb   |      |   Rn   |
// +--------+--------------+----------+--------+----------+------+--------+
// Definitions:
//   Rd = The destination register.
//   Rn = The first operand
//   lsb = The least significant bit to be used.
//   widthm1 = The width of the bitfield minus 1.
//      msb = imm5 = The most significant bit to be used.
//
// If Rd=R15, Rn=15, or lsbit + widthminus1 > 31, the instruction is
// unpredictable.
//
// NaCl disallows writing Pc to cause a jump.
class Binary2RegisterBitRangeNotRnIsPcBitfieldExtract : public ClassDecoder {
 public:
  // Interface for components of the instruction.
  static const RegBits0To3Interface n;
  static const Imm5Bits7To11Interface lsb;
  static const RegBits12To15Interface d;
  static const Imm5Bits16To20Interface widthm1;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary2RegisterBitRangeNotRnIsPcBitfieldExtract() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Binary2RegisterBitRangeNotRnIsPcBitfieldExtract);
};

// Models a 2-register binary operation with an immediate value.
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
//
// NaCl disallows writing to PC to cause a jump.
//
//   safety := (Rd=1111 & S=1) => DECODER_ERROR &  # ARM
//             Rd=1111 => FORBIDDEN_OPERANDS;  # NaCl
class Binary2RegisterImmediateOp : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const Imm12Bits0To11Interface imm;
  static const RegBits12To15Interface d;
  static const RegBits16To19Interface n;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary2RegisterImmediateOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary2RegisterImmediateOp);
};

// Models a Binary2RegisterImmediateOp that is used to mask a memory
// address to the actual limits of user's memory, using the immediate
// value. We use this to capture ImmediateBic.
class MaskedBinary2RegisterImmediateOp : public Binary2RegisterImmediateOp {
 public:
  MaskedBinary2RegisterImmediateOp() : Binary2RegisterImmediateOp() {}
  virtual bool clears_bits(Instruction i, uint32_t mask) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MaskedBinary2RegisterImmediateOp);
};

// Special case of Binary2RegisterImmediateOp, where dynamic code replacement
// can replace the immediate value.
class Binary2RegisterImmediateOpDynCodeReplace
    : public Binary2RegisterImmediateOp {
 public:
  Binary2RegisterImmediateOpDynCodeReplace() {}
  Instruction dynamic_code_replacement_sentinel(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary2RegisterImmediateOpDynCodeReplace);
};

// Models a Binary2RegisterImmediateOp with the safety constraints:
//   safety := (Rd=1111 & S=1) => DECODER_ERROR &  # ARM
//             (Rn=1111 & S=0) => DECODER_ERROR &
//             Rd=1111 => FORBIDDEN_OPERANDS;      # NaCl
class Binary2RegisterImmediateOpAddSub : public Binary2RegisterImmediateOp {
 public:
  Binary2RegisterImmediateOpAddSub() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary2RegisterImmediateOpAddSub);
};

// Models a Register to immediate test of the form:
// Op(S)<c> Rn, #<const>
// +--------+--------------+--+--------+--------+------------------------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7 6 5 4 3 2 1 0|
// +--------+--------------+--+--------+--------+------------------------+
// |  cond  |              | S|   Rn   |        |        imm12           |
// +--------+--------------+--+--------+--------+------------------------+
// Definitions:
//    Rn 0 The operand register.
//    const = ARMExpandImm_C(imm12, APSR.C)
//
// Implements:
//    CMN(immediate) A1 A8-74
//    CMP(immediate) A1 A8-80
//    TEQ(immediate) A1 A8-448
//    TST(immediate) A1 A8-454 - Note: See
//                               class MaskedBinaryRegisterImmediateTest.
class BinaryRegisterImmediateTest : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const Imm12Bits0To11Interface imm;
  static const RegBits16To19Interface n;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  BinaryRegisterImmediateTest() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BinaryRegisterImmediateTest);
};

// Models a BinaryRegisterImmediateTest that can be used to set a condition
// by testing if the immediate value appropriately masks the value in Rn.
class MaskedBinaryRegisterImmediateTest : public BinaryRegisterImmediateTest {
 public:
  MaskedBinaryRegisterImmediateTest() : BinaryRegisterImmediateTest() {}
  virtual bool sets_Z_if_bits_clear(Instruction i,
                                    Register r,
                                    uint32_t mask) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MaskedBinaryRegisterImmediateTest);
};

// Models a 2 register unary operation of the form:
// Op(S)<c> <Rd>, <Rm>
// +--------+--------------+--+--------+--------+----------------+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7 6 5 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+----------------+--------+
// |  cond  |              | S|        |   Rd   |                |   Rm   |
// +--------+--------------+--+--------+--------+----------------+--------+
// Definitions:
//    Rd - The destination register.
//    Rm - The source register.
//
// NaCl disallows writing to PC to cause a jump.
//
// Implements:
//    MOV(register) A1 A8-196 - Shouldn't parse when Rd=15 and S=1;
//    RRX A1 A8-282
class Unary2RegisterOp : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface m;
  static const RegBits12To15Interface d;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Unary2RegisterOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterOp);
};

// A Unary2RegisterOp with the additional constraint that if Rm=R15,
// the instruction is unpredictable.
class Unary2RegisterOpNotRmIsPc : public Unary2RegisterOp {
 public:
  Unary2RegisterOpNotRmIsPc() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterOpNotRmIsPc);
};

// Unary2RegisterOpNotRmIsPc where the conditions flags are not set, even
// though bit S may be true.
class Unary2RegisterOpNotRmIsPcNoCondUpdates
    : public Unary2RegisterOpNotRmIsPc {
 public:
  Unary2RegisterOpNotRmIsPcNoCondUpdates() {}
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterOpNotRmIsPcNoCondUpdates);
};

// Models a 2 register immediate shifted unary operation of the form:
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
class Unary2RegisterShiftedOp : public Unary2RegisterOp {
 public:
  // Interface for components in the instruction.
  static const ShiftTypeBits5To6Interface shift_type;
  static const Imm5Bits7To11Interface imm5;

  Unary2RegisterShiftedOp() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterShiftedOp);
};

// A Unary2RegisterShiftedOp with the additional constraint that imm5!=00000.
//   safety := (Rd=1111 & S=1) => DECODER_ERROR &  # ARM
//             imm5=00000 => DECODER_ERROR &
//             Rd=1111 => FORBIDDEN_OPERANDS;      # NaCl
class Unary2RegisterShiftedOpImmNotZero : public Unary2RegisterShiftedOp {
 public:
  Unary2RegisterShiftedOpImmNotZero() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterShiftedOpImmNotZero);
};

// Models a register immediate shifted binary operation of the form:
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
//
// NaCl disallows writing to PC to cause a jump.
class Binary3RegisterShiftedOp : public Unary2RegisterShiftedOp {
 public:
  static const RegBits16To19Interface n;

  Binary3RegisterShiftedOp() {}
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterShiftedOp);
};

// Models a 3-register binary operation of the form:
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
//
// If Rd, Rm, or Rn is R15, the instruction is unpredictable.
// NaCl disallows writing to PC to cause a jump.
//
// Implements:
//    ASR(register) A1 A8-42
//    LSL(register) A1 A8-180
//    LSR(register) A1 A8-184
//    ROR(register) A1 A8-280
class Binary3RegisterOp : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface n;
  static const RegBits8To11Interface m;
  static const RegBits12To15Interface d;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary3RegisterOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOp);
};

// Modes a 2-register load (exclusive) operation.
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
class LoadExclusive2RegisterOp : public ClassDecoder {
 public:
  static const RegBits12To15Interface t;
  static const RegBits16To19Interface n;
  static const ConditionBits28To31Interface cond;

  LoadExclusive2RegisterOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadExclusive2RegisterOp);
};

// Modes a 2-register load (exclusive) operation where the source is double
// wide (i.e. Rt and Rt2).
//
// Additional ARM constraints:
//    Rt<0>=1 then unpredictable.
//    Rt=14, then unpredictable (i.e. Rt2=R15).
class LoadExclusive2RegisterDoubleOp : public LoadExclusive2RegisterOp {
 public:
  static const RegBits12To15Plus1Interface t2;

  LoadExclusive2RegisterDoubleOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadExclusive2RegisterDoubleOp);
};

// Models a (PC relative) load 8-bit immediate operation of form:
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
//  is_literal_load := true;
//  safety := P=0 & W=1 => DECODER_ERROR &
//            P == W => UNPREDICTABLE &
//            Rt == Pc => UNPREDICTABLE &
//            # TODO(karl) Missing check:
//            # (ArchVersion() < 7) &
//            # not (UnalignedSupport() | address(0)=0) => UNKNOWN
class LoadRegisterImm8Op : public ClassDecoder {
 public:
  static const Imm4Bits0To3Interface imm4L;
  static const Imm4Bits8To11Interface imm4H;
  static const RegBits12To15Interface t;
  static const WritesBit21Interface writes;
  static const AddOffsetBit23Interface direction;
  static const PrePostIndexingBit24Interface indexing;
  static const ConditionBits28To31Interface cond;

  LoadRegisterImm8Op() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  virtual bool is_literal_load(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadRegisterImm8Op);
};

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
class LoadRegisterImm8DoubleOp : public LoadRegisterImm8Op {
 public:
  static const RegBits12To15Plus1Interface t2;

  LoadRegisterImm8DoubleOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadRegisterImm8DoubleOp);
};

// Models a 2-register load/store 8-bit immediate operation of the forms:
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
class LoadStore2RegisterImm8Op : public ClassDecoder {
 public:
  static const Imm4Bits0To3Interface imm4L;
  static const Imm4Bits8To11Interface imm4H;
  static const RegBits12To15Interface t;
  static const RegBits16To19Interface n;
  static const WritesBit21Interface writes;
  static const AddOffsetBit23Interface direction;
  static const PrePostIndexingBit24Interface indexing;
  static const ConditionBits28To31Interface cond;

  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual Register base_address_register(Instruction i) const;

  bool HasWriteBack(const Instruction i) const {
    return indexing.IsPostIndexing(i) || writes.IsDefined(i);
  }

 protected:
  // TODO(jfb) Remove friend? It looks like its usage of the protected
  //           ctor shouldn't be.
  friend class nacl_arm_test::LoadStore2RegisterImm8OpTester;
  explicit LoadStore2RegisterImm8Op(bool is_load)
      : ClassDecoder(), is_load_(is_load) {}
  bool is_load_;  // true if load (rather than store).

 private:
  NACL_DISALLOW_DEFAULT_COPY_AND_ASSIGN(LoadStore2RegisterImm8Op);
};

// Defines the virtuals for a load immediate instruction.
class Load2RegisterImm8Op : public LoadStore2RegisterImm8Op {
 public:
  Load2RegisterImm8Op() : LoadStore2RegisterImm8Op(true) {
  }
  virtual RegisterList defs(Instruction i) const;
  virtual bool is_literal_load(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load2RegisterImm8Op);
};

// Defines the virtuals for a store immediate instruction.
class Store2RegisterImm8Op : public LoadStore2RegisterImm8Op {
 public:
  Store2RegisterImm8Op() : LoadStore2RegisterImm8Op(false) {
  }
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 protected:
  NACL_DISALLOW_COPY_AND_ASSIGN(Store2RegisterImm8Op);
};

// Models a 2-register immediate load/store operation where the source/target
// is double wide (i.e.  Rt and Rt2).
class LoadStore2RegisterImm8DoubleOp
    : public LoadStore2RegisterImm8Op {
 public:
  // Interface for components in the instruction (and not inherited).
  static const RegBits12To15Plus1Interface t2;

  virtual SafetyLevel safety(Instruction i) const;

 protected:
  // TODO(jfb) Remove friend? It looks like its usage of the protected
  //           ctor shouldn't be.
  friend class nacl_arm_test::LoadStore2RegisterImm8DoubleOpTester;
  explicit LoadStore2RegisterImm8DoubleOp(bool is_load)
      : LoadStore2RegisterImm8Op(is_load) {}

 private:
  NACL_DISALLOW_DEFAULT_COPY_AND_ASSIGN(LoadStore2RegisterImm8DoubleOp);
};

// Defines the virtuals for a load immediate double instruction.
class Load2RegisterImm8DoubleOp
    : public LoadStore2RegisterImm8DoubleOp {
 public:
  Load2RegisterImm8DoubleOp() : LoadStore2RegisterImm8DoubleOp(true) {
  }
  virtual RegisterList defs(Instruction i) const;
  virtual bool is_literal_load(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load2RegisterImm8DoubleOp);
};

// Defines the virtuals for a store immediate double instruction.
class Store2RegisterImm8DoubleOp
    : public LoadStore2RegisterImm8DoubleOp {
 public:
  Store2RegisterImm8DoubleOp()
      : LoadStore2RegisterImm8DoubleOp(false) {
  }
  virtual RegisterList defs(Instruction i) const;

 protected:
  NACL_DISALLOW_COPY_AND_ASSIGN(Store2RegisterImm8DoubleOp);
};

// Models a prefetch load/store 12-bit immediate operation.
// Op [<Rn>, #+/-<imm12>]
// +--------+--------+--+--+----+--------+--------+------------------------+
// |31302928|27262524|23|22|2120|19181716|15141312|1110 9 8 7 6 5 4 3 2 1 0|
// +--------+--------+--+--+----+--------+--------+------------------------+
// |  cond  |        | U| R|    |   Rn   |        |          imm12         |
// +--------+--------+--+--+----+--------+--------+------------------------+
// cond=1111
// U - defines direction (+/-).
// R - 1 if memory access is a read (otherwise write).
// Rn - The base register.
// imm12 - The offset from the address.
//
// Preloads are only allowed if Rn was masked: ARM CPUs otherwise leak
// information which an attacker could use to figure out where the TCB lives.
//
// There is no need to mask when Rn==PC: the immediate is small. We represent
// these preloads as literal loads.
class PreloadRegisterImm12Op : public ClassDecoder {
 public:
  static const Imm12Bits0To11Interface imm12;
  static const RegBits16To19Interface n;
  static const FlagBit22Interface read;
  static const AddOffsetBit23Interface direction;
  static const ConditionBits28To31Interface cond;

  PreloadRegisterImm12Op() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  virtual bool is_literal_load(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PreloadRegisterImm12Op);
};

// Models a prefetch register pair load/store operation.
// Op [<Rn>, #+/-<Rm>{, <shift>}]
// +--------+--------+--+--+----+--------+--------+----------+----+--+--------+
// |31302928|27262523|23|22|2120|19181716|15141312|1110 9 8 7| 6 5| 4| 3 2 1 0|
// +--------+--------+--+--+----+--------+--------+----------+----+--+--------+
// |  cond  |        | U| R|    |   Rn   |        |   imm5   |type|  |   Rm   |
// +--------+--------+--+--+----+--------+--------+----------+----+--+--------+
// cond=1111
// U - defines direction (+/-)
// R - 1 if memory access is a read (otherwise write).
// Rn - The base register.
// Rm - The offset that is optionally shifted and applied to the value of <Rn>
//      to form the address.
//
// Register-register preloads are completely disallowed for the same reason
// register-immediate preloads must be masked.
class PreloadRegisterPairOp : public ClassDecoder {
 public:
  static const RegBits0To3Interface m;
  static const ShiftTypeBits5To6Interface shift_type;
  static const Imm5Bits7To11Interface imm5;
  static const RegBits16To19Interface n;
  static const FlagBit22Interface read;
  static const AddOffsetBit23Interface direction;
  static const ConditionBits28To31Interface cond;

  PreloadRegisterPairOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PreloadRegisterPairOp);
};

// Models a 2-register load/store 12-bit immediate operation of the forms:
// Op<c> <Rt>, [<Rn> {, #+/-<imm12>}]
// Op<c> <Rt>, [<Rn>], #+/-<imm12>
// Op<c> <Rt>, [<Rn>, #+/-<imm12>]
// +--------+------+--+--+--+--+--+--------+--------+------------------------+
// |31302928|272625|24|23|22|21|20|19181716|15141312|1110 9 8 7 6 5 4 3 2 1 0|
// +--------+------+--+--+--+--+--+--------+--------+------------------------+
// |  conds |      | P| U| B| w|  |   Rn   |   Rt   |        imm12           |
// +--------+------+--+--+--+--+--+--------+--------+------------------------+
// wback = (P=0 || W==1)
//
// if P=0 and W=1, should not parse as this instruction.
// if wback && (Rn=15 or Rn=Rt) then unpredictable.
// if B, load/store a byte. Otherwise, load/store a word.
// NaCl disallows writing to PC.
class LoadStore2RegisterImm12Op : public ClassDecoder {
 public:
  static const Imm12Bits0To11Interface imm12;
  static const RegBits12To15Interface t;
  static const RegBits16To19Interface n;
  static const FlagBit22Interface byte;
  static const WritesBit21Interface writes;
  static const AddOffsetBit23Interface direction;
  static const PrePostIndexingBit24Interface indexing;
  static const ConditionBits28To31Interface cond;

  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  bool HasWriteBack(const Instruction i) const {
    return indexing.IsPostIndexing(i) || writes.IsDefined(i);
  }

 protected:
  // TODO(jfb) Remove friend? It looks like its usage of the protected
  //           ctor shouldn't be.
  friend class nacl_arm_test::LoadStore2RegisterImm12OpTester;
  explicit LoadStore2RegisterImm12Op(bool is_load)
      : ClassDecoder() , is_load_(is_load) {}
  bool is_load_;  // true if load (rather than store).

 private:
  NACL_DISALLOW_DEFAULT_COPY_AND_ASSIGN(LoadStore2RegisterImm12Op);
};

// Defines the virtuals for a load immediate instruction.
//
// For load register immediate (Ldr rule 59, A1 on page 122):
// If Rn=Sp && P=0 && U=1 && W=0 && imm12=4, then POP ().
//    Note: This is just a special case instruction that behaves like a
//    Load2RegisterImm12Op instruction. That is, the pop loads from the
//    top of stack the value of Rt, and then increments the stack by 4. Since
//    this doesn't affect the NaCl constraints we need for such loads, we do
//    not model it as a special instruction.

class Load2RegisterImm12Op : public LoadStore2RegisterImm12Op {
 public:
  Load2RegisterImm12Op() : LoadStore2RegisterImm12Op(true) {
  }
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
  virtual bool is_literal_load(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load2RegisterImm12Op);
};

// Extends class Load2RegisterImm12Op to define virtual
// is_load_thread_address_pointer, assuming it is the
// instruction LDR (immedate).
class LdrImmediateOp : public Load2RegisterImm12Op {
 public:
  LdrImmediateOp() {}
  virtual bool is_load_thread_address_pointer(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LdrImmediateOp);
};

// Defines the virtuals for a store immediate instruction.
//
// For store register immediate (Str rule 194, A1 on page 384):
// if Rn=Sp && P=1 && U=0 && W=1 && imm12=4, then PUSH.
//    Note: This is just a special case instruction that behaves like
//    a Store2RegisterImm12Op instruction. That is, the push loads the
//    value of Rt on top of the stack, and then increments the stack by 4.
//    Since this doesn't affect the NaCl constraints we need for such loads,
//    we do not model it as a special instruction.
class Store2RegisterImm12Op : public LoadStore2RegisterImm12Op {
 public:
  Store2RegisterImm12Op() : LoadStore2RegisterImm12Op(false) {
  }
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Store2RegisterImm12Op);
};

// Models a load/store of  multiple registers into/out of memory.
// Op<c> <Rn>{!}, <registers>
// +--------+------------+--+--+--------+--------------------------------+
// |31302928|272625242322|21|20|19181716|151413121110 9 8 7 6 5 4 3 2 1 0|
// +--------+------------+--+--+--------+--------------------------------+
// |  cond  |            | W|  |   Rn   |         register_list          |
// +--------+------------+--+--+--------+--------------------------------+
// if n=15 || BitCount(registers) < 1 then UNPREDICTABLE.
class LoadStoreRegisterList : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegisterListBits0To15Interface register_list;
  static const RegBits16To19Interface n;
  static const WritesBit21Interface wback;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  LoadStoreRegisterList() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStoreRegisterList);
};

// A LoadStoreRegisterList with constraint:
// Arm constraints:
// If wback && register<n> == '1' && ArchVerision() >= 7 then UNPREDICTABLE.
// Note: Since we don't know how to implement ArchVersion(), we are being
// safe by assuming ArchVersion() >= 7.
// Nacl Constraints:
// If registers<pc> == '1' then FORBIDDEN_OPERANDS.
class LoadRegisterList : public LoadStoreRegisterList {
 public:
  LoadRegisterList() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadRegisterList);
};

// A LoadStoreRegisterList with constraint:
// If wback && register<n> == '1' && n != LowestSetBit(registers)
//   then UNPREDICTABLE.
class StoreRegisterList : public LoadStoreRegisterList {
 public:
  StoreRegisterList() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreRegisterList);
};

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
//     coproc=1011. coproc=1011 implies the register size is 32-bit
class LoadStoreVectorOp : public CondVfpOp {
 public:
  // Interfaces for components in the instruction.
  static const Imm8Bits0To7Interface imm8;
  static const RegBits12To15Interface vd;
  static const RegBits16To19Interface n;
  static const WritesBit21Interface wback;
  static const Imm1Bit22Interface d_bit;
  static const AddOffsetBit23Interface direction;
  static const PrePostIndexingBit24Interface indexing;

  LoadStoreVectorOp() {}
  virtual Register base_address_register(Instruction i) const;

  // Returns the first register in the register list.
  Register FirstReg(const Instruction& i) const;
};

// Models a vector load/store of multiple vector registers into/out of memory.
// Op{mode}<c><q> {,<size>} <Rn>!, <register list>
//
// See base class LoadStoreVectorOp for layout of this instruction.
//
// <mode> in { IA , DB } where IA implies P=0 and U=1,
//                       and DB implies P=1 and U=0
// <size> in { 32 , 64 } defines the number of registers in the register list.
// <Rn> Is the base register defining the memory to use.
// <register list> is a list of consecutively numbered registers defining
//        the vector registers to use (the list is defined by <Vd>, D,
//        and <imm8>).
// <imm8> defines how many (32-bit) registers are to be updated.
//
// Constraints:
//    P=U && W=1 then undefined.
//    if Rn=15  && (W=1 || CurrentInstSet() != ARM) then UNPREDICTABLE.
//    if imm8=0 || (first_reg +imm8) > 32 then UNPREDICTABLE.
//    if coproc=1011 and imm8 > 16 then UNPREDICTABLE.
//
// Note: Legal combinations of PUW: { 010 , 011, 101 }
class LoadStoreVectorRegisterList : public LoadStoreVectorOp {
 public:
  LoadStoreVectorRegisterList() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;

  // Returns the number of register in the register list, i.e. <size>.
  uint32_t NumRegisters(const Instruction& i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStoreVectorRegisterList);
};

// Models a load of a LoadStoreVectorRegisterList.
//
class LoadVectorRegisterList : public LoadStoreVectorRegisterList {
 public:
  LoadVectorRegisterList() {}
  virtual bool is_literal_load(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadVectorRegisterList);
};

// Models a store of a LoadStoreVectorRegisterList.
//
// Adds NaCl Constraint:
//    Rn != PC  (i.e. don't change code space).
class StoreVectorRegisterList : public LoadStoreVectorRegisterList {
 public:
  StoreVectorRegisterList() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreVectorRegisterList);
};

// Models a load/store of a vector register into/out of memory.
// Op<c> <Dd>, [<Rn>{, #+/-<imm8>}]
// Op<c> <Sd>, [<Rn>{, #+/-<imm8>}]
//
// See base class LoadStoreVectorOp for layout of this instruction.
//
// <imm8> defines the offset to add to Rn to compute the memory address.
class LoadStoreVectorRegister : public LoadStoreVectorOp {
 public:
  LoadStoreVectorRegister() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStoreVectorRegister);
};

// Models a LoadStoreVectorRegister where it is a load.
class LoadVectorRegister : public LoadStoreVectorRegister {
 public:
  LoadVectorRegister() {}
  virtual bool is_literal_load(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadVectorRegister);
};

// Models a LoadStoreVectorRegister where it is a store.
//
// Adds ARM Constraint:
//    if Rn=PC && CurrentInstSet() != ARM then UNPREDICTABLE.
//
// Adds NaCl Constraint:
//    Rn != PC  (i.e. don't change code space).
class StoreVectorRegister : public LoadStoreVectorRegister {
 public:
  StoreVectorRegister() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreVectorRegister);
};

// Models a 3-register binary operation of the form:
// Op(S)<c> <Rd>, <Rn>, <Rm>
// +--------+--------------+--+--------+--------+--------+--------+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8| 7 6 5 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+--------+--------+--------+
// |  cond  |              | S|   Rd   |        |   Rm   |        |   Rn   |
// +--------+--------------+--+--------+--------+--------+--------+--------+
// Definitions:
//    Rd - The destination register.
//    Rn - The first operand
//    Rm - The second operand
//    S - Defines if the flags regsiter is updated.
//
// If Rd, Rm, or Rn is R15, the instruction is unpredictable.
// NaCl disallows writing to PC to cause a jump.
class Binary3RegisterOpAltA : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface n;
  static const RegBits8To11Interface m;
  static const RegBits16To19Interface d;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary3RegisterOpAltA() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpAltA);
};

// Same as Binary3RegisterOpAltA, but the S flag is ignored. That is,
// conditions NZCV are not updated.
class Binary3RegisterOpAltANoCondsUpdate : public Binary3RegisterOpAltA {
 public:
  Binary3RegisterOpAltANoCondsUpdate() : Binary3RegisterOpAltA() {}
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpAltANoCondsUpdate);
};

// Models a 3-register binary operation of the form:
// Op(S)<c> <Rd>, <Rn>, <Rm>
// +--------+--------------+--+--------+--------+----------------+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7 6 5 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+----------------+--------+
// |  cond  |                 |   Rn   |   Rd   |                |   Rm   |
// +--------+--------------+--+--------+--------+----------------+--------+
// Definitions:
//    Rd - The destination register.
//    Rn - The first operand
//    Rm - The second operand
//
// If Rd, Rm, or Rn is R15, the instruction is unpredictable.
// NaCl disallows writing to PC to cause a jump.
class Binary3RegisterOpAltBNoCondUpdates : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface m;
  static const RegBits12To15Interface d;
  static const RegBits16To19Interface n;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary3RegisterOpAltBNoCondUpdates() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpAltBNoCondUpdates);
};

// Models a 4-register double binary operation of the form:
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
//
// If Rd, Rm, Rn, or Ra is R15, the instruction is unpredictable.
// NaCl disallows writing to PC to cause a jump.
class Binary4RegisterDualOp : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface n;
  static const RegBits8To11Interface m;
  static const RegBits12To15Interface a;
  static const RegBits16To19Interface d;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class
  Binary4RegisterDualOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary4RegisterDualOp);
};

// A Binary4RegisterDualOp where S is ignored. That is,
// conditions NZCV are not updated.
class Binary4RegisterDualOpNoCondsUpdate : public Binary4RegisterDualOp {
 public:
  Binary4RegisterDualOpNoCondsUpdate() : Binary4RegisterDualOp() {}
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary4RegisterDualOpNoCondsUpdate);
};

// A Binary4RegisterDualOp that adds the restruction that Rd!=Rn if
// ArchVersion<6.
class Binary4RegisterDualOpLtV6RdNotRn : public Binary4RegisterDualOp {
 public:
  Binary4RegisterDualOpLtV6RdNotRn() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary4RegisterDualOpLtV6RdNotRn);
};

// Models a dual level, 2 input, 2 output binary operation of the form:
// Op(S)<c> <RdLo>, <RdHi>, <Rn>, <Rm>
// +--------+--------------+--+--------+--------+--------+--------+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8| 7 6 5 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+--------+--------+--------+
// |  cond  |              | S|  RdHi  |  RdLo  |   Rm   |        |   Rn   |
// +--------+--------------+--+--------+--------+--------+--------+--------+
// Definitions:
//    RdHi - Input to the outer binary operation, and the upper 32-bit result
//           of that operation.
//    RdLo - Input to the outer bianry operation, and the lower 32-bit result
//           of that operation.
//    Rn - The first operand to the inner binary operation.
//    Rm - The second operand to the inner binary operation.
// Note: The result of the inner operation is a 64-bit value used as an
//    argument to the outer operation.
//
// If RdHi, RdLo, Rn, or Rm is R15, the instruction is unpredictable.
// If RdHi == RdLo, the instruction is unpredictable.
// NaCl disallows writing to PC to cause a jump.
//
//   defs := {RdLo, RdHi, NZCV if setflags else None};
//   safety := Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE &
//             RdHi == RdLo => UNPREDICTABLE;
//   uses := {RdLo, RdHi, Rn, Rm};
class Binary4RegisterDualResult : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface n;
  static const RegBits8To11Interface m;
  static const RegBits12To15Interface d_lo;
  static const RegBits16To19Interface d_hi;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class
  Binary4RegisterDualResult() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary4RegisterDualResult);
};

// A Binary4RegisterDualResult where S is ignored. That is,
// conditions NZCV are not updated.
class Binary4RegisterDualResultNoCondsUpdate
    : public Binary4RegisterDualResult {
 public:
  Binary4RegisterDualResultNoCondsUpdate()
      : Binary4RegisterDualResult() {}
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary4RegisterDualResultNoCondsUpdate);
};

// A Binary4RegisterDualResult where its input (i.e. uses) is just
// Rn and Rm (and doesn't include RdHi and RdLo).
//
//   setflags := S=1;
//   safety := Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE &     # ARM
//             RdHi == RdLo => UNPREDICTABLE &
//             (ArchVersion() < 6 & (RdHi == Rn | RdLo == Rn)) => UNPREDICTABLE;
//   uses := {Rn, Rm};
class Binary4RegisterDualResultUsesRnRm : public Binary4RegisterDualResult {
 public:
  Binary4RegisterDualResultUsesRnRm() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary4RegisterDualResultUsesRnRm);
};

// A Binary4RegisterDualResult where RdHi and RdLo can't be Rn if
// ArchVersion < 6.
//
//   safety := Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE &     # ARM
//             RdHi == RdLo => UNPREDICTABLE &
//             (ArchVersion() < 6 & (RdHi == Rn | RdLo == Rn))
//                          => UNPREDICTABLE;
class Binary4RegisterDualResultLtV6RdHiLoNotRn
    : public Binary4RegisterDualResult {
 public:
  Binary4RegisterDualResultLtV6RdHiLoNotRn() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary4RegisterDualResultLtV6RdHiLoNotRn);
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
class LoadStore3RegisterOp : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface m;
  static const RegBits12To15Interface t;
  static const RegBits16To19Interface n;
  static const WritesBit21Interface writes;
  static const AddOffsetBit23Interface direction;
  static const PrePostIndexingBit24Interface indexing;
  static const ConditionBits28To31Interface cond;

  virtual SafetyLevel safety(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  bool HasWriteBack(const Instruction i) const {
    return indexing.IsPostIndexing(i) || writes.IsDefined(i);
  }

 protected:
  // TODO(jfb) Remove friend? It looks like its usage of the protected
  //           ctor shouldn't be.
  friend class nacl_arm_test::LoadStore3RegisterOpTester;
  explicit LoadStore3RegisterOp(bool is_load)
      : ClassDecoder(), is_load_(is_load) {}
  bool is_load_;  // true if load (rather than store).

 private:
  NACL_DISALLOW_DEFAULT_COPY_AND_ASSIGN(LoadStore3RegisterOp);
};

// Defines the virtuals for a load register instruction.
class Load3RegisterOp : public LoadStore3RegisterOp {
 public:
  Load3RegisterOp() : LoadStore3RegisterOp(true) {
  }
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load3RegisterOp);
};

// Defines the virtuals for a store register instruction.
class Store3RegisterOp : public LoadStore3RegisterOp {
 public:
  Store3RegisterOp() : LoadStore3RegisterOp(false) {
  }
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Store3RegisterOp);
};

// Models a 3-register load/store operation where the source/target is double
//  wide (i.e. Rt and Rt2).
class LoadStore3RegisterDoubleOp : public LoadStore3RegisterOp {
 public:
  // Interface for components in the instruction (and not inherited).
  static const RegBits12To15Plus1Interface t2;

  virtual SafetyLevel safety(Instruction i) const;

 protected:
  // TODO(jfb) Remove friend? It looks like its usage of the protected
  //           ctor shouldn't be.
  friend class nacl_arm_test::LoadStore3RegisterDoubleOpTester;
  explicit LoadStore3RegisterDoubleOp(bool is_load)
      : LoadStore3RegisterOp(is_load) {}

 private:
  NACL_DISALLOW_DEFAULT_COPY_AND_ASSIGN(LoadStore3RegisterDoubleOp);
};

// Defines the virtuals for a load double register instruction
class Load3RegisterDoubleOp : public LoadStore3RegisterDoubleOp {
 public:
  Load3RegisterDoubleOp() : LoadStore3RegisterDoubleOp(true) {
  }
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load3RegisterDoubleOp);
};

// Defines the virtuals for s store double register instruction.
class Store3RegisterDoubleOp : public LoadStore3RegisterDoubleOp {
 public:
  Store3RegisterDoubleOp() : LoadStore3RegisterDoubleOp(false) {
  }
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Store3RegisterDoubleOp);
};

// Models a 2-register store (exclusive) operation with a register to hold the
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
class StoreExclusive3RegisterOp : public ClassDecoder {
 public:
  static const RegBits0To3Interface t;
  static const RegBits12To15Interface d;
  static const RegBits16To19Interface n;
  static const ConditionBits28To31Interface cond;

  StoreExclusive3RegisterOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreExclusive3RegisterOp);
};

// Models a 2-register store (exclusive) operation with a register to hold the
// status of the update, and the source is double wide (i.e. Rt and Rt2).
//
// Additional ARM constraints:
//    Rt<0>=1 then unpredictable.
//    Rt=14, then unpredictable (i.e. Rt2=R15).
//    Rd=Rt2, then unpredictable.
class StoreExclusive3RegisterDoubleOp : public StoreExclusive3RegisterOp {
 public:
  static const RegBits0To3Plus1Interface t2;

  StoreExclusive3RegisterDoubleOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreExclusive3RegisterDoubleOp);
};


// Models a 3-register with (shifted) immediate 5 load/store operation of
// the forms:
// Op<c> <Rt>, [<Rn>, +/-<Rm> {, <shift>}]{!}
// Op<c> <Rt>, [<Rn>], +-<Rm> {, <shift>}
// +------+------+--+--+--+--+--+--------+--------+----------+----+--+---------+
// |31..28|272625|24|23|22|21|20|19181716|15141312|1110 9 8 7| 6 5| 4| 3 2 1 0 |
// +------+------+--+--+--+--+--+--------+--------+----------+----+--+---------+
// | cond |      | P| U| B| W|  |   Rm   |   Rt   |   imm5   |type|  |    Rm   |
// +------+------+--+--+--+--+--+--------+--------+----------+----+--+---------+
// wback = (P=0 || W=1)
//
// If P=0 and W=1, should not parse as this instruction.
// If Rm=15 then unpredicatble.
// If wback && (Rn=15 or Rn=Rt) then unpredictable.
// if ArchVersion() < 6 && wback && Rm=Rn then unpredictable.
// if B, load/store a byte. Otherwise, load/store a word.
// NaCl Disallows writing to PC.
class LoadStore3RegisterImm5Op : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface m;
  static const RegBits12To15Interface t;
  static const RegBits16To19Interface n;
  static const WritesBit21Interface writes;
  static const FlagBit22Interface byte;
  static const AddOffsetBit23Interface direction;
  static const PrePostIndexingBit24Interface indexing;
  static const ConditionBits28To31Interface cond;
  static const ShiftTypeBits5To6Interface shift_type;
  static const Imm5Bits7To11Interface imm;

  virtual SafetyLevel safety(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;

  bool HasWriteBack(const Instruction i) const {
    return indexing.IsPostIndexing(i) || writes.IsDefined(i);
  }

  // The immediate value stored in the instruction.
  uint32_t ImmediateValue(const Instruction& i) const {
    return shift_type.DecodeImmShift(i, imm.value(i));
  }

 protected:
  // TODO(jfb) Remove friend? It looks like its usage of the protected
  //           ctor shouldn't be.
  friend class nacl_arm_test::LoadStore3RegisterImm5OpTester;
  explicit LoadStore3RegisterImm5Op(bool is_load)
      : ClassDecoder(), is_load_(is_load) {}
  bool is_load_;  // true if load (rather than store).

 private:
  NACL_DISALLOW_DEFAULT_COPY_AND_ASSIGN(LoadStore3RegisterImm5Op);
};

// Defines the virtuals for a load register instruction.
class Load3RegisterImm5Op : public LoadStore3RegisterImm5Op {
 public:
  Load3RegisterImm5Op() : LoadStore3RegisterImm5Op(true) {
  }
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load3RegisterImm5Op);
};

// Defines the virtuals for a store register instruction.
class Store3RegisterImm5Op : public LoadStore3RegisterImm5Op {
 public:
  Store3RegisterImm5Op() : LoadStore3RegisterImm5Op(false) {
  }
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Store3RegisterImm5Op);
};

// Models a 2-register immediate-shifted unary operation of the form:
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
//
// NaCl disallows writing to PC to cause a jump.
//
// Implements:
//    ASR(immediate) A1 A8-40
//    LSL(immediate) A1 A8-178 -- Shouldn't parse when imm5=0.
//    LSR(immediate) A1 A8-182
//    MVN(register) A8-216     -- Shouldn't parse when Rd=15 and S=1.
//    ROR(immediate) A1 A8-278 -- Shouldn't parse when imm5=0.
class Unary2RegisterImmedShiftedOp : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface m;
  static const ShiftTypeBits5To6Interface shift_type;
  static const Imm5Bits7To11Interface imm;
  static const RegBits12To15Interface d;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Unary2RegisterImmedShiftedOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

  // The immediate value stored in the instruction.
  uint32_t ImmediateValue(const Instruction& i) const {
    return shift_type.DecodeImmShift(i, imm.value(i));
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterImmedShiftedOp);
};

// Implements a Unary2RegisterImmedShiftedOp with the constraint
// that if either Rm or Rd is Pc, the instruction is unpredictable.
class Unary2RegisterImmedShiftedOpRegsNotPc
    : public Unary2RegisterImmedShiftedOp {
 public:
  Unary2RegisterImmedShiftedOpRegsNotPc() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterImmedShiftedOpRegsNotPc);
};

// Models a 2-register immediate-shifted unary operation with saturation
// of the form:
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
class Unary2RegisterSatImmedShiftedOp : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface n;
  static const ShiftTypeBits5To6Interface shift_type;
  static const Imm5Bits7To11Interface imm5;
  static const RegBits12To15Interface d;
  static const Imm5Bits16To20Interface sat_immed;
  static const ConditionBits28To31Interface cond;

  // The immediate value stored in the instruction.
  uint32_t ImmediateValue(const Instruction& i) const {
    return shift_type.DecodeImmShift(i, imm5.value(i));
  }

  // Methods for class.
  Unary2RegisterSatImmedShiftedOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterSatImmedShiftedOp);
};

// Models a 3-register-shifted unary operation of the form:
// Op(S)<c> <Rd>, <Rm>,  <type> <Rs>
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8| 7| 6 5| 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// |  cond  |              | S|        |   Rd   |   Rs   |  |type|  |   Rm   |
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// Definitions:
//    Rd - The destination register.
//    Rm - The register that is shifted and used as the operand.
//    Rs - The regsiter whose bottom byte contains the amount to shift by.
//    type - The type of shift to apply (not modeled).
//    S - Defines if the flags regsiter is updated.
//
// if Rd, Rs, or Rm is R15, the instruction is unpredictable.
// NaCl disallows writing to PC to cause a jump.
//
// Implements:
//    MVN(register-shifted) A1 A8-218
class Unary3RegisterShiftedOp : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface m;
  static const ShiftTypeBits5To6Interface shift_type;
  static const RegBits8To11Interface s;
  static const RegBits12To15Interface d;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Unary3RegisterShiftedOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary3RegisterShiftedOp);
};

// Models a 3-register immediate-shifted binary operation of the form:
// Op(S)<c> <Rd>, <Rn>, <Rm> {,<shift>}
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7| 6 5| 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// |  cond  |              | S|   Rn   |   Rd   |   imm5   |type|  |   Rm   |
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// Definitions:
//    Rd - The destination register.
//    Rn - The first operand register.
//    Rm - The second operand that is (optionally) shifted.
//    shift = DecodeImmShift(type, imm5) is the amount to shift.
//
// NaCl disallows writing to PC to cause a jump.
//
// Implements:
//    ADC(register) A1 A8-16  -- Shouldn't parse when Rd=15 and S=1.
//    ADD(register) A1 A8-24  -- Shouldn't parse when Rd=15 and S=1, or Rn=13.
//    AND(register) A1 A8-36  -- Shouldn't parse when Rd=15 and S=1.
//    BIC(register) A1 A8-52  -- Shouldn't parse when Rd=15 and S=1.
//    EOR(register) A1 A8-96  -- Shouldn't parse when Rd=15 and S=1.
//    ORR(register) A1 A8-230 -- Shouldn't parse when Rd=15 and S=1.
//    RSB(register) A1 A8-286 -- Shouldn't parse when Rd=15 and S=1.
//    RSC(register) A1 A8-292 -- Shouldn't parse when Rd=15 and S=1.
//    SBC(register) A1 A8-304 -- Shouldn't parse when Rd=15 and S=1.
//    SUB(register) A1 A8-422 -- Shouldn't parse when Rd=15 and S=1, or Rn=13.
class Binary3RegisterImmedShiftedOp : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface m;
  static const ShiftTypeBits5To6Interface shift_type;
  static const Imm5Bits7To11Interface imm;
  static const RegBits12To15Interface d;
  static const RegBits16To19Interface n;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary3RegisterImmedShiftedOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // The shift value to use.
  uint32_t ShiftValue(const Instruction& i) const {
    return shift_type.DecodeImmShift(i, imm.value(i));
  }
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterImmedShiftedOp);
};

// Implements a Binary3RegisterImmedShiftedOp with the constraint
// that if either Rn, Rm, or Rd is Pc, the instruction is unpredictable.
class Binary3RegisterImmedShiftedOpRegsNotPc
    : public Binary3RegisterImmedShiftedOp {
 public:
  Binary3RegisterImmedShiftedOpRegsNotPc() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterImmedShiftedOpRegsNotPc);
};

// Models a 4-register-shifted binary operation of the form:
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
//
// if Rn, Rd, Rs, or Rm is R15, the instruction is unpredictable.
// NaCl disallows writing to PC to cause a jump.
//
// Implements:
//    ADC(register-shifted) A1 A8-18
//    ADD(register-shifted) A1 A8-26
//    AND(register-shifted) A1 A8-38
//    BIC(register-shifted) A1 A8-54
//    EOR(register-shifted) A1 A8-98
//    ORR(register-shifted) A1 A8-232
//    RSB(register-shifted) A1 A8-288
//    RSC(register-shifted) A1 A8-294
//    SBC(register-shifted) A1 A8-306
//    SUB(register-shifted) A1 A8-424
class Binary4RegisterShiftedOp : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface m;
  static const RegBits8To11Interface s;
  static const RegBits12To15Interface d;
  static const RegBits16To19Interface n;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary4RegisterShiftedOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary4RegisterShiftedOp);
};

// Models a 2-register immediate-shifted binary operation of the form:
// Op(S)<c> Rn, Rm {,<shift>}
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8 7| 6 5| 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// |  cond  |              | S|   Rn   |        |   imm5   |type|  |   Rm   |
// +--------+--------------+--+--------+--------+----------+----+--+--------+
// Definitions:
//    Rn - The first operand register.
//    Rm - The second operand that is (optionally) shifted.
//    shift = DecodeImmShift(type, imm5) is the amount to shift.
//
// Implements:
//    CMN(register) A1 A8-76
//    CMP(register) A1 A8-82
//    TEQ(register) A1 A8-450
//    TST(register) A1 A8-456
class Binary2RegisterImmedShiftedTest : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface m;
  static const ShiftTypeBits5To6Interface shift_type;
  static const Imm5Bits7To11Interface imm;
  static const RegBits16To19Interface n;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary2RegisterImmedShiftedTest() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

  // The shift value to use.
  uint32_t ShiftValue(const Instruction& i) const {
    return shift_type.DecodeImmShift(i, imm.value(i));
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary2RegisterImmedShiftedTest);
};

// Models a 3-register-shifted test operation of the form:
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
//
// if Rn, Rs, or Rm is R15, the instruction is unpredictable.
//
// Implements:
//    CMN(register-shifted) A1 A8-78
//    CMP(register-shifted) A1 A8-84
//    TEQ(register-shifted) A1 A8-452
//    TST(register-shifted) A1 A8-458
class Binary3RegisterShiftedTest : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface m;
  static const ShiftTypeBits5To6Interface shift_type;
  static const RegBits8To11Interface s;
  static const RegBits16To19Interface n;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary3RegisterShiftedTest() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterShiftedTest);
};

// Vector unary operator base class.
// Op<c> Rd, Rm, ...
// +--------+----------+--+------------+--------+------------+--+--+--------+
// |31302928|2726252423|22|212019181716|15141312|1110 9 8 7 6| 5| 4| 3 2 1 0|
// +--------+----------+--+------------+--------+------------+--+--+--------+
// |  cond  |          | D|            |   Vd   |            | M|  |   Vm   |
// +--------+----------+--+------------+--------+----+-------+--+--+--------+
// Rd - The destination register.
// Rm - The operand.
//
// d = D:Vd, m = M:Vm
//
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled, other than their index.
class VectorUnary2RegisterOpBase : public UncondDecoder {
 public:
  static const RegBits0To3Interface vm;
  static const Imm1Bit5Interface m;
  static const RegBits12To15Interface vd;
  static const Imm1Bit22Interface d;

  VectorUnary2RegisterOpBase() {}

  static uint32_t m_reg_index(const Instruction& i) {
    return (m.value(i) << 4) + vm.number(i);
  }

  static uint32_t d_reg_index(const Instruction& i) {
    return (d.value(i) << 4) + vd.number(i);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorUnary2RegisterOpBase);
};

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
//
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled, other than their index.
class Vector2RegisterMiscellaneous : public VectorUnary2RegisterOpBase {
 public:
  static const FlagBit6Interface q;
  static const Imm2Bits7To8Interface op;
  static const FlagBit10Interface f;
  static const Imm2Bits18To19Interface size;

  Vector2RegisterMiscellaneous() {}
};

// Defines a Vector2RegisterMiscellaneous which reverses a group of values.
//   safety := op + size >= 3 => UNDEFINED &
//             Q=1 & (Vd(0)=1 | Vm(0)=1) => UNDEFINED;
class Vector2RegisterMiscellaneous_RG : public Vector2RegisterMiscellaneous {
 public:
  Vector2RegisterMiscellaneous_RG() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector2RegisterMiscellaneous_RG);
};

// Defines a Vector2RegisterMiscellaneous that operates on 8, 16, and 32-bit
// values.
//   safety := size=11 => UNDEFINED &
//             Q=1 & (Vd(0)=1 | Vm(0)=1) => UNDEFINED;
class Vector2RegisterMiscellaneous_V8_16_32
    : public Vector2RegisterMiscellaneous {
 public:
  Vector2RegisterMiscellaneous_V8_16_32() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector2RegisterMiscellaneous_V8_16_32);
};

// Defines a Vector2RegisterMiscellaneous that operates on 8-bit values.
//   safety := size=~00 => UNDEFINED &
//             Q=1 & (Vd(0)=1 | Vm(0)=1) => UNDEFINED;
class Vector2RegisterMiscellaneous_V8
    : public Vector2RegisterMiscellaneous {
 public:
  Vector2RegisterMiscellaneous_V8() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector2RegisterMiscellaneous_V8);
};

// Defines a Vector2RegisterMiscellaneous that operates on 32-bit floating-point
// values.
//   safety := Q=1 & (Vd(0)=1 | Vm(0)=1) => UNDEFINED &
//             size=~10 => UNDEFINED;
class Vector2RegisterMiscellaneous_F32
    : public Vector2RegisterMiscellaneous {
 public:
  Vector2RegisterMiscellaneous_F32() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector2RegisterMiscellaneous_F32);
};

// Defines a Vector2RegisterMiscellaneous that swaps vectors of 8-bit values.
//   safety := d == m => UNKNOWN &
//             size=~00 => UNDEFINED &
//             Q=1 & (Vd(0)=1 | Vm(0)=1) => UNDEFINED;
class Vector2RegisterMiscellaneous_V8S
    : public Vector2RegisterMiscellaneous {
 public:
  Vector2RegisterMiscellaneous_V8S() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector2RegisterMiscellaneous_V8S);
};

// Defines a Vector2RegisterMiscellaneous that transposes vectors of 8,
// 16, and 32-bit values.
//   safety := d == m => UNKNOWN &
//             size=11 => UNDEFINED &
//             Q=1 & (Vd(0)=1 | Vm(0)=1) => UNDEFINED;
class Vector2RegisterMiscellaneous_V8_16_32T
    : public Vector2RegisterMiscellaneous {
 public:
  Vector2RegisterMiscellaneous_V8_16_32T() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector2RegisterMiscellaneous_V8_16_32T);
};

// Defines a Vector2RegisterMiscellaneous that interleaves vectors of 8,
// 16, and 32-bit values.
//   safety := d == m => UNKNOWN &
//             size=11 | (Q=0 & size=10) => UNDEFINED &
//             Q=1 & (Vd(0)=1 | Vm(0)=1) => UNDEFINED;
class Vector2RegisterMiscellaneous_V8_16_32I
    : public Vector2RegisterMiscellaneous {
 public:
  Vector2RegisterMiscellaneous_V8_16_32I() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector2RegisterMiscellaneous_V8_16_32I);
};

// Defines a Vector2RegisterMiscellaneous that narrows 16, 32, and 64-bit
// values.
//   safety := size=11 => UNDEFINED &
//             Vm(0)=1 => UNDEFINED;
class Vector2RegisterMiscellaneous_V16_32_64N
    : public Vector2RegisterMiscellaneous {
 public:
  Vector2RegisterMiscellaneous_V16_32_64N() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector2RegisterMiscellaneous_V16_32_64N);
};

// Defines a Vector2RegisterMiscellaneous that applies an immediate shift
// value to 8, 16, or 32-bit values, expanding the result to twice the
// length of the argument.
//   safety := size=11 | Vd(0)=1 => UNDEFINED;
class Vector2RegisterMiscellaneous_I8_16_32L
    : public Vector2RegisterMiscellaneous {
 public:
  Vector2RegisterMiscellaneous_I8_16_32L() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector2RegisterMiscellaneous_I8_16_32L);
};

// Defines a Vector2RegisterMiscellaneous that converts between floating-point
// and integer.
//   safety := Q=1 & (Vd(0)=1 | Vm(0)=1) => UNDEFINED &
//             size=~10 => UNDEFINED;
class Vector2RegisterMiscellaneous_CVT_F2I
    : public Vector2RegisterMiscellaneous {
 public:
  Vector2RegisterMiscellaneous_CVT_F2I() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector2RegisterMiscellaneous_CVT_F2I);
};

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
//
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled, other than their index.
class Vector2RegisterMiscellaneous_I16_32_64N
    : public VectorUnary2RegisterOpBase {
 public:
  static const FlagBit6Interface q;
  static const Imm2Bits6To7Interface op;
  static const Imm2Bits18To19Interface size;

  Vector2RegisterMiscellaneous_I16_32_64N() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector2RegisterMiscellaneous_I16_32_64N);
};

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
//
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled, other than their index.
class Vector2RegisterMiscellaneous_CVT_H2S : public VectorUnary2RegisterOpBase {
 public:
  static const FlagBit8Interface op;
  static const Imm2Bits18To19Interface size;

  Vector2RegisterMiscellaneous_CVT_H2S() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector2RegisterMiscellaneous_CVT_H2S);
};

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
//
// If Q=1 && Vd<0>=1 then UNDEFINED.
// if imm4='0000' then UNDEFINED.
// If imm4 not in {'xxx1', 'xx10', 'x100' } then UNDEFINED.
//
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled, other than their index.
class VectorUnary2RegisterDup : public VectorUnary2RegisterOpBase {
 public:
  static const FlagBit6Interface q;
  static const Imm4Bits16To19Interface imm4;

  VectorUnary2RegisterDup() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorUnary2RegisterDup);
};

// Vector binary operator base class.
// Op<c> Rd, Rn, Rm,...
// +--------+----------+--+----+--------+--------+--------+--+--+--+--+--------+
// |31302928|2726252423|22|2120|19181716|15141312|1110 9 8| 7| 6| 5| 4| 3 2 1 0|
// +--------+----------+--+----+--------+--------+--------+--+--+--+--+--------+
// |  cond  |          | D|    |   Vn   |   Vd   |        | N|  | M|  |   Vm   |
// +--------+----------+--+----+--------+--------+--------+--+--+--+--+--------+
// Rd - The destination register.
// Rn - The first operand.
// Rm - The second operand.
//
// d = D:Vd, n = N:Vn, m = M:Vm
//
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled, other than their index.
class VectorBinary3RegisterOpBase : public UncondDecoder {
 public:
  static const RegBits0To3Interface vm;
  static const Imm1Bit5Interface m;
  static const Imm1Bit7Interface n;
  static const RegBits12To15Interface vd;
  static const RegBits16To19Interface vn;
  static const Imm1Bit22Interface d;

  VectorBinary3RegisterOpBase() {}

  static uint32_t m_reg_index(const Instruction& i) {
    return (m.value(i) << 4) + vm.number(i);
  }

  static uint32_t n_reg_index(const Instruction& i) {
    return (n.value(i) << 4) + vn.number(i);
  }

  static uint32_t d_reg_index(const Instruction& i) {
    return (d.value(i) << 4) + vd.number(i);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterOpBase);
};

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
class VectorBinary2RegisterShiftAmount : public VectorBinary3RegisterOpBase {
 public:
  static const FlagBit6Interface q;
  static const Imm1Bit7Interface l;
  static const FlagBit8Interface op;
  static const Imm6Bits16To21Interface imm6;
  static const FlagBit24Interface u;

  VectorBinary2RegisterShiftAmount() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary2RegisterShiftAmount);
};

// Defines a VectorBinary2RegisterShiftAmount that shifts 8, 16, 32, and
// 64-bit integers.
//   safety := L:imm6=0000xxx => DECODER_ERROR &
//             Q=1 & (Vd(0)=1 | Vm(1)=1) => UNDEFINED;
class VectorBinary2RegisterShiftAmount_I
    : public VectorBinary2RegisterShiftAmount {
 public:
  VectorBinary2RegisterShiftAmount_I() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary2RegisterShiftAmount_I);
};

// Defines a VectorBinary2RegisterShiftAmount_I that shifts left with
// signed/unsigned integers.
//   safety := L:imm6=0000xxx => DECODER_ERROR &
//             Q=1 & (Vd(0)=1 | Vm(1)=1) => UNDEFINED &
//             U=0 & op=0 => UNDEFINED;
class VectorBinary2RegisterShiftAmount_ILS
    : public VectorBinary2RegisterShiftAmount_I {
 public:
  VectorBinary2RegisterShiftAmount_ILS() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary2RegisterShiftAmount_ILS);
};

// Defines a VectorBinary2RegisterShiftAmount that right shifts 16, 32,
// and 64-bit integers while narrowing the result to half the length of
// the arguments.
//   safety := imm6=000xxx => DECODER_ERROR &
//             Vm(0)=1 => UNDEFINED;
class VectorBinary2RegisterShiftAmount_N16_32_64R
    : public VectorBinary2RegisterShiftAmount {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64R() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary2RegisterShiftAmount_N16_32_64R);
};

// Defines a VectorBinary2RegisterShiftAmount_N16_32_64R that narrows
// signed/unsigned arguments.
//   safety := imm6=000xxx => DECODER_ERROR &
//             Vm(0)=1 => UNDEFINED &
//             U=0 & op=0 => DECODER_ERROR;
class VectorBinary2RegisterShiftAmount_N16_32_64RS
    : public VectorBinary2RegisterShiftAmount_N16_32_64R {
 public:
  VectorBinary2RegisterShiftAmount_N16_32_64RS() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary2RegisterShiftAmount_N16_32_64RS);
};

// Defines a VectorBinary2RegisterShiftAmount that left shifts 8, 16, and 32-bit
// integers and expands the result to twice the length of the arguments.
// Shift_amount == 0 => VMOVL
//
//   safety := imm6=000xxx => DECODER_ERROR &
//             Vd(0)=1 => UNDEFINED;
class VectorBinary2RegisterShiftAmount_E8_16_32L
    : public VectorBinary2RegisterShiftAmount {
 public:
  VectorBinary2RegisterShiftAmount_E8_16_32L() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary2RegisterShiftAmount_E8_16_32L);
};

// Defines a VectorBinary2RegisterShiftAmount that converts between
// floating-point and fixed-point.
//   safety := imm6=000xxx => DECODER_ERROR &
//             imm6=0xxxxx => UNDEFINED &
//             Q=1 & (Vd(0)=1 | Vm(0)=1)  => UNDEFINED;
class VectorBinary2RegisterShiftAmount_CVT
    : public VectorBinary2RegisterShiftAmount {
 public:
  VectorBinary2RegisterShiftAmount_CVT() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary2RegisterShiftAmount_CVT);
};

// Vector binary operator, 3 registers same length
// Op<c> Rd, Rn, Rm,...
// +--------+--+--+--+--+----+--------+--------+----+--+--+--+--+--+--+--------+
// |31..28|27..|24|23|22|2120|19181716|15141312|1110| 9| 8| 7| 6| 5| 4| 3 2 1 0|
// +------+----+--+--+--+----+--------+--------+----+--+--+--+--+--+--+--------+
// | cond |    | U|  | D|size|   Vn   |   Vd   |    |op|  | N| Q| M|  |   Vm   |
// +--------+--+--+--+--+----+--------+--------+----+--+--+--+--+--+--+--------+
// Rd - The destination register.
// Rn - The first operand.
// Rm - The second operand.
//
// d = D:Vd, n = N:Vn, m = M:Vm
//
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled, other than their index.
class VectorBinary3RegisterSameLength
    : public VectorBinary3RegisterOpBase {
 public:
  static const FlagBit6Interface q;
  static const FlagBit9Interface op;
  static const Imm2Bits20To21Interface size;
  static const FlagBit24Interface u;

  VectorBinary3RegisterSameLength() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterSameLength);
};

// Same as VectorBinary3RegisterSameLength, with 32/64 bit results,
// depending on the value of Q.
//   safety := Q=1 & (Vd(0)=1 | Vn(0)=1 | Vd(0)=1) => UNDEFINED;
class VectorBinary3RegisterSameLengthDQ
    : public VectorBinary3RegisterSameLength {
 public:
  VectorBinary3RegisterSameLengthDQ() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterSameLengthDQ);
};

// Same as VectorBinary3RegisterSameLengthDQ, but works on 8, 16 and
// 32-bit integers.
//   safety := Q=1 & (Vd(0)=1 | Vn(0)=1 | Vd(0)=1) => UNDEFINED &
//             size=11 => UNDEFINED;
class VectorBinary3RegisterSameLengthDQI8_16_32
    : public VectorBinary3RegisterSameLengthDQ {
 public:
  VectorBinary3RegisterSameLengthDQI8_16_32() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterSameLengthDQI8_16_32);
};

// Same as VectorBinary3RegisterSameLengthDQI, but works on
// 8-bit polynomials.
//   safety := Q=1 & (Vd(0)=1 | Vn(0)=1 | Vd(0)=1) => UNDEFINED &
//             size=~00 => UNDEFINED;
class VectorBinary3RegisterSameLengthDQI8P
    : public VectorBinary3RegisterSameLengthDQ {
 public:
  VectorBinary3RegisterSameLengthDQI8P() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterSameLengthDQI8P);
};

// Same as VectorBinary3RegisterSameLengthDQ, but works on 16 and 32-bit
// integers.
//   safety := Q=1 & (Vd(0)=1 | Vn(0)=1 | Vd(0)=1) => UNDEFINED &
//             (size=11 | size=00) => UNDEFINED;
class VectorBinary3RegisterSameLengthDQI16_32
    : public VectorBinary3RegisterSameLengthDQ {
 public:
  VectorBinary3RegisterSameLengthDQI16_32() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterSameLengthDQI16_32);
};

// Same as VectorBinary3RegisterSameLength, but operates on pairs of 32-bit
// values (sz = size(0)).
//   safety := sz=1 | Q=1 => UNDEFINED;
class VectorBinary3RegisterSameLength32P
    : public VectorBinary3RegisterSameLength {
 public:
  VectorBinary3RegisterSameLength32P() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterSameLength32P);
};

// Same as VectorBinary3RegisterSameLength, but uses 32 values,
// with 32/64 bit results, depending on the value of Q (sz = size(0)).
//   safety := Q=1 & (Vd(0)=1 | Vn(0)=1 | Vd(0)=1) => UNDEFINED &
//             sz=1 =>UNDEFINED;
class VectorBinary3RegisterSameLength32_DQ
    : public VectorBinary3RegisterSameLength {
 public:
  VectorBinary3RegisterSameLength32_DQ() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterSameLength32_DQ);
};

// Same as VectorBinary3RegisterSameLength, but works on D registers
// as 8, 16, or 32-bit integers.
//   safety := size=11 => UNDEFINED & Q=1 => UNDEFINED;
class VectorBinary3RegisterSameLengthDI
    : public VectorBinary3RegisterSameLength {
 public:
  VectorBinary3RegisterSameLengthDI() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterSameLengthDI);
};

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
//
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled, other than their index.
class VectorBinary3RegisterDifferentLength
    : public VectorBinary3RegisterOpBase {
 public:
  static const FlagBit6Interface q;
  static const FlagBit8Interface op;
  static const Imm2Bits20To21Interface size;
  static const FlagBit24Interface u;

  VectorBinary3RegisterDifferentLength() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterDifferentLength);
};

// Defines a VectorBinary3RegisterDifferentLength that allows 8, 16, and
// 32-bit integer arguments.
//   safety := size=11 => DECODER_ERROR &
//             Vd(0)=1 | (op=1 & Vn(0)=1) => UNDEFINED;
class VectorBinary3RegisterDifferentLength_I8_16_32
    : public VectorBinary3RegisterDifferentLength {
 public:
  VectorBinary3RegisterDifferentLength_I8_16_32() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VectorBinary3RegisterDifferentLength_I8_16_32);
};

// Defines a VectorBinary3RegisterDifferentLength that allows 16, 32, and
// 64-bit integer values (defined by 2*size).
//   safety := size=11 => DECODER_ERROR &
//             Vn(0)=1 | Vm(0)=1 => UNDEFINED;
class VectorBinary3RegisterDifferentLength_I16_32_64
    : public VectorBinary3RegisterDifferentLength {
 public:
  VectorBinary3RegisterDifferentLength_I16_32_64() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterDifferentLength_I16_32_64);
};

// Defines a VectorBinary3RegisterDifferentLength that allows 8, 16, and
// 32-bit integer argments, and long (i.e. double sized) results.
//   safety := size=11 => DECODER_ERROR &
//             Vd(0)=1 => UNDEFINED;
class VectorBinary3RegisterDifferentLength_I8_16_32L
    : public VectorBinary3RegisterDifferentLength {
 public:
  VectorBinary3RegisterDifferentLength_I8_16_32L() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterDifferentLength_I8_16_32L);
};

// Defines a VectorBinary3RegisterDifferentLength that allows 16 and
// 32-bit operands, and long (i.e. double sized) results.
//   safety := size=11 => DECODER_ERROR &
//             size=00 | Vd(0)=1 => UNDEFINED;
class VectorBinary3RegisterDifferentLength_I16_32L
    : public VectorBinary3RegisterDifferentLength {
 public:
  VectorBinary3RegisterDifferentLength_I16_32L() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterDifferentLength_I16_32L);
};

// Defines a VectorBinary3RegisterDifferentLength that allows 8-bit
// vector polynomial operations.
//   safety := size=11 => DECODER_ERROR &
//             U=1 | size=~00 => UNDEFINED &
//             Vd(0)=1 => UNDEFINED;
class VectorBinary3RegisterDifferentLength_P8
    : public VectorBinary3RegisterDifferentLength {
 public:
  VectorBinary3RegisterDifferentLength_P8() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterDifferentLength_P8);
};

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
class VectorBinary2RegisterScalar : public VectorBinary3RegisterOpBase {
 public:
  static const FlagBit8Interface f;
  static const FlagBit10Interface op;
  static const Imm2Bits20To21Interface size;
  static const FlagBit24Interface q;

  VectorBinary2RegisterScalar() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary2RegisterScalar);
};

// Defines a VectorBinary2RegisterScalar that allows 16 and 32-bit integer
// arguments.
//   index := M:Vm(3) if size=11 else M;
//   safety := size=11 => DECODER_ERROR &
//             size=00 => UNDEFINED &
//             Q=1 & (Vd(0)=1 | Vn(0)=1) => UNDEFINED;
class VectorBinary2RegisterScalar_I16_32 : public VectorBinary2RegisterScalar {
 public:
  VectorBinary2RegisterScalar_I16_32() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary2RegisterScalar_I16_32);
};

// Defines a VectorBinary2RegisterScalar that allows 16 and 32-bit integer
// arguments, and long (i.e. double sized) results.
//   safety := size=11 => DECODER_ERROR &
//             (size=00 | Vd(0)=1) => UNDEFINED;
class VectorBinary2RegisterScalar_I16_32L : public VectorBinary2RegisterScalar {
 public:
  VectorBinary2RegisterScalar_I16_32L() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary2RegisterScalar_I16_32L);
};

// Defines a VectorBinary2RegisterScalar that allows 32-bit floating point
//  values.
//   safety := size=11 => DECODER_ERROR &
//             (size=00 | size=01) => UNDEFINED &
//             Q=1 & (Vd(0)=1 | Vn(0)=1) => UNDEFINED;
class VectorBinary2RegisterScalar_F32 : public VectorBinary2RegisterScalar {
 public:
  VectorBinary2RegisterScalar_F32() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary2RegisterScalar_F32);
};

// Vector binary operator with imm4 value.
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
//
// Q=1 implies quadword operation. Otherwise doubleword.
//
// if Q=1 && (Vd<0>=1 || Vn<0>==1 || Vm<0>==1) then UNDEFINED;
// if Q=0 && imm4<3>==1 then UNDEFINED:
class VectorBinary3RegisterImmOp : public VectorBinary3RegisterOpBase {
 public:
  static const FlagBit6Interface q;
  static const Imm4Bits8To11Interface imm;

  VectorBinary3RegisterImmOp() {}
  virtual SafetyLevel safety(Instruction i) const;
};

// Vector 1 register immediate SIMD instruction.
// +--------------+--+--+--+------+------+--------+--------+--+--+--+--+-----+
// |31302928272625|24|23|22|212019|181716|15141312|1110 9 8| 7| 6| 5| 4| 3..0|
// +--------------+--+--+--+------+------+--------+--------+--+--+--+--+-----+
// |              | i|  | D|      | imm3 |   Vd   |  cmode |  | Q|op|  | imm4|
// +--------------+--+--+--+------+------+--------+----+---+--+--+--+--+-----+
// d := D:Vd;
// imm64 := AdvSIMDExpandImm(op, cmode, i:imm3:imm4);
class Vector1RegisterImmediate : public UncondDecoder {
 public:
  static const Imm4Bits0To3Interface imm4;
  static const FlagBit5Interface op;
  static const FlagBit6Interface q;
  static const Imm4Bits8To11Interface cmode;
  static const RegBits12To15Interface vd;
  static const Imm3Bits16To18Interface imm3;
  static const Imm1Bit22Interface d;
  static const Imm1Bit24Interface i;

  Vector1RegisterImmediate() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector1RegisterImmediate);
};

// Defines a Vector1RegisterImmediate that implements a VMOV instruction.
//   safety := op=0 & cmode(0)=1 & cmode(3:2)=~11 => DECODER_ERROR &
//             op=1 & cmode=~1110 => DECODER_ERROR &
//             Q=1 & Vd(0)=1 => UNDEFINED;
class Vector1RegisterImmediate_MOV : public Vector1RegisterImmediate {
 public:
  Vector1RegisterImmediate_MOV() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector1RegisterImmediate_MOV);
};

// Defines a Vector1RegisterImmediate that implements a VORR or VBIC
// instruction.
//   safety := cmode(0)=0 | cmode(3:2)=11 => DECODER_ERROR &
//             Q=1 & Vd(0)=1 => UNDEFINED;
class Vector1RegisterImmediate_BIT : public Vector1RegisterImmediate {
 public:
  Vector1RegisterImmediate_BIT() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector1RegisterImmediate_BIT);
};

// Defines a Vector1RegisterImmediate that implements a VMVN instruction.
//   safety := (cmode(0)=1 & cmode(3:2)=~11) | cmode(3:1)=111 => DECODER_ERROR &
//             Q=1 & Vd(0)=1 => UNDEFINED;
class Vector1RegisterImmediate_MVN : public Vector1RegisterImmediate {
 public:
  Vector1RegisterImmediate_MVN() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Vector1RegisterImmediate_MVN);
};

// Vector table lookup
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
//
// if n+length > 32 then UNPREDICTABLE
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled.
class VectorBinary3RegisterLookupOp : public VectorBinary3RegisterOpBase {
 public:
  static const FlagBit6Interface op_flag;
  static const Imm2Bits8To9Interface len;

  VectorBinary3RegisterLookupOp() {}
  virtual SafetyLevel safety(Instruction i) const;

  static uint32_t length(const Instruction& i) {
    return len.value(i) + 1;
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorBinary3RegisterLookupOp);
};

// Models of use of a general purpose register by a vfp instruction.
// Op<c> ..., <Rt>, ...
// +--------+------------------------+--------+--------+---------------+
// |31302928|272625242322212019181716|15141312|1120 9 8|7 6 5 4 3 2 1 0|
// +--------+------------------------+--------+--------+---------------+
// |  cond  |                        |   Rt   | coproc |               |
// +--------+------------------------+--------+--------+---------------+
//
// If t=15 then UNPREDICTABLE
class VfpUsesRegOp : public CondVfpOp {
 public:
  static const RegBits12To15Interface t;

  // Methods for class.
  VfpUsesRegOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VfpUsesRegOp);
};

// Models a define of a general purpose register from some vfp data register.
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
class VfpMrsOp : public CondVfpOp {
 public:
  static const RegBits12To15Interface t;

  // Methods for class.
  VfpMrsOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VfpMrsOp);
};

// Models a move to/from a vfp register to the corresponding
// core register.
// Op<c> <Sn>, <Rt>
// Op<c> <Rt>, <Sn>
// +--------+--------------+--+--------+--------+--------+--+--------------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8| 7| 6 5 4 3 2 1 0|
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
// Note: We don't model Register S[Vn:N] since it can't affect NaCl validation.
class MoveVfpRegisterOp : public CondVfpOp {
 public:
  static const RegBits12To15Interface t;
  static const UpdatesArmRegisterBit20Interface to_arm_reg;

  // Methods for class.
  MoveVfpRegisterOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MoveVfpRegisterOp);
};

// Models a MoveVfpRegisterOp with added constraint:
// When bits(23:21):bits(6:5) = '10x00' or 'x0x10', the instruction is
// undefined. That is, these bits are used to define 8, 16, and 32 bit selectors
// within Vn.
class MoveVfpRegisterOpWithTypeSel : public MoveVfpRegisterOp {
 public:
  static const Imm3Bits21To23Interface opc1;
  static const ShiftTypeBits5To6Interface opc2;

  // Methods for class
  MoveVfpRegisterOpWithTypeSel() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MoveVfpRegisterOpWithTypeSel);
};

// Move either 2*S or 1*D FPR register(s) to/from 2*GPRs.
class MoveDoubleVfpRegisterOp : public MoveVfpRegisterOp {
 public:
  static const RegBits16To19Interface t2;
  static const RegBits0To3Interface vm;
  static const Imm1Bit5Interface m;

  MoveDoubleVfpRegisterOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  static bool IsSinglePrecision(Instruction i) { return !i.Bit(8); }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MoveDoubleVfpRegisterOp);
};

// Models a move from a core register to every element of an FP register.
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
class DuplicateToAdvSIMDRegisters : public CondAdvSIMDOp {
 public:
  static const Imm1Bit5Interface e;
  static const RegBits12To15Interface t;
  static const RegBits16To19Interface vd;
  static const FlagBit21Interface is_two_regs;
  static const Imm1Bit22Interface b;

  // Methods for class
  DuplicateToAdvSIMDRegisters() {}
  virtual SafetyLevel safety(Instruction i) const;

  uint32_t be_value(const Instruction& i) const {
    return (b.value(i) << 1) | e.value(i);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(DuplicateToAdvSIMDRegisters);
};

// Models an advanced SIMD vector load/store multiple elements.
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
//
// Note: The main reason that this class can be used for both loads and
// stores is because the source/target register of the store/load is D:Vd
// (a FPR), rather than any general purpose register. As a result, "defs"
// and "uses" are not affected by changes to D:Vd.
class VectorLoadStoreMultiple : public UncondDecoder {
 public:
  static const RegBits0To3Interface rm;
  static const Imm2Bits4To5Interface align;
  static const Imm2Bits6To7Interface size;
  static const Imm4Bits8To11Interface type;
  static const RegBits12To15Interface vd;
  static const RegBits16To19Interface rn;
  static const Imm1Bit22Interface d;

  virtual RegisterList defs(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual Register base_address_register(Instruction i) const;

  // Computes the value of D:Vd.
  inline uint32_t d_reg_index(Instruction i) const {
    return (d.value(i) << 4) | vd.number(i);
  }

  // Computes the value of register_index.
  bool is_register_index(Instruction i) const {
    return !RegisterList().Add(Register::Pc()).Add(Register::Sp())
        .Contains(rm.reg(i));
  }

  // Computes the value of wback.
  bool is_wback(Instruction i) const {
    return !rm.reg(i).Equals(Register::Pc());
  }

 protected:
  // Protected, since one shouldn't instantiate directly.
  VectorLoadStoreMultiple() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadStoreMultiple);
  friend class nacl_arm_test::VectorLoadStoreMultipleTester;
};

// Implements a VLD1/VST1 multiple instruction.
//   regs := 1 if type=0111 else
//           2 if type=1010 else
//           3 if type=0110 else
//           4 if type=0010 else
//           0;  # Error value.
//   safety := type=0111 & align(1)=1 => UNDEFINED &
//             type=1010 & align=11 => UNDEFINED &
//             type=0110 & align(1)=1 => UNDEFINED &
//             not type in bitset {0111, 1010, 0110, 0010} => DECODER_ERROR &
//             n == Pc | d + regs > 32 => UNPREDICTABLE;
class VectorLoadStoreMultiple1 : public VectorLoadStoreMultiple {
 public:
  VectorLoadStoreMultiple1() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadStoreMultiple1);
};

// Implements a VLD2/VST2 multiple instruction.
//   regs := 1 if type in bitset {1000, 1001} else 2;
//   inc  := 1 if type=1000 else 2;
//   d2 := d + inc;
//   safety := size=11 => UNDEFINED &
//             type in bitset {1000, 1001} & align=11 => UNDEFINED &
//             not type in bitset {1000, 1001, 0011} => DECODER_ERROR &
//             n == Pc | d2 + regs > 32 => UNPREDICTABLE;
class VectorLoadStoreMultiple2 : public VectorLoadStoreMultiple {
 public:
  VectorLoadStoreMultiple2() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadStoreMultiple2);
};

// Implements a VLD3/VST3 multiple instruction.
//   inc := 1 if type=0100 else 2;
//   alignment := 1 if align(0)=0 else 8;
//   d2 := d + inc; d3 := d2 + inc;
//   safety := size=11 | align(1)=1 => UNDEFINED &
//             not type in bitset {0100, 0101} => DECODER_ERROR &
//             n == Pc | d3 > 31 => UNPREDICTABLE;
class VectorLoadStoreMultiple3 : public VectorLoadStoreMultiple {
 public:
  VectorLoadStoreMultiple3() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadStoreMultiple3);
};

// Implements a VLD4/VST4 multiple instruction.
//   inc := 1 if type=0000 else 2;
//   d2 := d + inc; d3 := d2 + inc; d4 := d3 + inc;
//   safety := size=11 => UNDEFINED &
//             not type in bitset {0000, 0001} => DECODER_ERROR &
//             n == Pc | d4 > 31 => UNPREDICTABLE;
class VectorLoadStoreMultiple4 : public VectorLoadStoreMultiple {
 public:
  VectorLoadStoreMultiple4() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadStoreMultiple4);
};

// Models an advanced SIMD vector load/store single elements to one lane.
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
//
// Note: The main reason that this class can be used for both loads and
// stores is because the source/target register of the store/load is D:Vd
// (a FPR), rather than any general purpose register. As a result, "defs"
// and "uses" are not affected by changes to D:Vd.
class VectorLoadStoreSingle : public UncondDecoder {
 public:
  static const RegBits0To3Interface rm;
  static const Imm4Bits4To7Interface index_align;
  static const Imm2Bits10To11Interface size;
  static const RegBits12To15Interface vd;
  static const RegBits16To19Interface rn;
  static const Imm1Bit22Interface d;

  virtual RegisterList defs(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual Register base_address_register(Instruction i) const;

  // computes the value of d = D:Vd;
  inline uint32_t d_reg_index(Instruction i) const {
    return (d.value(i) << 4) | vd.number(i);
  }

  // computes the value of register_index.
  bool is_register_index(Instruction i) const {
    return !RegisterList().Add(Register::Pc()).Add(Register::Sp())
        .Contains(rm.reg(i));
  }

  // Computes the value of wback.
  bool is_wback(Instruction i) const {
    return !rm.reg(i).Equals(Register::Pc());
  }

  // Computes the value inc.
  uint32_t inc(Instruction i) const;

 protected:
  // Protected, since one shouldn't instantiate directly.
  VectorLoadStoreSingle() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadStoreSingle);
  friend class nacl_arm_test::VectorLoadStoreSingleTester;
};

// Implements a VLD1/VST1 single instruction to one lane.
//   alignment := 1 if size=00 else
//                (1 if index_align(0)=0 else 2) if size=01 else
//                (1 if index_align(1:0)=00 else 4) if size=10 else
//                0;  # error value.
//   safety := size=11 => UNDEFINED &
//             size=00 & index_align(0)=~0 => UNDEFINED &
//             size=01 & index_align(1)=~0 => UNDEFINED &
//             size=10 & index_align(2)=~0 => UNDEFINED &
//             size=10 & index_align(1:0)=~00 => UNDEFINED &
//             n == Pc => UNPREDICTABLE;
class VectorLoadStoreSingle1 : public VectorLoadStoreSingle {
 public:
  VectorLoadStoreSingle1() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadStoreSingle1);
};

// Implements a VLD2/VST2 single instruction to one lane.
//   alignment := (1 if index_align(0)=0 else 2) if size=00 else
//                (1 if index_align(0)=0 else 4) if size=01 else
//                (1 if index_align(0)=0 else 8) if size=10 else
//                0;  # error value.
//   d2 := d + inc;
//   safety := size=11 => UNDEFINED &
//             size=10 & index_align(1)=~0 => UNDEFINED &
//             n == Pc | d2 > 31 => UNPREDICTABLE;
class VectorLoadStoreSingle2 : public VectorLoadStoreSingle {
 public:
  VectorLoadStoreSingle2() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadStoreSingle2);
};

// Implements a VLD3/VST3 single instruction to one lane.
//   d2 := d + inc; d3 := d2 + inc;
//   alignment := 1;
//   safety := size=11 => UNDEFINED &
//             size=00 & index_align(0)=~0 => UNDEFINED &
//             size=01 & index_align(0)=~0 => UNDEFINED &
//             size=10 & index_align(1:0)=~00 => UNDEFINED &
//             n == Pc | d3 > 31 => UNPREDICTABLE;
class VectorLoadStoreSingle3 : public VectorLoadStoreSingle {
 public:
  VectorLoadStoreSingle3() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadStoreSingle3);
};

// Implements a VLD4/VST4 single instruction to one lane.
//   d2 := d + inc; d3 := d2 + inc; d4 := d3 + inc;
//   alignment := (1 if index_align(0)=0 else 4) if size=00 else
//                (1 if index_align(0)=0 else 8) if size=01 else
//                (1 if index_align(1:0)=00 else 4 << index_align(1:0))
//                   if size=10 else
//                0;  # error value.
//   safety := size=11 => UNDEFINED &
//             size=10 & index_align(1:0)=11 => UNDEFINED &
//             n == Pc | d4 > 31 => UNPREDICTABLE;
class VectorLoadStoreSingle4 : public VectorLoadStoreSingle {
 public:
  VectorLoadStoreSingle4() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadStoreSingle4);
};

// Implements a VLD1, VLD2, VLD3, VLD4 single instruction to all lanes.
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
class VectorLoadSingleAllLanes : public UncondDecoder {
 public:
  static const RegBits0To3Interface rm;
  static const FlagBit4Interface a;
  static const FlagBit5Interface t;
  static const Imm2Bits6To7Interface size;
  static const RegBits12To15Interface vd;
  static const RegBits16To19Interface rn;
  static const Imm1Bit22Interface d;

  virtual RegisterList defs(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual Register base_address_register(Instruction i) const;

  // computes the value of d = D:Vd;
  inline uint32_t d_reg_index(Instruction i) const {
    return (d.value(i) << 4) | vd.number(i);
  }

  // computes the value of register_index.
  bool is_register_index(Instruction i) const {
    return !RegisterList().Add(Register::Pc()).Add(Register::Sp())
        .Contains(rm.reg(i));
  }

  // Computes the value of wback.
  bool is_wback(Instruction i) const {
    return !rm.reg(i).Equals(Register::Pc());
  }

 protected:
  // Protected, since one shouldn't instantiate directly.
  VectorLoadSingleAllLanes() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadSingleAllLanes);
  friend class nacl_arm_test::VectorLoadSingleAllLanesTester;
};

// Implements a VLD1 single instruction to all lanes.
//   alignment := 1 if a=0 else ebytes;
//   regs := 1 if T=0 else 2;
//   safety := size=11 | (size=00 & a=1) => UNDEFINED &
//             n == Pc | d + regs > 32 => UNPREDICTABLE;
class VectorLoadSingle1AllLanes : public VectorLoadSingleAllLanes {
 public:
  VectorLoadSingle1AllLanes() {}
  virtual SafetyLevel safety(Instruction i) const;

  // Computes the value of regs
  uint32_t num_regs(Instruction i) const {
    return t.IsDefined(i) ? 2 : 1;
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadSingle1AllLanes);
};

// Implements a VLD2 single instruction to all lanes.
//   alignment := 1 if a=0 else 2 * ebytes;
//   inc := 1 if T=0 else 2;
//   d2 := d + inc;
//   safety := size=11 => UNDEFINED &
//             n == Pc | d2 > 31 => UNPREDICTABLE;
class VectorLoadSingle2AllLanes : public VectorLoadSingleAllLanes {
 public:
  VectorLoadSingle2AllLanes() {}
  virtual SafetyLevel safety(Instruction i) const;

  // Computes the value of inc.
  uint32_t inc(Instruction i) const {
    return t.IsDefined(i) ? 2 : 1;
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadSingle2AllLanes);
};

// Implements a VLD3 single instruction to all lanes.
//   inc := 1 if T=0 else 2;
//   d2 := d + inc; d3 := d2 + inc;
//   safety := size=11 | a=1 => UNDEFINED &
//             n == Pc | d3 > 31 => UNPREDICTABLE;
class VectorLoadSingle3AllLanes : public VectorLoadSingleAllLanes {
 public:
  VectorLoadSingle3AllLanes() {}
  virtual SafetyLevel safety(Instruction i) const;

  // Computes the value of inc.
  uint32_t inc(Instruction i) const {
    return t.IsDefined(i) ? 2 : 1;
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadSingle3AllLanes);
};

// Implements a VLD4 single instruction to all lanes.
//   alignment := 16 if size=11 else
//                (1 if a=0 else 8) if size=10 else
//                (1 if a=0 else 4 * ebytes);
//   inc := 1 if T=0 else 2;
//   d2 := d + inc; d3 := d2 + inc; d4 := d3 + inc;
//   safety := size=11 & a=0 => UNDEFINED &
//             n == Pc | d4 > 31 => UNPREDICTABLE;
class VectorLoadSingle4AllLanes : public VectorLoadSingleAllLanes {
 public:
  VectorLoadSingle4AllLanes() {}
  virtual SafetyLevel safety(Instruction i) const;

  // Computes the value of inc.
  uint32_t inc(Instruction i) const {
    return t.IsDefined(i) ? 2 : 1;
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoadSingle4AllLanes);
};

// Models a synchronization barrier instruction.
// Op #<option>
// +--------+------------------------------------------------+--------+
// |31302928|272625242322212019181716151413121110 9 8 7 6 5 4| 3 2 1 0|
// +--------+------------------------------------------------+--------+
// |  cond  |                                                | option |
// +--------+------------------------------------------------+--------+
// cond=1111
class BarrierInst : public UncondDecoder {
 public:
  static const Imm4Bits0To3Interface option;

  // Methods for class.
  BarrierInst() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BarrierInst);
};

// Implements a data synchronization barrier instruction.
//
// option in {1111, 1110, 1011, 1010, 0111, 0110, 0011, 0010}
//
// Note: While the ARM v7 manual states that only option SY(1111) is guaranteed
// to be implemented, and the remaining 7 option values are IMPLEMENTATION
// DEFINED, the NaCl compilers generate more than 1111. As a result, we
// are currently allowing all 8 option values.
// TODO(karl) Figure out if we should allow all 8 values, or only 1111.
class DataBarrier : public BarrierInst {
 public:
  DataBarrier() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(DataBarrier);
};

// Implements an instruction synchronization barrier instruction.
//
// option = 1111 (All other values are reserved).
class InstructionBarrier : public BarrierInst {
 public:
  InstructionBarrier() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(InstructionBarrier);
};

}  // namespace nacl_arm_dec

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_BASELINE_CLASSES_H_
