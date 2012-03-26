/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_INST_CLASSES_H
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_INST_CLASSES_H

#include <stdint.h>
#include "native_client/src/trusted/validator_arm/model.h"
#include "native_client/src/include/portability.h"

/*
 * Models the "instruction classes" that the decoder produces.
 */
namespace nacl_arm_dec {

/*
 * Used to describe whether an instruction is safe, and if not, what the issue
 * is.  Only instructions that MAY_BE_SAFE should be allowed in untrusted code,
 * and even those may be rejected by the validator.
 */
enum SafetyLevel {
  // The initial value of uninitialized SafetyLevels -- treat as unsafe.
  UNKNOWN = 0,

  // This instruction is left undefined by the ARMv7 ISA spec.
  UNDEFINED,
  // This instruction has unpredictable effects at runtime.
  UNPREDICTABLE,
  // This instruction is deprecated in ARMv7.
  DEPRECATED,

  // This instruction is forbidden by our SFI model.
  FORBIDDEN,
  // This instruction's operands are forbidden by our SFI model.
  FORBIDDEN_OPERANDS,

  /*
   * This instruction may be safe in untrusted code: in isolation it contains
   * nothing scary, but the validator may overrule this during global analysis.
   */
  MAY_BE_SAFE
};

/****************  Decoder helper classes *****************************
 *
 * Decoder helper classes provide facilities to extract information from
 * thumb instructions. Thumb instructions are 1 or 2 16-bit words.
 **********************************************************************/

/* Helper class to pull out Register D from bits 12 through 15. */
class RegDBits12To15 {
 public:
  virtual ~RegDBits12To15() {}
  inline Register Rd(const Instruction& i) const {
    return i.reg(15, 12);
  }
};

/* Helper class to pull out Register M from bits 0 through 3. */
class RegMBits0To3 {
 public:
  virtual ~RegMBits0To3() {}
  inline Register Rm(const Instruction& i) const {
    return i.reg(3, 0);
  }
};

/* Helper class to pull out Register n from bits 16 through 19.
 */
class RegNBits16To19 {
 public:
  virtual ~RegNBits16To19() {}
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
};

/* Helper class to pull out Register S from bits 8 through 11. */
class RegSBits8To11 {
 public:
  virtual ~RegSBits8To11() {}
  inline Register Rs(const Instruction& i) const {
    return i.reg(11, 8);
  }
};

/****************  Arm decoder classes ******************************/

/*
 * Decodes a class of instructions.  Does spooky undefined things if handed
 * instructions that don't belong to its class.  Who defines which instructions
 * these are?  Why, the generated decoder, of course.
 *
 * This is an abstract base class intended to be overridden with the details of
 * particular instruction-classes.
 *
 * ClassDecoders should be stateless, and should provide a no-arg constructor
 * for use by the generated decoder.
 */
class ClassDecoder {
 public:
  /*
   * Checks how safe this instruction is, in isolation.
   * This will detect any violation in the ARMv7 spec -- undefined encodings,
   * use of registers that are unpredictable -- and the most basic constraints
   * in our SFI model.  Because ClassDecoders are referentially-transparent and
   * cannot touch global state, this will not check things that may vary with
   * ABI version.
   *
   * The most positive result this can return is called MAY_BE_SAFE because it
   * is necessary, but not sufficient: the validator has the final say.
   */
  virtual SafetyLevel safety(Instruction i) const = 0;

  /*
   * Gets the set of registers affected when an instruction executes.  This set
   * is complete, and includes
   *  - explicit destination register(s),
   *  - changes to flags,
   *  - indexed-addressing writeback,
   *  - changes to r15 by branches,
   *  - implicit register results, like branch-with-link.
   *
   * The default implementation returns a ridiculous bitmask that suggests that
   * all possible side effects will occur -- override if this is not
   * appropriate. :-)
   */
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterListEverything;
  }

  /*
   * Gets the set of registers that this instruction defines through immediate
   * indexed addressing writeback -- a subset of the defs() set.
   *
   * This distinction is useful for operations like SP-relative loads, because
   * the maximum displacement that immediate addressing can produce is small.
   *
   * Note that this does not include defs produced by *register* indexed
   * addressing writeback, since they have no useful properties in our model.
   *
   * Stubbed to indicate that no such addressing occurs.
   */
  virtual RegisterList immediate_addressing_defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }

  /*
   * Checks whether the instruction can write to memory.  Note that we only
   * permit base+immediate addressing stores, so if the safety() looks good
   * and writes_memory() is true, you can assume base+immediate addressing is
   * being used.
   *
   * Stubbed to return 'false', which is the common case.
   */
  virtual bool writes_memory(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return false;
  }

  /*
   * For instructions that can read or write memory, gets the register used as
   * the base for generating the effective address.
   *
   * It is stubbed to return nonsense.
   */
  virtual Register base_address_register(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }

  /*
   * Checks whether the instruction computes its read or write address as
   * base address + immediate.
   *
   * It is stubbed to return false.
   */
  virtual bool offset_is_immediate(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return false;
  }

  /*
   * For indirect branch instructions, returns the register being moved into
   * r15.  Otherwise, reports kRegisterNone.
   *
   * Note that this exclusively describes instructions that write r15 from a
   * register, unmodified.  This means BX, BLX, and MOV without shift.  Not
   * even BIC, which we allow to write to r15, is modeled this way.
   */
  virtual Register branch_target_register(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }

  /*
   * Checks whether the instruction is a direct relative branch -- meaning it
   * adds a constant offset to r15.
   */
  virtual bool is_relative_branch(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return false;
  }

  /*
   * For relative branches, gets the offset added to the instruction's
   * virtual address to find the target.  The results are bogus unless
   * is_relative_branch() returns true.
   *
   * Note that this is different than the offset added to r15 at runtime, since
   * r15 reads as 8 bytes ahead.  This function does the math so you don't have
   * to.
   */
  virtual int32_t branch_target_offset(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return 0;
  }

  /*
   * Checks whether this instruction is the special bit sequence that marks
   * the start of a literal pool.
   */
  virtual bool is_literal_pool_head(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return false;
  }

  /*
   * Checks that an instruction clears a certain pattern of bits in all its
   * (non-flag) result registers.  The mask should include 1s in the positions
   * that should be cleared.
   */
  virtual bool clears_bits(Instruction i, uint32_t mask) const {
    UNREFERENCED_PARAMETER(i);
    UNREFERENCED_PARAMETER(mask);
    return false;
  }

  /*
   * Checks that an instruction will set Z if certain bits in r (chosen by 1s in
   * the mask) are clear.
   *
   * Note that the inverse does not hold: the actual instruction i may require
   * *more* bits to be clear to set Z.  This is fine.
   */
  virtual bool sets_Z_if_bits_clear(Instruction i,
                                    Register r,
                                    uint32_t mask) const {
    UNREFERENCED_PARAMETER(i);
    UNREFERENCED_PARAMETER(r);
    UNREFERENCED_PARAMETER(mask);
    return false;
  }

  /* Many instructions define control bits in bits 20-24. The useful bits
   * are defined here.
   */

  // True if U (updates flags register) flag is defined.
  inline bool UpdatesFlagsRegister(const Instruction& i) const {
    return i.bit(20);
  }

  // True if W (does write) flag is defined.
  inline bool WritesFlag(const Instruction& i) const {
    return i.bit(21);
  }
  // True if P (pre-indexing) flag is defined.
  inline bool PreindexingFlag(const Instruction& i) const {
    return i.bit(24);
  }

 protected:
  ClassDecoder() {}
  virtual ~ClassDecoder() {}
};

/*
 * Represents an instruction that is forbidden under all circumstances, so we
 * didn't bother decoding it further.
 */
class Forbidden : public ClassDecoder {
 public:
  virtual ~Forbidden() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return FORBIDDEN;
  }
  // Switch off the def warnings -- it's already forbidden!
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }
};

/*
 * Represents the undefined space in the instruction encoding.
 */
class Undefined : public ClassDecoder {
 public:
  virtual ~Undefined() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return UNDEFINED;
  }
  // Switch off the def warnings -- it's already undefined!
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }
};

/*
 * Represents instructions that have been deprecated in ARMv7.
 */
class Deprecated : public ClassDecoder {
 public:
  virtual ~Deprecated() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return DEPRECATED;
  }
  // Switch off the def warnings -- it's already deprecated!
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }
};

/*
 * Represents an unpredictable encoding.  Note that many instructions may
 * *become* unpredictable based on their operands -- this is used only for
 * the case where a large space of the instruction set is unpredictable.
 */
class Unpredictable : public ClassDecoder {
 public:
  virtual ~Unpredictable() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return UNPREDICTABLE;
  }
  // Switch off the def warnings -- it's already unpredictable!
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }
};

/*
 * An instruction that, for modeling purposes, is a no-op.  It has no
 * side effects that are significant to SFI, and has no operand combinations
 * that cause it to become unsafe.
 *
 * Uses of this class must be carefully evaluated, because it bypasses all
 * further validation.
 *
 * Includes:
 * NOP, YIELD, WFE, WFI, SEV, DBG, PLDW(immediate), PLD(immediate),
 * PLD(literal), CLREX, DSB, DMB, ISB, PLI(register), PLDW(register),
 * PLD(register), VEXT, VTBL, VTBX, VHADD, VQADD, VRHADD, VAND(register),
 * VBIC(register), VORR(register), VORN(register), VEOR(register), VBSL,
 * VBIT, VBIF, VHSUB, VQSUB, VCGT(register), VCGE(register), VSHL(register),
 * VQSHL(register), VRSHL(register), VQRSHL(register), VMAX, VMIN (integer),
 * VABD, VABDL (integer), VABA, VABAL, VADD(integer), VSUB(integer),
 * VTST(integer), VCEQ(integer), VMLA, VMLAL, VMLS, VMLSL (integer),
 * VMUL, VMULL(integer/poly), VPMAX, VPMIN(integer), VQDMULH, VQRDMULH,
 * VPADD(integer), VADD(float), VSUB(float), VPADD(float), VABD(float),
 * VMLA, VMLS(float), VMUL(float), VCEQ(register), VCGE(register),
 * VCGT(register), VACGE, VACGT, VACLE, VACLT, VMAX, VMIN(float),
 * VPMAX, VPMIN(float), VRECPS, VRSQRTS, VADDL, VSUBL, VADDHN, VRADDHN,
 * VABA, VABAL, VSUBHN, VRSUBHN, VABD, VABDL(integer), VMLA, VMLAL,
 * VMLS, VMLSL (integer), VQDMLAL, VQDMLSL, VMUL, VMULL (integer), VQDMULL,
 * VMUL, VMULL (polynomial), VMLA, VMLS (scalar), VMLAL, VMLSL (scalar),
 * VQDMLAL, VMQDLSL, VMUL(scalar), VMULL(scalar), VQDMULL, VQDMULH, VQRDMULH,
 * VSHR, VSRA, VRSHR, VRSRA, VSRI, VSHL(immediate), VSLI,
 * VQSHL, VQSHLU(immediate), VSHRN, VRSHRN, VQSHRUN, VQRSHRUN, VQSHRN, VQRSHRN,
 * VSHLL, VMOVL, VCVT (floating- and fixed-point), VREV64, VREV32, VREV16,
 * VPADDL, VCLS, VCLZ, VCNT, VMVN(register), VPADAL, VQABS, VQNEG,
 * VCGT (immediate #0), VCGE (immediate #0), VCEQ (immediate #0),
 * VCLE (immediate #0), VCLT (immediate #0), VABS, VNEG, VSWP, VTRN, VUZP,
 * VZIP, VMOVN, VQMOVUN, VQMOVN, VSHLL, VCVT (half- and single-precision),
 * VRECPE, VRSQRTE, VCVT (float and integer), VMOV(immediate),
 * VORR(immediate), VMOV(immediate), VORR(immediate), VMOV(immediate),
 * VMVN(immediate), VBIC(immediate), VMVN(immediate), VBIC(immediate),
 * VMVN(immediate), VMOV(immediate)
 */
class EffectiveNoOp : public ClassDecoder {
 public:
  virtual ~EffectiveNoOp() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }
};

/*
 * Models all instructions that reliably trap, preventing execution from falling
 * through to the next instruction.  Note that roadblocks currently have no
 * special role in the SFI model, so Breakpoints are distinguished below.
 */
class Roadblock : public ClassDecoder {
 public:
  virtual ~Roadblock() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }
};

/*
 * BKPT
 * We model this mostly so we can use it to recognize literal pools -- untrusted
 * code isn't expected to use it, but it's not unsafe, and there are cases where
 * we may generate it.
 */
class Breakpoint : public Roadblock {
 public:
  virtual ~Breakpoint() {}

  virtual bool is_literal_pool_head(Instruction i) const;
};

/* Models a 3-register-shifted unary operation of the form:
 * Op(S)<c> <Rd>, <Rm>,  <type> <Rs>
 * +--------+--------------+--+--------+--------+--------+--+----+--+--------+
 * |31302928|27262524232221|20|19181716|15141312|1110 9 8| 7| 6 5| 4| 3 2 1 0|
 * +--------+--------------+--+--------+--------+--------+--+----+--+--------+
 * |  cond  |              | S|        |   Rd   |   Rs   |  |type|  |   Rm   |
 * +--------+--------------+--+--------+--------+--------+--+----+--+--------+
 * Note: if Rd, Rs, or Rm is R15, the instruction is unpredictable.
 * Definitions:
 *    Rd - The destination register.
 *    Rm - The register that is shifted and used as the operand.
 *    Rs - The regsiter whose bottom byte contains the amount to shift by.
 *    type - The type of shift to apply (not modeled).
 *    S - Defines if the flags regsiter is updated.
 * Implements:
 *    MVN(register-shifted) A1 A8-218
 */
class Unary3RegisterShiftedOp : public ClassDecoder,
                                public RegMBits0To3,
                                public RegSBits8To11,
                                public RegDBits12To15 {
 public:
  virtual ~Unary3RegisterShiftedOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // Returns true if S is 1. That is, it updates the flags register.
  inline bool UpdatesFlagsRegister(Instruction i) const {
    return i.bit(20);
  }
};

/* Models a 4-register-shifted binary operation of the form:
 * Op(S)<c> <Rd>, <Rn>, <Rm>,  <type> <Rs>
 * +--------+--------------+--+--------+--------+--------+--+----+--+--------+
 * |31302928|27262524232221|20|19181716|15141312|1110 9 8| 7| 6 5| 4| 3 2 1 0|
 * +--------+--------------+--+--------+--------+--------+--+----+--+--------+
 * |  cond  |              | S|   Rn   |   Rd   |   Rs   |  |type|  |   Rm   |
 * +--------+--------------+--+--------+--------+--------+--+----+--+--------+
 * Note: if Rn, Rd, Rs, or Rm is R15, the instruction is unpredictable.
 * Definitions:
 *    Rd - The destination register.
 *    Rn - The first operand register.
 *    Rm - The register that is shifted and used as the second operand.
 *    Rs - The regsiter whose bottom byte contains the amount to shift by.
 *    type - The type of shift to apply (not modeled).
 *    S - Defines if the flags regsiter is updated.
 * Implements:
 *    ADC(register-shifted) A1 A8-18
 *    ADD(register-shifted) A1 A8-26
 *    AND(register-shifted) A1 A8-38
 *    BIC(register-shifted) A1 A8-54
 *    EOR(register-shifted) A1 A8-98
 *    ORR(register-shifted) A1 A8-232
 *    RSB(register-shifted) A1 A8-288
 *    RSC(register-shifted) A1 A8-294
 *    SBC(register-shifted) A1 A8-306
 *    SUB(register-shifted) A1 A8-424
 */
class Binary4RegisterShiftedOp : public ClassDecoder,
                                 public RegMBits0To3,
                                 public RegSBits8To11,
                                 public RegDBits12To15,
                                 public RegNBits16To19 {
 public:
  virtual ~Binary4RegisterShiftedOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // Returns true if S is 1. That is, it updates the flags register.
  inline bool UpdatesFlagsRegister(Instruction i) const {
    return i.bit(20);
  }
};

/*
 * Models the most common class of data processing instructions.  We use this
 * for any operation that
 *  - writes a single register, specified in 15:12;
 *  - does not write memory,
 *  - should not be permitted to cause a jump by writing r15,
 *  - writes flags when bit 20 is set.
 *
 * Includes:
 * MOV (immediate) 16-bit version, MOVT, AND(register),
 * EOR(register), SUB(register),
 * RSB(register), ADD(register),
 * ADC(register), SBC(register),
 * RSC(register), ORR(register),
 * MOV(register), LSL(register), LSL(immediate), LSL(register),
 * LSR(immediate), LSR(register), ASR(immediate), ASR(register),
 * RRX, ROR(register), ROR(immediate), ROR(register), BIC(register),
 * MVN(register), AND(immediate),
 * EOR(immediate), SUB(immediate), ADR, RSB(immediate), ADD(immediate), ADR,
 * ADC(immediate), SBC(immediate), RSC(immediate), ORR(immediate),
 * MOV(immediate), MVN(immediate), MRS, CLZ, SBFX, BFC, BFI, UBFX, UADD16,
 * UASX, USAX, USUB16, UADD8, USUB8, UQADD16, UQASX, UQSAX, UQSUB16, UQADD8,
 * UQSUB8, UHADD16, UHASX, UHSAX, UHSUB16, UHADD8, UHSUB8
 */
class DataProc : public ClassDecoder {
 public:
  virtual ~DataProc() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // Defines the destination register of the data operation.
  inline Register Rd(const Instruction& i) const {
    return i.reg(15, 12);
  }
};

/* Models a 3-register-shifted test operation of the form:
 * OpS<c> <Rn>, <Rm>, <type>, <Rs>
 * +--------+--------------+--+--------+--------+--------+--+----+--+--------+
 * |31302928|27262524232221|20|19181716|15141312|1110 9 8| 7| 6 5| 4| 3 2 1 0|
 * +--------+--------------+--+--------+--------+--------+--+----+--+--------+
 * |  cond  |              | S|   Rn   |        |   Rs   |  |type|  |   Rm   |
 * +--------+--------------+--+--------+--------+--------+--+----+--+--------+
 * Note: if Rn, Rs, or Rm is R15, the instruction is unpredictable.
 * Definitions:
 *    Rn - The first operand register.
 *    Rm - The register that is shifted and used as the second operand.
 *    Rs - The regsiter whose bottom byte contains the amount to shift by.
 *    type - The type of shift to apply (not modeled).
 *    S - Defines if the flags regsiter is updated.
 * Implements:
 *    CMN(register-shifted) A1 A8-78
 *    CMP(register-shifted) A1 A8-84
 *    TEQ(register-shifted) A1 A8-452
 *    TST(register-shifted) A1 A8-458
 */
class Binary3RegisterShiftedTest : public ClassDecoder,
                                   public RegMBits0To3,
                                   public RegSBits8To11,
                                   public RegNBits16To19 {
 public:
  virtual ~Binary3RegisterShiftedTest() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // Returns true if S is 1. That is, it updates the flags register.
  inline bool UpdatesFlagsRegister(Instruction i) const {
    return i.bit(20);
  }
};

/*
 * Models the few data-processing instructions that *don't* produce a result,
 * but may still set flags.
 *
 * Includes:
 * TST(register), TEQ(register),
 * CMP(register), CMN(register),
 * TST(immediate), TEQ(immediate), CMP(immediate), CMN(immediate)
 */
class Test : public DataProc {
 public:
  virtual ~Test() {}
  virtual RegisterList defs(Instruction i) const;
};

/*
 * Specifically models the TST register-immediate instruction, which is
 * important to our conditional store sandbox.
 */
class TestImmediate : public Test {
 public:
  virtual ~TestImmediate() {}

  virtual bool sets_Z_if_bits_clear(Instruction i,
                                    Register r,
                                    uint32_t mask) const;
  // Defines the operand register.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
};

/*
 * A special-case data processing instruction: the immediate BIC.  We consider
 * this the only instruction that reliably clears bits in its result, so we
 * model it separately from other logic ops.
 */
class ImmediateBic : public DataProc {
 public:
  virtual ~ImmediateBic() {}

  // ImmediateBic is exempted from the writes-r15 check.
  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual bool clears_bits(Instruction i, uint32_t mask) const;
};

/*
 * Models the Pack/Saturate/Reverse instructions, which
 *  - Write a register identified by 15:12,
 *  - Are not permitted to affect r15,
 *  - Always set flags.
 *
 *  Note that not all of the Pack/Sat/Rev instructions really set flags, but
 *  we deliberately err on the side of caution to simplify modeling (because we
 *  don't care, in practice, about their flag effects).
 *
 * Includes:
 * PKH, SSAT, USAT, SXTAB16, SXTB16, SEL, SSAT16, SXTAB, SXTB, REV,
 * SXTAH, SXTH, REV16, UXTAB16, UXTB16, USAT16, UXTAB, UXTB, RBIT,
 * UXTAH, UXTH, REVSH
 */
class PackSatRev : public ClassDecoder {
 public:
  virtual ~PackSatRev() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // Defines the destination register.
  inline Register Rd(const Instruction& i) const {
    return i.reg(15, 12);
  }
};


/*
 * Models multiply instructions, which
 *  - Write a register identified by 19:16,
 *  - Are not permitted to affect r15,
 *  - Always set flags.
 *
 * Note that multiply instructions don't *really* always set the flags.  We
 * deliberately take this shortcut to simplify modeling of the many, many
 * multiply instructions -- some of which always set flags, some of which never
 * do, and some of which set conditionally.
 *
 * Includes:
 * MUL, MLA, MLS, SMLABB et al., SMLAWB et al., SMULWB et al., SMULBB et al.,
 * USAD8, USADA8, SMLAD, SMUAD, SMLSD, SMUSD, SMMLA, SMMUL, SMMLS
 */
class Multiply : public ClassDecoder {
 public:
  virtual ~Multiply() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // Defines the destination register.
  inline Register Rd(const Instruction& i) const {
    return i.reg(19, 16);
  }
};

/*
 * Models double-word multiply instructions, which
 *  - Write TWO registers, identified by 19:16 and 15:12,
 *  - Are not permitted to affect r15,
 *  - Always set flags.
 *
 * Note that multiply instructions don't *really* always set the flags.  We
 * deliberately take this shortcut to simplify modeling of the many, many
 * multiply instructions -- some of which always set flags, some of which never
 * do, and some of which set conditionally.
 *
 * Includes:
 * UMAAL, UMULL, UMLAL, SMULL, SMLAL, SMLALBB, SMLALD, SMLSLD
 */
class LongMultiply : public Multiply {
 public:
  virtual ~LongMultiply() {}

  virtual RegisterList defs(Instruction i) const;
  // Supplies the lower 32 bits for the destination result.
  inline Register RdLo(const Instruction& i) const {
    return i.reg(15, 12);
  }
  // Supplies the other 32 bits of the destination result.
  inline Register RdHi(const Instruction& i) const {
    return i.reg(19, 16);
  }
};

/*
 * Saturating adds and subtracts.  Conceptually equivalent to DataProc,
 * except for the use of the S bit (bit 20) -- they always set flags.
 *
 * Includes:
 * QADD, QSUB, QDADD, QDSUB
 */
class SatAddSub : public DataProc {
 public:
  virtual ~SatAddSub() {}

  virtual RegisterList defs(Instruction i) const;
};

/*
 * Move to Status Register.  Used from application code to alter or restore
 * condition flags in APSR.  The side effects are similar to the Test class,
 * but we model it separately so we can catch code trying to poke the reserved
 * APSR bits.
 *
 * Includes:
 * MSR(immediate), MSR(immediate), MSR(register)
 */
class MoveToStatusRegister : public ClassDecoder {
 public:
  virtual ~MoveToStatusRegister() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual RegisterList defs(Instruction i) const;
};

/*
 * A base+immediate store, of unspecified width.  (We don't care whether it
 * stores one byte or 64.)
 *
 * Includes:
 * STRH(immediate), STRD(immediate), STR(immediate), STRB(immediate),
 * STMDA / STMED, STM / STMIA / STMEA, STMDB / STMFD, STMIB / STMFA
 */
class StoreImmediate : public ClassDecoder {
 public:
  virtual ~StoreImmediate() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual bool writes_memory(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return true;
  }
  virtual Register base_address_register(Instruction i) const;
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
};

/*
 * A base+register store, of unspecified width.  (We don't care whether it
 * stores one byte or 64.)  Note that only register-post-indexing will pass
 * safety checks -- register pre-indexing is unpredictable to us.
 *
 * Includes:
 * STRH(register), STRD(register), STR(register), STRB(register)
 */
class StoreRegister : public ClassDecoder {
 public:
  virtual ~StoreRegister() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual bool writes_memory(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return true;
  }
  virtual Register base_address_register(Instruction i) const;
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
};

/*
 * STREX - a lot like a store, but with restricted addressing modes and more
 * register writes.  Unfortunately the encodings aren't compatible, so they
 * don't share code.
 *
 * Includes:
 * STREX, STREXD, STREXB, STREXH
 */
class StoreExclusive : public ClassDecoder {
 public:
  virtual ~StoreExclusive() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual bool writes_memory(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return true;
  }
  virtual Register base_address_register(Instruction i) const;
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
  // Defines the destination register.
  inline Register Rd(const Instruction& i) const {
    return i.reg(15, 12);
  }
};

/*
 * Abstract base class for single- and double-register load instructions,
 * below.  These instructions have common characteristics:
 * - They aren't permitted to alter PC.
 * - They produce a result in reg(15:12).
 */
class AbstractLoad : public ClassDecoder {
 public:
  virtual ~AbstractLoad() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 protected:
  bool writeback(Instruction i) const;
  // Defines the destination register.
  inline Register Rt(const Instruction& i) const {
    return i.reg(15, 12);
  }
};

/*
 * Loads using a register displacement, which may affect Rt (the destination)
 * and Rn (the base address, if writeback is used).
 *
 * Notice we do not care about the width of the loaded value, because it doesn't
 * affect addressing.
 *
 * Includes:
 * LDRH(register), LDRSB(register), LDRSH(register), LDR(register),
 * LDRB(register)
 */
class LoadRegister : public AbstractLoad {
 public:
  virtual ~LoadRegister() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
};

/*
 * Loads using an immediate displacement, which may affect Rt (the destination)
 * and Rn (the base address, if writeback is used).
 *
 * Notice we do not care about the width of the loaded value, because it doesn't
 * affect addressing.
 *
 * Includes:
 * LDRH(immediate), LDRH(literal), LDRSB(immediate), LDRSB(literal),
 * LDRSH(immediate), LDRSH(literal), LDR(immediate), LDR(literal),
 * LDRB(immediate), LDRB(literal)
 */
class LoadImmediate : public AbstractLoad {
 public:
  virtual ~LoadImmediate() {}

  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  virtual bool offset_is_immediate(Instruction i) const;
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
};

/*
 * Two-register immediate-offset load, which also writes Rt+1.
 *
 * Includes:
 * LDRD(immediate), LDRD(literal)
 */
class LoadDoubleI : public LoadImmediate {
 public:
  virtual ~LoadDoubleI() {}

  virtual RegisterList defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  virtual bool offset_is_immediate(Instruction i) const;
  // Defines the second destination register.
  inline Register Rt2(const Instruction& i) const {
    return Register(Rt(i).number() + 1);
  }
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
};

/*
 * Two-register register-offset load, which also writes Rt+1.
 *
 * Includes:
 * LDRD(register)
 */
class LoadDoubleR : public LoadRegister {
 public:
  virtual ~LoadDoubleR() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the second destination register.
  inline Register Rt2(const Instruction& i) const {
    return Register(Rt(i).number() + 1);
  }
};

/*
 * LDREX and friends, where writeback is unavailable.
 *
 * Includes:
 * LDREX, LDREXB, LDREXH
 */
class LoadExclusive : public AbstractLoad {
 public:
  virtual ~LoadExclusive() {}
  virtual Register base_address_register(Instruction i) const;
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
};

/*
 * LDREXD, which also writes Rt+1.
 */
class LoadDoubleExclusive : public LoadExclusive {
 public:
  virtual ~LoadDoubleExclusive() {}

  virtual RegisterList defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the second destination register.
  inline Register Rt2(const Instruction& i) const {
    return Register(Rt(i).number() + 1);
  }
};

/*
 * And, finally, the oddest class of loads: LDM.  In addition to the base
 * register, this may write every other register, subject to a bitmask.
 *
 * Includes:
 * LDMDA / LDMFA, LDM / LDMIA / LDMFD, LDMDB / LDMEA, LDMIB / LDMED
 */
class LoadMultiple : public ClassDecoder {
 public:
  virtual ~LoadMultiple() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
};

/*
 * A load to a vector register.  Like LoadCoprocessor below, the only visible
 * side effect is from writeback. Note: Implements VLD1, VLD2, VLD3, and VLD4.
 *
 * Includes:
 * VLD1(multiple), VLD2(multiple), VLD3(multiple), VLD4(multiple),
 * VLD1(single), VLD1(single, all lanes), VLD2(single),
 * VLD2(single, all lanes), VLD3(single), VLD3(single, all lanes),
 * VLD4(single), VLD4(single, all lanes)
 */
class VectorLoad : public ClassDecoder {
 public:
  virtual ~VectorLoad() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  /* Defines the base address for the access. */
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
  /* Contains an address offset applied after the access. */
  inline Register Rm(const Instruction& i) const {
    return i.reg(3, 0);
  }
};

/*
 * A store from a vector register.
 *
 * Includes:
 * VST1(multiple), VST2(multiple), VST3(multiple), VST4(multiple),
 * VST1(single), VST2(single), VST3(single), VST4(single)
 */
class VectorStore : public ClassDecoder {
 public:
  virtual ~VectorStore() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual bool writes_memory(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  /* Defines the base address for the access. */
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
  // Contains an address offset applied after the access.
  inline Register Rm(const Instruction& i) const {
    return i.reg(3, 0);
  }
};

/*
 * A generic coprocessor instruction that (by default) has no side effects.
 * These instructions are:
 * - Permitted only for whitelisted coprocessors that we've analyzed;
 * - Not permitted to update r15.
 *
 * Coprocessor ops with visible side-effects should extend and override this.
 *
 * Includes:
 * MCRR, MCRR2, CDP, CDP2, MCR, MCR2, MCRR, MCRR2, CDP, CDP2, MCR, MCR2
 */
class CoprocessorOp : public ClassDecoder {
 public:
  virtual ~CoprocessorOp() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }
  // Returns the name (i.e. index) of the coprocessor referenced.
  inline uint32_t CoprocIndex(const Instruction& i) const {
    return i.bits(11, 8);
  }
};

/*
 * LDC/LDC2, which load data from memory directly into a coprocessor.
 * The only visible side effect of this is optional indexing writeback,
 * controlled by bit 21.
 *
 * Includes:
 * LDC(immediate), LDC2(immediate), LDC(literal), LDC2(literal),
 * LDC(immed), LDC2(immed), LDC(literal), LDC2(literal),
 * LDC(literal), LDC2(literal)
 */
class LoadCoprocessor : public CoprocessorOp {
 public:
  virtual ~LoadCoprocessor() {}

  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  // Contains the base address for the access.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
};

/*
 * STC/STC2, which store data from a coprocessor into RAM.  Fortunately the
 * base address is not a coprocessor register!
 *
 * The side effects of this are identical to LoadCoprocessor, except of course
 * that it writes memory.  Unfortunately, the size of the memory write is not
 * defined in the ISA, but by the coprocessor.  Thus, we wind up having to
 * whitelist certain cases of this on known coprocessor types (see the impl).
 */
class StoreCoprocessor : public CoprocessorOp {
 public:
  virtual ~StoreCoprocessor() {}

  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual bool writes_memory(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return true;
  }
  virtual Register base_address_register(Instruction i) const;
  // Contains the base address for the access.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
};

/*
 * MRC/MRC2, which load a single register from a coprocessor register.
 */
class MoveFromCoprocessor : public CoprocessorOp {
 public:
  virtual ~MoveFromCoprocessor() {}

  virtual RegisterList defs(Instruction i) const;

  // Defines the destination core register.
  inline Register Rt(const Instruction& i) const {
    Register rt = i.reg(15, 12);
    if (rt == kRegisterPc) {
      // Per ARM ISA spec: r15 here is a synonym for the flags register!
      return kRegisterFlags;
    }
    return rt;
  }
};

/*
 * MRRC/MRRC2, which load pairs of registers from a coprocessor register.
 */
class MoveDoubleFromCoprocessor : public CoprocessorOp {
 public:
  virtual ~MoveDoubleFromCoprocessor() {}

  virtual RegisterList defs(Instruction i) const;
  // Contains the first destination core register.
  inline Register Rt(const Instruction& i) const {
    return i.reg(15, 12);
  }
  // Contains the second destination core register.
  inline Register Rt2(const Instruction& i) const {
    return i.reg(19, 16);
  }
};

/*
 * BX and BLX - everyone's favorite register-indirect branch.
 * This implementation makes assumptions about where the L bit is located, and
 * should not be used for other indirect branches (such as mov pc, rN).
 * Hence the cryptic name.
 */
class BxBlx : public ClassDecoder {
 public:
  virtual ~BxBlx() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual RegisterList defs(Instruction i) const;
  virtual Register branch_target_register(Instruction i) const;
  // Defines flag that indicates that Link register is used as well.
  inline bool UsesLinkRegister(const Instruction& i) const {
    return i.bit(5);
  }
  // Contains branch target address and instruction set selection bit.
  inline Register Rm(const Instruction& i) const {
    return i.reg(3, 0);
  }
};

/*
 * B and BL
 *
 * This implementation makes assumptions about where the L bit is located, but
 * the assumption holds for all non-illegal direct branches.
 */
class Branch : public ClassDecoder {
 public:
  virtual ~Branch() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual RegisterList defs(Instruction i) const;
  virtual bool is_relative_branch(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return true;
  }
  virtual int32_t branch_target_offset(Instruction i) const;
};

}  // namespace

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_INST_CLASSES_H
