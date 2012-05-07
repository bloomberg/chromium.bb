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
  static const UpdatesFlagsRegisterBit20Interface flags;
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
  static const UpdatesFlagsRegisterBit20Interface flags;
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
  static const UpdatesFlagsRegisterBit20Interface flags;
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
  static const UpdatesFlagsRegisterBit20Interface flags;
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
  static const UpdatesFlagsRegisterBit20Interface flags;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  inline Binary3RegisterOp() : ClassDecoder() {}
  virtual ~Binary3RegisterOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOp);
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
  static const UpdatesFlagsRegisterBit20Interface flags;
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
  static const UpdatesFlagsRegisterBit20Interface flags;
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
  static const UpdatesFlagsRegisterBit20Interface flags;
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

// Defines Binary3RegisterImmedShiftedOp instructions where Rn=sp is
// deprecated.
class Binary3RegisterImmedShiftedOpRnNotSp
    : public Binary3RegisterImmedShiftedOp {
 public:
  inline Binary3RegisterImmedShiftedOpRnNotSp()
      : Binary3RegisterImmedShiftedOp() {}
  virtual ~Binary3RegisterImmedShiftedOpRnNotSp() {}
  virtual SafetyLevel safety(Instruction i) const;
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
  static const UpdatesFlagsRegisterBit20Interface flags;
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
  static const UpdatesFlagsRegisterBit20Interface flags;
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
  static const UpdatesFlagsRegisterBit20Interface flags;
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
