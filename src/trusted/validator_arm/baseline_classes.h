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
  inline CondNop() : ClassDecoder() {}
  virtual ~CondNop() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(CondNop);
};

// Models a (conditional) nop that is always unsafe (i.e. one of:
// Forbidden, Undefined, Deprecated, and Unpredictable).
class UnsafeCondNop : public CondNop {
 public:
  explicit inline UnsafeCondNop(SafetyLevel safety)
      : CondNop(), safety_(safety) {}
  virtual ~UnsafeCondNop() {}
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
  explicit inline ForbiddenCondNop() : UnsafeCondNop(FORBIDDEN) {}
  virtual ~ForbiddenCondNop() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ForbiddenCondNop);
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
  inline Unary1RegisterImmediateOp() : ClassDecoder() {}
  virtual ~Unary1RegisterImmediateOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

  // The immediate value stored in the instruction.
  inline uint32_t ImmediateValue(const Instruction& i) const {
    return (imm4.value(i) << 12) | imm12.value(i);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterImmediateOp);
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
// If Rd is R15, the instruction is unpredictable.
// NaCl disallows writing to PC to cause a jump.
// Note: Currently, only implements bfc. (A8-46).
class Unary1RegisterBitRange : public ClassDecoder {
 public:
  // Interface for components of the instruction.
  static const Imm5Bits7To11Interface lsb;
  static const RegDBits12To15Interface d;
  static const Imm5Bits16To20Interface msb;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  inline Unary1RegisterBitRange() : ClassDecoder() {}
  virtual ~Unary1RegisterBitRange() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual bool clears_bits(Instruction i, uint32_t mask) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterBitRange);
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
  inline Binary2RegisterImmediateOp() : ClassDecoder() {}
  virtual ~Binary2RegisterImmediateOp() {}
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
  virtual ~MaskedBinary2RegisterImmediateOp() {}
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
  inline BinaryRegisterImmediateTest() : ClassDecoder() {}
  virtual ~BinaryRegisterImmediateTest() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BinaryRegisterImmediateTest);
};

// Models a BinaryRegisterImmediateTest that can be used to set a condition
// by testing if the immediate value appropriately masks the value in Rn.
class MaskedBinaryRegisterImmediateTest : public BinaryRegisterImmediateTest {
 public:
  inline MaskedBinaryRegisterImmediateTest() : BinaryRegisterImmediateTest() {}
  virtual ~MaskedBinaryRegisterImmediateTest() {}
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
  inline Unary2RegisterOp() : ClassDecoder() {}
  virtual ~Unary2RegisterOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterOp);
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
  inline Binary3RegisterOp() : ClassDecoder() {}
  virtual ~Binary3RegisterOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOp);
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
  inline LoadStore2RegisterImm8Op() : ClassDecoder() {}
  virtual ~LoadStore2RegisterImm8Op() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual Register base_address_register(const Instruction i) const;
  inline bool HasWriteBack(const Instruction i) const {
    return indexing.IsPostIndexing(i) || writes.IsDefined(i);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore2RegisterImm8Op);
};

// Defines the virtuals for a load immediate instruction.
class Load2RegisterImm8Op : public LoadStore2RegisterImm8Op {
 public:
  inline Load2RegisterImm8Op() : LoadStore2RegisterImm8Op() {}
  virtual ~Load2RegisterImm8Op() {}
  virtual RegisterList defs(Instruction i) const;
  virtual bool offset_is_immediate(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load2RegisterImm8Op);
};

// Defines the virtuals for a store immediate instruction.
class Store2RegisterImm8Op : public LoadStore2RegisterImm8Op {
 public:
  inline Store2RegisterImm8Op() : LoadStore2RegisterImm8Op() {}
  virtual ~Store2RegisterImm8Op() {}
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
  virtual ~LoadStore2RegisterImm8DoubleOp() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore2RegisterImm8DoubleOp);
};

// Defines the virtuals for a load immediate double instruction.
class Load2RegisterImm8DoubleOp
    : public LoadStore2RegisterImm8DoubleOp {
 public:
  inline Load2RegisterImm8DoubleOp()
      : LoadStore2RegisterImm8DoubleOp() {}
  virtual ~Load2RegisterImm8DoubleOp() {}
  virtual RegisterList defs(Instruction i) const;
  virtual bool offset_is_immediate(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load2RegisterImm8DoubleOp);
};

// Defines the virtuals for a store immediate double instruction.
class Store2RegisterImm8DoubleOp
    : public LoadStore2RegisterImm8DoubleOp {
 public:
  inline Store2RegisterImm8DoubleOp()
      : LoadStore2RegisterImm8DoubleOp() {}
  virtual ~Store2RegisterImm8DoubleOp() {}
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
// Note: We NaCl disallow Rt=PC for stores (not just loads), even
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
  inline LoadStore2RegisterImm12Op() : ClassDecoder() {}
  virtual ~LoadStore2RegisterImm12Op() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual Register base_address_register(const Instruction i) const;
  inline bool HasWriteBack(const Instruction i) const {
    return indexing.IsPostIndexing(i) || writes.IsDefined(i);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore2RegisterImm12Op);
};

// Defines the virtuals for a load immediate instruction.
class Load2RegisterImm12Op : public LoadStore2RegisterImm12Op {
 public:
  inline Load2RegisterImm12Op() : LoadStore2RegisterImm12Op() {}
  virtual ~Load2RegisterImm12Op() {}
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
  inline Store2RegisterImm12Op() : LoadStore2RegisterImm12Op() {}
  virtual ~Store2RegisterImm12Op() {}
  virtual RegisterList defs(Instruction i) const;

 protected:
  NACL_DISALLOW_COPY_AND_ASSIGN(Store2RegisterImm12Op);
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
  inline Binary3RegisterOpAltA() : ClassDecoder() {}
  virtual ~Binary3RegisterOpAltA() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpAltA);
};

// A Binary3RegisterOpAltA where the conditions flags are not set,
// even though bit S is true.
class Binary3RegisterOpAltANoCondUpdates : public Binary3RegisterOpAltA {
 public:
  inline Binary3RegisterOpAltANoCondUpdates() {}
  virtual ~Binary3RegisterOpAltANoCondUpdates() {}
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpAltANoCondUpdates);
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
  inline Binary4RegisterDualOp() : ClassDecoder() {}
  virtual ~Binary4RegisterDualOp() {}
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
  inline Binary4RegisterDualResult() : ClassDecoder() {}
  virtual ~Binary4RegisterDualResult() {}
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

  inline LoadStore3RegisterOp() : ClassDecoder() {}
  virtual ~LoadStore3RegisterOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual Register base_address_register(const Instruction i) const;
  inline bool HasWriteBack(const Instruction i) const {
    return indexing.IsPostIndexing(i) || writes.IsDefined(i);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore3RegisterOp);
};

// Defines the virtuals for a load register instruction.
class Load3RegisterOp : public LoadStore3RegisterOp {
 public:
  inline Load3RegisterOp() : LoadStore3RegisterOp() {}
  virtual ~Load3RegisterOp() {}
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load3RegisterOp);
};

// Defines the virtuals for a store register instruction.
class Store3RegisterOp : public LoadStore3RegisterOp {
 public:
  inline Store3RegisterOp() : LoadStore3RegisterOp() {}
  virtual ~Store3RegisterOp() {}
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
  virtual ~LoadStore3RegisterDoubleOp() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore3RegisterDoubleOp);
};

// Defines the virtuals for a load double register instruction
class Load3RegisterDoubleOp : public LoadStore3RegisterDoubleOp {
 public:
  Load3RegisterDoubleOp() : LoadStore3RegisterDoubleOp() {}
  virtual ~Load3RegisterDoubleOp() {}
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load3RegisterDoubleOp);
};

// Defines the virtuals for s store double register instruction.
class Store3RegisterDoubleOp : public LoadStore3RegisterDoubleOp {
 public:
  Store3RegisterDoubleOp() : LoadStore3RegisterDoubleOp() {}
  virtual ~Store3RegisterDoubleOp() {}
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Store3RegisterDoubleOp);
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

  inline LoadStore3RegisterImm5Op() : ClassDecoder() {}
  virtual ~LoadStore3RegisterImm5Op() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual Register base_address_register(const Instruction i) const;
  inline bool HasWriteBack(const Instruction i) const {
    return indexing.IsPostIndexing(i) || writes.IsDefined(i);
  }

  // The immediate value stored in the instruction.
  inline uint32_t ImmediateValue(const Instruction& i) const {
    return shift_type.DecodeImmShift(i, imm.value(i));
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore3RegisterImm5Op);
};

// Defines the virtuals for a load register instruction.
class Load3RegisterImm5Op : public LoadStore3RegisterImm5Op {
 public:
  inline Load3RegisterImm5Op() : LoadStore3RegisterImm5Op() {}
  virtual ~Load3RegisterImm5Op() {}
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Load3RegisterImm5Op);
};

// Defines the virtuals for a store register instruction.
class Store3RegisterImm5Op : public LoadStore3RegisterImm5Op {
 public:
  inline Store3RegisterImm5Op() : LoadStore3RegisterImm5Op() {}
  virtual ~Store3RegisterImm5Op() {}
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
  inline Unary2RegisterImmedShiftedOp() : ClassDecoder() {}
  virtual ~Unary2RegisterImmedShiftedOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

  // The immediate value stored in the instruction.
  inline uint32_t ImmediateValue(const Instruction& i) const {
    return shift_type.DecodeImmShift(i, imm.value(i));
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterImmedShiftedOp);
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
  inline Unary3RegisterShiftedOp() : ClassDecoder() {}
  virtual ~Unary3RegisterShiftedOp() {}
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
  inline Binary3RegisterImmedShiftedOp() : ClassDecoder() {}
  virtual ~Binary3RegisterImmedShiftedOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // The shift value to use.
  inline uint32_t ShiftValue(const Instruction& i) const {
    return shift_type.DecodeImmShift(i, imm.value(i));
  }
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterImmedShiftedOp);
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
  inline Binary4RegisterShiftedOp() : ClassDecoder() {}
  virtual ~Binary4RegisterShiftedOp() {}
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
  inline Binary2RegisterImmedShiftedTest() : ClassDecoder() {}
  virtual ~Binary2RegisterImmedShiftedTest() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // The shift value to use.
  inline uint32_t ShiftValue(const Instruction& i) const {
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
  inline Binary3RegisterShiftedTest() : ClassDecoder() {}
  virtual ~Binary3RegisterShiftedTest() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterShiftedTest);
};

}  // namespace

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_BASELINE_CLASSES_H_
