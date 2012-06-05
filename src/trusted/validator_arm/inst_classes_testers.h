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

// Implements a decoder tester for an UnsafeClassDecoder
class UnsafeCondNopTester : public Arm32DecoderTester {
 public:
  explicit UnsafeCondNopTester(const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(UnsafeCondNopTester);
};

// Implements a decoder tester for decoder CondNop.
// Nop<c>
// +--------+--------------------------------------------------------+
// |31302918|272625242322212019181716151413121110 9 8 7 6 5 4 3 2 1 0|
// +--------+--------------------------------------------------------+
// |  cond  |                                                        |
// +--------+--------------------------------------------------------+
class CondNopTester : public Arm32DecoderTester {
 public:
  explicit CondNopTester(const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(CondNopTester);
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
class Unary1RegisterImmediateOpTester : public Arm32DecoderTester {
 public:
  explicit Unary1RegisterImmediateOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
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
  Unary1RegisterImmediateOpTesterNotRdIsPcAndS(
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
// If Rd is R15, the instruction is unpredictable.
// NaCl disallows writing to PC to cause a jump.
// Note: Currently, only implements bfc. (A8-46).
class Unary1RegisterBitRangeTester : public Arm32DecoderTester {
 public:
  explicit Unary1RegisterBitRangeTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterBitRangeTester);
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
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
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
  virtual bool ApplySanityChecks(
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
  virtual bool ApplySanityChecks(
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
  virtual bool ApplySanityChecks(
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
  Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS(
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
class Binary3RegisterOpTester : public Arm32DecoderTester {
 public:
  explicit Binary3RegisterOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
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
class Binary3RegisterOpAltATester : public Arm32DecoderTester {
 public:
  explicit Binary3RegisterOpAltATester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

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
class Binary3RegisterOpAltBTester : public Arm32DecoderTester {
 public:
  explicit Binary3RegisterOpAltBTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);

 protected:
  // When true, tester should test conditions flag S.
  bool test_conditions_;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpAltBTester);
};

// Implements a decoder tester for Binary3RegisterOpAltB with a
// constraint that if Rd, Rm, or Rn is R15, the instruction is
// unpredictable.
class Binary3RegisterOpAltBTesterRegsNotPc
    : public Binary3RegisterOpAltBTester {
 public:
  explicit Binary3RegisterOpAltBTesterRegsNotPc(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpAltBTesterRegsNotPc);
};

// Implements a decoder tester for Binary3RegisterOpAltB, except
// that conditions flags are not checked.
class Binary3RegisterOpAltBNoCondUpdatesTester
    : public Binary3RegisterOpAltBTester {
 public:
  explicit Binary3RegisterOpAltBNoCondUpdatesTester(
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterOpAltBNoCondUpdatesTester);
};

// Implements a decoder tester for Binary3RegisterOpAltB with a
// constraint that if Rd, Rm, or Rn is R15, the instruction is
// unpredictable, and that condition flags are not checked.
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

// Implements a Binary3RegisterOpAltANoCondUpdates tester (i.e.
// a Binary3RegisterOpAltA above except that S is 1 but ignored).
// class Binary3RegisterOpAltANoCondUpdates : public

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
class Binary4RegisterDualOpTester : public Arm32DecoderTester {
 public:
  explicit Binary4RegisterDualOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

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
class Binary4RegisterDualResultTester : public Arm32DecoderTester {
 public:
  explicit Binary4RegisterDualResultTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

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
class LoadStore2RegisterImm8OpTester : public Arm32DecoderTester {
 public:
  explicit LoadStore2RegisterImm8OpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore2RegisterImm8OpTester);
};

// Defines a LoadStore2RegisterImm8OpTester with the added constraint
// that it doesn't parse when Rn=15
class LoadStore2RegisterImm8OpTesterNotRnIsPc
    : public LoadStore2RegisterImm8OpTester {
 public:
  LoadStore2RegisterImm8OpTesterNotRnIsPc(
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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore2RegisterImm8DoubleOpTester);
};

// Defines a LoadStore2RegisterImm8DoubleOpTester with the added constraint
// that it doesn't parse when Rn=15.
class LoadStore2RegisterImm8DoubleOpTesterNotRnIsPc
    : public LoadStore2RegisterImm8DoubleOpTester {
 public:
  LoadStore2RegisterImm8DoubleOpTesterNotRnIsPc(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      LoadStore2RegisterImm8DoubleOpTesterNotRnIsPc);
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
class LoadStore2RegisterImm12OpTester : public Arm32DecoderTester {
 public:
  explicit LoadStore2RegisterImm12OpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore2RegisterImm12OpTester);
};

// Defines a LoadStore2RegisterImm12OpTester with the added constraint
// that it doesn't parse when Rn=15.
class LoadStore2RegisterImm12OpTesterNotRnIsPc
    : public LoadStore2RegisterImm12OpTester {
 public:
  LoadStore2RegisterImm12OpTesterNotRnIsPc(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore2RegisterImm12OpTesterNotRnIsPc);
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
class LoadStore3RegisterOpTester : public Arm32DecoderTester {
 public:
  explicit LoadStore3RegisterOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadStore3RegisterDoubleOpTester);
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
class LoadStore3RegisterImm5OpTester : public Arm32DecoderTester {
 public:
  explicit LoadStore3RegisterImm5OpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

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
class Unary2RegisterImmedShiftedOpTester : public Arm32DecoderTester {
 public:
  explicit Unary2RegisterImmedShiftedOpTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
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
  virtual bool ApplySanityChecks(
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
  virtual bool ApplySanityChecks(
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
  virtual bool ApplySanityChecks(
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
  virtual bool ApplySanityChecks(
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
  virtual bool ApplySanityChecks(
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
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS);
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
  virtual bool ApplySanityChecks(
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
class Binary2RegisterImmedShiftedTestTester : public Arm32DecoderTester {
 public:
  explicit Binary2RegisterImmedShiftedTestTester(
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(
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
  virtual bool ApplySanityChecks(
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
  virtual bool ApplySanityChecks(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterShiftedTestTesterRegsNotPc);
};

}  // namespace

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_INST_CLASSES_TESTERS_H_
