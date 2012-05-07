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
class Unary1RegisterImmediateOpTester : public Arm32DecoderTester {
 public:
  explicit Unary1RegisterImmediateOpTester(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterImmediateOpTester);
};

// Implements a decoder tester for decoder Unary1RegisterImmediateOp
// with a constraint that if Rd is R15, the instruction is unpredictable.
class Unary1RegisterImmediateOpTesterRegsNotPc
    : public Unary1RegisterImmediateOpTester {
 public:
  explicit Unary1RegisterImmediateOpTesterRegsNotPc(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterImmediateOpTesterRegsNotPc);
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
//    const = ARMExpandImm_C(imm12, ASPR.C)
class BinaryRegisterImmediateTestTester : public Arm32DecoderTester {
 public:
  explicit BinaryRegisterImmediateTestTester(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

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
class Unary2RegisterOpTester : public Arm32DecoderTester {
 public:
  explicit Unary2RegisterOpTester(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterOpTester);
};

// Implements a decoder tester for decoder Unary2RegisterOp, and
// should not parse when Rd=15 and S=1.
class Unary2RegisterOpTesterNotRdIsPcAndS : public Unary2RegisterOpTester {
 public:
  explicit Unary2RegisterOpTesterNotRdIsPcAndS(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterOpTesterNotRdIsPcAndS);
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
class Binary2RegisterImmediateOpTester : public  Arm32DecoderTester {
 public:
  explicit Binary2RegisterImmediateOpTester(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 protected:
  // If true, the Rd=15 Nacl Check is applied.
  bool apply_rd_is_pc_check_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary2RegisterImmediateOpTester);
};

// Implements a decoder tester for decoder Binary2RegisterImmediateOp,
// and should not parse when Rd=15 and S=1.
class Binary2RegisterImmediateOpTesterNotRdIsPcAndS
    : public Binary2RegisterImmediateOpTester {
 public:
  Binary2RegisterImmediateOpTesterNotRdIsPcAndS(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary2RegisterImmediateOpTesterNotRdIsPcAndS);
};

// Implements a decoder tester for decoder Binary2RegisterImmediateOp,
// where Rd can be Pc (overriding default NaCl assumptions),
// and should not parse when Rd=15 and S=1.
class Binary2RegisterImmediateOpTesterRdCanBePcAndNotRdIsPcAndS
    : public Binary2RegisterImmediateOpTesterNotRdIsPcAndS {
 public:
  Binary2RegisterImmediateOpTesterRdCanBePcAndNotRdIsPcAndS(
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Binary2RegisterImmediateOpTesterRdCanBePcAndNotRdIsPcAndS);
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
class Binary3RegisterOpTester : public Arm32DecoderTester {
 public:
  explicit Binary3RegisterOpTester(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

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
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpTesterRegsNotPc);
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
class Unary2RegisterImmedShiftedOpTester : public Arm32DecoderTester {
 public:
  explicit Unary2RegisterImmedShiftedOpTester(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterImmedShiftedOpTester);
};

// Implements a decoder tester for decoder Unary2RegisterImmedShiftedOp, and
// should not parse when imm5=0
class Unary2RegisterImmedShiftedOpTesterImm5NotZero
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  explicit Unary2RegisterImmedShiftedOpTesterImm5NotZero(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary2RegisterImmedShiftedOpTesterImm5NotZero);
};

// Implements a decoder tester for decoder Unary2RegisterImmedShiftedOp, and
// should not parse when Rd=1111 and S=1
class Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS
    : public Unary2RegisterImmedShiftedOpTester {
 public:
  explicit Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS);
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
class Unary3RegisterShiftedOpTester : public Arm32DecoderTester {
 public:
  explicit Unary3RegisterShiftedOpTester(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

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
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary3RegisterShiftedOpTesterRegsNotPc);
};

// Implements a decoder tester for decoder Binary3RegisterImmedShiftedOp.
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
class Binary3RegisterImmedShiftedOpTester : public Arm32DecoderTester {
 public:
  explicit Binary3RegisterImmedShiftedOpTester(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterImmedShiftedOpTester);
};

// Implements a decoder tester for decoder Binary3RegisterImmedShiftedOp, and
// should not parse when Rd=15 and S=1.
class Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS
    : public Binary3RegisterImmedShiftedOpTester {
 public:
  explicit Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS);
};

// Implements a decoder tester for decoder Binary3RegisterImmedShiftedOp, and
// should not parse when Rd=15 and S=1, or Rn=13
class Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndSOrRnIsSp
    : public Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS {
 public:
  explicit Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndSOrRnIsSp(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndSOrRnIsSp);
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
class Binary4RegisterShiftedOpTester : public Arm32DecoderTester {
 public:
  explicit Binary4RegisterShiftedOpTester(
      const NamedClassDecoder &decoder);
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

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
  virtual void ApplySanityChecks(
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
class Binary2RegisterImmedShiftedTestTester : public Arm32DecoderTester {
 public:
  explicit Binary2RegisterImmedShiftedTestTester(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

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
class Binary3RegisterShiftedTestTester : public Arm32DecoderTester {
 public:
  explicit Binary3RegisterShiftedTestTester(
      const NamedClassDecoder& decoder);
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

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
  virtual void ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterShiftedTestTesterRegsNotPc);
};

}  // namespace

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_INST_CLASSES_TESTERS_H_
