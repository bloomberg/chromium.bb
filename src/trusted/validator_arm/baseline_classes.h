/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_BASELINE_CLASSES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_BASELINE_CLASSES_H_

#include "native_client/src/trusted/validator_arm/inst_classes.h"

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

// Models an unconditional nop.
// Nop<c>
// +--------+--------------------------------------------------------+
// |31302918|272625242322212019181716151413121110 9 8 7 6 5 4 3 2 1 0|
// +--------+--------------------------------------------------------+
// |  cond  |                                                        |
// +--------+--------------------------------------------------------+
//
// if cond!=1111 then UNDEFINED.
class UncondNop : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  UncondNop() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(UncondNop);
};


// Models an unconditional nop that is always unsafe (i.e. one of:
// Forbidden, Undefined, Deprecated, and Unpredictable).
class UnsafeUncondNop : public UncondNop {
 public:
  explicit UnsafeUncondNop(SafetyLevel safety)
      : UncondNop(), safety_(safety) {}
  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return safety_;
  }

 private:
  SafetyLevel safety_;  // The unsafe value to return.
  NACL_DISALLOW_COPY_AND_ASSIGN(UnsafeUncondNop);
};


// Models an unconditional forbidden UnsafeCondNop.
class ForbiddenUncondNop : public UnsafeUncondNop {
 public:
  explicit ForbiddenUncondNop() : UnsafeUncondNop(FORBIDDEN) {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ForbiddenUncondNop);
};

// Models a (conditional) nop.
// Nop<c>
// +--------+--------------------------------------------------------+
// |31302918|272625242322212019181716151413121110 9 8 7 6 5 4 3 2 1 0|
// +--------+--------------------------------------------------------+
// |  cond  |                                                        |
// +--------+--------------------------------------------------------+
class CondNop : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  CondNop() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(CondNop);
};

// Models a (conditional) nop that is always unsafe (i.e. one of:
// Forbidden, Undefined, Deprecated, and Unpredictable).
class UnsafeCondNop : public CondNop {
 public:
  explicit UnsafeCondNop(SafetyLevel safety)
      : CondNop(), safety_(safety) {}
  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return safety_;
  }

 private:
  SafetyLevel safety_;  // The unsafe value to return.
  NACL_DISALLOW_COPY_AND_ASSIGN(UnsafeCondNop);
};

// Models a (conditional) forbidden UnsafeCondNop.
class ForbiddenCondNop : public UnsafeCondNop {
 public:
  explicit ForbiddenCondNop() : UnsafeCondNop(FORBIDDEN) {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ForbiddenCondNop);
};

// Implements a VFP operation on instructions of the form;
// +--------+--------------------------------+--------+----------------+
// |31302928|27262524232221201918171615141312|1110 9 8| 7 6 5 4 3 2 1 0|
// +--------+--------------------------------+--------+----------------+
// |  cond  |                                | coproc |                |
// +--------+--------------------------------+--------+----------------+
// A generic VFP instruction that (by default) only effects vector
// register banks. Hence, they do not change general purpose registers.
class CondVfpOp : public CoprocessorOp {
 public:
  // Accessor to non-vector register fields.
  static const ConditionBits28To31Interface cond;

  CondVfpOp() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(CondVfpOp);
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
//           mask<0>=1, the GE bits (3:0 and 19:16) are updated.
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
// if the constant is 0x7777.
class BreakPointAndConstantPoolHead : public Immediate16Use {
 public:
  BreakPointAndConstantPoolHead() {}
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
  static const RegMBits0To3Interface m;
  static const UpdatesLinkRegisterBit5Interface link_register;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  BranchToRegister() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual Register branch_target_register(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BranchToRegister);
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
class Unary1RegisterImmediateOp : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const Imm12Bits0To11Interface imm12;
  static const RegDBits12To15Interface d;
  static const Imm4Bits16To19Interface imm4;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Unary1RegisterImmediateOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

  // The immediate value stored in the instruction.
  uint32_t ImmediateValue(const Instruction& i) const {
    return (imm4.value(i) << 12) | imm12.value(i);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterImmediateOp);
};

// Models a 2-register binary operation with two immediate values
// defining a bit range.
// Op<c> Rd, Rn, #<lsb>, #width
// +--------+--------------+----------+--------+----------+------+--------+
// |31302928|27262524232221|2019181716|15141312|1110 9 8 7| 6 5 4| 3 2 1 0|
// +--------+--------------+----------+--------+----------+------+--------+
// |  cond  |              |    imm5  |   Rd   |    lsb   |      |   Rn   |
// +--------+--------------+----------+--------+----------+------+--------+
// Definitions:
//   Rd = The destination register.
//   Rn = The first operand
//   lsb = The least significant bit to be used.
//   imm5 = Constant where either:
//      width = imm5 + 1 = The width of the bitfield.
//      msb = imm5 = The most significant bit to be used.
//
// If Rd=R15, the instruction is unpredictable.
//
// NaCl disallows writing Pc to cause a jump.
//
// Note: The instruction SBFX (an instance of this instruction) sign extends.
// Hence, we do not assume that this instruction can be used to clear bits.
class Binary2RegisterBitRange : public ClassDecoder {
 public:
  // Interface for components of the instruction.
  static const RegNBits0To3Interface n;
  static const Imm5Bits7To11Interface lsb;
  static const RegDBits12To15Interface d;
  static const Imm5Bits16To20Interface imm5;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary2RegisterBitRange() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary2RegisterBitRange);
};

// A Binary2RegisterBitRange with the additional constraint that
// if Rn=R15, the instruction is unpredictable.
class Binary2RegisterBitRangeNotRnIsPc : public Binary2RegisterBitRange {
 public:
  Binary2RegisterBitRangeNotRnIsPc() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary2RegisterBitRangeNotRnIsPc);
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
class Binary2RegisterImmediateOp : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const Imm12Bits0To11Interface imm;
  static const RegDBits12To15Interface d;
  static const RegNBits16To19Interface n;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary2RegisterImmediateOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary2RegisterImmediateOp);
};

// Models a Binary2RegisterImmediateOp that is used to mask a memory
// address to the actual limits of user's memory, using the immediate
// value. We use this to capture ImmediateBic.
class MaskedBinary2RegisterImmediateOp : public Binary2RegisterImmediateOp {
 public:
  MaskedBinary2RegisterImmediateOp() : Binary2RegisterImmediateOp() {}
  // TODO(karl): find out why we added this so that we allowed an
  // override on NaCl restriction that one can write to r15.
  // virtual SafetyLevel safety(Instruction i) const;
  virtual bool clears_bits(Instruction i, uint32_t mask) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MaskedBinary2RegisterImmediateOp);
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
//    const = ARMExpandImm_C(imm12, ASPR.C)
//
// Implements:
//    CMN(immediate) A1 A8-74
//    CMP(immediate) A1 A8-80
//    TEQ(immediate) A1 A8-448
//    TST(immediate) A1 A8-454 - Note: See class TestImmediate.
class BinaryRegisterImmediateTest : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const Imm12Bits0To11Interface imm;
  static const RegNBits16To19Interface n;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  BinaryRegisterImmediateTest() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

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
  static const RegMBits0To3Interface m;
  static const RegDBits12To15Interface d;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Unary2RegisterOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

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
  static const RegNBits0To3Interface n;
  static const RegMBits8To11Interface m;
  static const RegDBits12To15Interface d;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary3RegisterOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

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
  static const RegTBits12To15Interface t;
  static const RegNBits16To19Interface n;
  static const ConditionBits28To31Interface cond;

  LoadExclusive2RegisterOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
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
  static const RegT2Bits12To15Interface t2;

  LoadExclusive2RegisterDoubleOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadExclusive2RegisterDoubleOp);
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
  static const RegTBits12To15Interface t;
  static const RegNBits16To19Interface n;
  static const WritesBit21Interface writes;
  static const AddOffsetBit23Interface direction;
  static const PrePostIndexingBit24Interface indexing;
  static const ConditionBits28To31Interface cond;

  LoadStore2RegisterImm8Op() : ClassDecoder(), is_load_(false) {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual Register base_address_register(const Instruction i) const;

  bool HasWriteBack(const Instruction i) const {
    return indexing.IsPostIndexing(i) || writes.IsDefined(i);
  }

 protected:
  bool is_load_;  // true if load (rather than store).

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore2RegisterImm8Op);
};

// Defines the virtuals for a load immediate instruction.
class Load2RegisterImm8Op : public LoadStore2RegisterImm8Op {
 public:
  Load2RegisterImm8Op() : LoadStore2RegisterImm8Op() {
    is_load_ = true;
  }
  virtual RegisterList defs(Instruction i) const;
  virtual bool offset_is_immediate(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load2RegisterImm8Op);
};

// Defines the virtuals for a store immediate instruction.
class Store2RegisterImm8Op : public LoadStore2RegisterImm8Op {
 public:
  Store2RegisterImm8Op() : LoadStore2RegisterImm8Op() {
    is_load_ = false;
  }
  virtual RegisterList defs(Instruction i) const;

 protected:
  NACL_DISALLOW_COPY_AND_ASSIGN(Store2RegisterImm8Op);
};

// Models a 2-register immediate load/store operation where the source/target
// is double wide (i.e.  Rt and Rt2).
class LoadStore2RegisterImm8DoubleOp
    : public LoadStore2RegisterImm8Op {
 public:
  // Interface for components in the instruction (and not inherited).
  static const RegT2Bits12To15Interface t2;

  LoadStore2RegisterImm8DoubleOp()
      : LoadStore2RegisterImm8Op() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore2RegisterImm8DoubleOp);
};

// Defines the virtuals for a load immediate double instruction.
class Load2RegisterImm8DoubleOp
    : public LoadStore2RegisterImm8DoubleOp {
 public:
  Load2RegisterImm8DoubleOp() : LoadStore2RegisterImm8DoubleOp() {
    is_load_ = true;
  }
  virtual RegisterList defs(Instruction i) const;
  virtual bool offset_is_immediate(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load2RegisterImm8DoubleOp);
};

// Defines the virtuals for a store immediate double instruction.
class Store2RegisterImm8DoubleOp
    : public LoadStore2RegisterImm8DoubleOp {
 public:
  Store2RegisterImm8DoubleOp()
      : LoadStore2RegisterImm8DoubleOp() {
    is_load_ = false;
  }
  virtual RegisterList defs(Instruction i) const;

 protected:
  NACL_DISALLOW_COPY_AND_ASSIGN(Store2RegisterImm8DoubleOp);
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
// if wback && (Rn=15 or Rn=Rt) then unpredictable.
// NaCl disallows writing to PC.
//
// Note: NaCl disallows Rt=PC for stores (not just loads), even
// though it isn't a requirement of the corresponding baseline
// classees. This is done so that StrImmediate (in the actual class
// decoders) behave the same as instances of this. This simplifies
// what we need to model in actual classes.
//
// For store register immediate (Str rule 194, A1 on page 384):
// if Rn=Sp && P=1 && U=0 && W=1 && imm12=4, then PUSH (A8.6.123, A2 on A8-248).
//    Note: This is just a special case instruction that behaves like a
//    Store2RegisterImm12Op instruction. That is, the push saves the
//    value of Rt at Sp-4, and then decrements sp by 4. Since this doesn't
//    effect the NaCl constraints we need for such stores, we do not model
//    it as a special instruction.
//
// For load register immediate (Ldr rule 59, A1 on page 122):
// If Rn=Sp && P=0 && U=1 && W=0 && imm12=4, then POP ().
//    Note: This is just a speciial case instruction that behaves like a
//    Load2RegisterImm12Op instruction. That is, the pop loads from the
//    top of stack the value of Rt, and then increments the stack by 4. Since
//    this doesn't effect the NaCl constraints we need for such loads, we do
//    not model it as a special instruction.
class LoadStore2RegisterImm12Op : public ClassDecoder {
 public:
  static const Imm12Bits0To11Interface imm12;
  static const RegTBits12To15Interface t;
  static const RegNBits16To19Interface n;
  static const WritesBit21Interface writes;
  static const AddOffsetBit23Interface direction;
  static const PrePostIndexingBit24Interface indexing;
  static const ConditionBits28To31Interface cond;
  LoadStore2RegisterImm12Op() : ClassDecoder() , is_load_(false) {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual Register base_address_register(const Instruction i) const;
  bool HasWriteBack(const Instruction i) const {
    return indexing.IsPostIndexing(i) || writes.IsDefined(i);
  }

 protected:
  bool is_load_;  // true if load (rather than store).

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore2RegisterImm12Op);
};

// Defines the virtuals for a load immediate instruction.
class Load2RegisterImm12Op : public LoadStore2RegisterImm12Op {
 public:
  Load2RegisterImm12Op() : LoadStore2RegisterImm12Op() {
    is_load_ = true;
  }
  virtual RegisterList defs(Instruction i) const;
  virtual bool offset_is_immediate(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load2RegisterImm12Op);
};

// Defines the virtuals for a store immediate instruction.
// Note: See class LoadStore2RegisterImm12Op for more information
// on how PUSH (i.e. when Rn=Sp && U=0 && W=1 && Imm12=4) is handled.
class Store2RegisterImm12Op : public LoadStore2RegisterImm12Op {
 public:
  Store2RegisterImm12Op() : LoadStore2RegisterImm12Op() {
    is_load_ = false;
  }
  virtual RegisterList defs(Instruction i) const;

 protected:
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
  static const RegNBits16To19Interface n;
  static const WritesBit21Interface wback;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  LoadStoreRegisterList() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;

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

class StoreRegisterList : public LoadStoreRegisterList {
 public:
  StoreRegisterList() {}

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
  static const RegDBits12To15Interface vd;
  static const RegNBits16To19Interface n;
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
  virtual RegisterList immediate_addressing_defs(Instruction i) const;

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
  virtual bool offset_is_immediate(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStoreVectorRegister);
};

// Models a LoadStoreVectorRegister where it is a load.
class LoadVectorRegister : public LoadStoreVectorRegister {
 public:
  LoadVectorRegister() {}

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
  static const RegNBits0To3Interface n;
  static const RegMBits8To11Interface m;
  static const RegDBits16To19Interface d;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary3RegisterOpAltA() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpAltA);
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
//    S - Defines if the flags register is updated.
//
// If Rd, Rm, or Rn is R15, the instruction is unpredictable.
// NaCl disallows writing to PC to cause a jump.
class Binary3RegisterOpAltB : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegMBits0To3Interface m;
  static const RegDBits12To15Interface d;
  static const RegNBits16To19Interface n;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary3RegisterOpAltB() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpAltB);
};

// A Binary3RegisterOpAltB where the conditions flags are not set,
// even though bit S is true.
class Binary3RegisterOpAltBNoCondUpdates : public Binary3RegisterOpAltB {
 public:
  Binary3RegisterOpAltBNoCondUpdates() {}
  virtual RegisterList defs(Instruction i) const;

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
  static const RegNBits0To3Interface n;
  static const RegMBits8To11Interface m;
  static const RegABits12To15Interface a;
  static const RegDBits16To19Interface d;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class
  Binary4RegisterDualOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary4RegisterDualOp);
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
class Binary4RegisterDualResult : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegNBits0To3Interface n;
  static const RegMBits8To11Interface m;
  static const RegDBits12To15Interface d_lo;
  static const RegDBits16To19Interface d_hi;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class
  Binary4RegisterDualResult() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
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
  static const RegMBits0To3Interface m;
  static const RegTBits12To15Interface t;
  static const RegNBits16To19Interface n;
  static const WritesBit21Interface writes;
  static const AddOffsetBit23Interface direction;
  static const PrePostIndexingBit24Interface indexing;
  static const ConditionBits28To31Interface cond;

  LoadStore3RegisterOp() : ClassDecoder(), is_load_(false) {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual Register base_address_register(const Instruction i) const;
  bool HasWriteBack(const Instruction i) const {
    return indexing.IsPostIndexing(i) || writes.IsDefined(i);
  }

 protected:
  bool is_load_;  // true if load (rather than store).

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore3RegisterOp);
};

// Defines the virtuals for a load register instruction.
class Load3RegisterOp : public LoadStore3RegisterOp {
 public:
  Load3RegisterOp() : LoadStore3RegisterOp() {
    is_load_ = true;
  }
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load3RegisterOp);
};

// Defines the virtuals for a store register instruction.
class Store3RegisterOp : public LoadStore3RegisterOp {
 public:
  Store3RegisterOp() : LoadStore3RegisterOp() {
    is_load_ = false;
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
  static const RegT2Bits12To15Interface t2;

  LoadStore3RegisterDoubleOp() : LoadStore3RegisterOp() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore3RegisterDoubleOp);
};

// Defines the virtuals for a load double register instruction
class Load3RegisterDoubleOp : public LoadStore3RegisterDoubleOp {
 public:
  Load3RegisterDoubleOp() : LoadStore3RegisterDoubleOp() {
    is_load_ = true;
  }
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load3RegisterDoubleOp);
};

// Defines the virtuals for s store double register instruction.
class Store3RegisterDoubleOp : public LoadStore3RegisterDoubleOp {
 public:
  Store3RegisterDoubleOp() : LoadStore3RegisterDoubleOp() {
    is_load_ = false;
  }
  virtual RegisterList defs(Instruction i) const;

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
  static const RegTBits0To3Interface t;
  static const RegDBits12To15Interface d;
  static const RegNBits16To19Interface n;
  static const ConditionBits28To31Interface cond;

  StoreExclusive3RegisterOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
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
  static const RegT2Bits0To3Interface t2;

  StoreExclusive3RegisterDoubleOp() {}
  virtual SafetyLevel safety(Instruction i) const;

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
class LoadStore3RegisterImm5Op : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegMBits0To3Interface m;
  static const RegTBits12To15Interface t;
  static const RegNBits16To19Interface n;
  static const WritesBit21Interface writes;
  static const AddOffsetBit23Interface direction;
  static const PrePostIndexingBit24Interface indexing;
  static const ConditionBits28To31Interface cond;
  static const ShiftTypeBits5To6Interface shift_type;
  static const Imm5Bits7To11Interface imm;

  LoadStore3RegisterImm5Op() : ClassDecoder(), is_load_(false) {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual Register base_address_register(const Instruction i) const;

  bool HasWriteBack(const Instruction i) const {
    return indexing.IsPostIndexing(i) || writes.IsDefined(i);
  }

  // The immediate value stored in the instruction.
  uint32_t ImmediateValue(const Instruction& i) const {
    return shift_type.DecodeImmShift(i, imm.value(i));
  }

 protected:
  bool is_load_;  // true if load (rather than store).

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore3RegisterImm5Op);
};

// Defines the virtuals for a load register instruction.
class Load3RegisterImm5Op : public LoadStore3RegisterImm5Op {
 public:
  Load3RegisterImm5Op() : LoadStore3RegisterImm5Op() {
    is_load_ = true;
  }
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load3RegisterImm5Op);
};

// Defines the virtuals for a store register instruction.
class Store3RegisterImm5Op : public LoadStore3RegisterImm5Op {
 public:
  Store3RegisterImm5Op() : LoadStore3RegisterImm5Op() {
    is_load_ = false;
  }
  virtual RegisterList defs(Instruction i) const;

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
  static const RegMBits0To3Interface m;
  static const ShiftTypeBits5To6Interface shift_type;
  static const Imm5Bits7To11Interface imm;
  static const RegDBits12To15Interface d;
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
  static const RegMBits0To3Interface n;
  static const ShiftTypeBits5To6Interface shift_type;
  static const Imm5Bits7To11Interface imm5;
  static const RegDBits12To15Interface d;
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
  static const RegMBits0To3Interface m;
  static const ShiftTypeBits5To6Interface shift_type;
  static const RegSBits8To11Interface s;
  static const RegDBits12To15Interface d;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Unary3RegisterShiftedOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

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
  static const RegMBits0To3Interface m;
  static const ShiftTypeBits5To6Interface shift_type;
  static const Imm5Bits7To11Interface imm;
  static const RegDBits12To15Interface d;
  static const RegNBits16To19Interface n;
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
  static const RegMBits0To3Interface m;
  static const RegSBits8To11Interface s;
  static const RegDBits12To15Interface d;
  static const RegNBits16To19Interface n;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary4RegisterShiftedOp() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

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
  static const RegMBits0To3Interface m;
  static const ShiftTypeBits5To6Interface shift_type;
  static const Imm5Bits7To11Interface imm;
  static const RegNBits16To19Interface n;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary2RegisterImmedShiftedTest() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

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
  static const RegMBits0To3Interface m;
  static const ShiftTypeBits5To6Interface shift_type;
  static const RegSBits8To11Interface s;
  static const RegNBits16To19Interface n;
  static const UpdatesConditionsBit20Interface conditions;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Binary3RegisterShiftedTest() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterShiftedTest);
};

// Vector unary operator base class.
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
//
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled, other than their index.
class VectorUnary2RegisterOpBase : public UncondNop {
 public:
  static const RegMBits0To3Interface vm;
  static const Imm1Bit5Interface m;
  static const RegDBits12To15Interface vd;
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
  static const Bit6FlagInterface q;
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
// +--------+----------+--+----+--------+--------+----+---+--+--+--+--+--------+
// Rd - The destination register.
// Rn - The first operand.
// Rm - The second operand.
//
// d = D:Vd, n = N:Vn, m = M:Vm
//
// Note: The vector registers are not tracked by the validator, and hence,
// are not modeled, other than their index.
class VectorBinary3RegisterOpBase : public UncondNop {
 public:
  static const RegMBits0To3Interface vm;
  static const Imm1Bit5Interface m;
  static const Imm1Bit7Interface n;
  static const RegDBits12To15Interface vd;
  static const RegNBits16To19Interface vn;
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
  static const Bit6FlagInterface q;
  static const Imm4Bits8To11Interface imm;

  VectorBinary3RegisterImmOp() {}
  virtual SafetyLevel safety(Instruction i) const;
};

// Vector table lookup
// Op<c> <Dd>, <list>, <Dm>
// +--------+----------+--+----+--------+--------+----+----+--+--+--+--+--------+
// |31302928|2726252423|22|2120|19181716|15141312|1110| 9 8| 7| 6| 5| 4| 3 2 1 0|
// +--------+----------+--+----+--------+--------+----+----+--+--+--+--+--------+
// |  cond  |          | D|    |   Vn   |   Vd   |    | len| N|op| M|  |   Vm   |
// +--------+----------+--+----+--------+--------+----+----+--+--+--+--+--------+
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
  static const Bit6FlagInterface op_flag;
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
  static const RegTBits12To15Interface t;

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
// if Rt=13 then UNPREDICTABLE.
//
// Note: if Rt=PC, then it doesn't update PC. Rather, it updates the
// conditions flags ASPR.{N, Z, C, V} from corresponding conditions
// in FPSCR.
class VfpMrsOp : public CondVfpOp {
 public:
  static const RegTBits12To15Interface t;

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
// Note: We don't model Register S[Vn:N] since it can't effect NaCl validation.
class MoveVfpRegisterOp : public CondVfpOp {
 public:
  static const RegTBits12To15Interface t;
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
};

// Models a move from a core register to every element of a vfp register.
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
// if Q=1 and Vd<0>=1 then UNDEFINED
// if t=15 then UNPREDICTABLE.
// if b:e=11 then UNDEFINED.
class DuplicateToVfpRegisters : public CondVfpOp {
 public:
  static const Imm1Bit5Interface e;
  static const RegTBits12To15Interface t;
  static const RegDBits16To19Interface vd;
  static const FlagBit21Interface is_two_regs;
  static const Imm1Bit22Interface b;

  // Methods for class
  DuplicateToVfpRegisters() {}
  virtual SafetyLevel safety(Instruction i) const;

  uint32_t be_value(const Instruction& i) const {
    return (b.value(i) << 1) | e.value(i);
  }
};

}  // namespace

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_BASELINE_CLASSES_H_
