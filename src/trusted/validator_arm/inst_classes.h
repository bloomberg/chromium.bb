/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_INST_CLASSES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_INST_CLASSES_H_

#include <stdint.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/validator_arm/model.h"

/*
 * Models the "instruction classes" that the decoder produces.
 */
namespace nacl_arm_dec {

// Used to describe whether an instruction is safe, and if not, what the issue
// is.  Only instructions that MAY_BE_SAFE should be allowed in untrusted code,
// and even those may be rejected by the validator.
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

  // This instruction may be safe in untrusted code: in isolation it contains
  // nothing scary, but the validator may overrule this during global analysis.
  MAY_BE_SAFE
};

// ------------------------------------------------------------------
// The following list of Interface classes are "mixed" into the class
// decoders below as static fields. The point of these interfaces is
// to control access to data fields within the instruction the class
// decoder, using higher level symbolic names.
//
// For example, register Rn may be located in different bit sequences
// in different instructions. However, they all refer to the concept
// of register Rn (some use bits 0..3 while others use bits
// 16..19). The interfaces for each possible Rn is integrated as a
// static field named n_. Hence virtuals can now use n_.reg() to get
// the corresponding register within its virtual functions.
//
// Note: These fields are static, and the corresponding methods are
// static so that the reference to the static field can be optimized
// out.
// ------------------------------------------------------------------

// Interface class to pull out shift type from bits 5 through 6
class ShiftTypeBits5To6Interface {
 public:
  inline ShiftTypeBits5To6Interface() {}
  inline uint32_t value(const Instruction& i) const {
    return i.bits(6, 5);
  }
  // Converts the given immediate value using the shift type specified
  // by this interface. Defined in A8.4.3, page A8-11.
  inline uint32_t DecodeImmShift(Instruction insn, uint32_t imm5_value) const {
    return ComputeDecodeImmShift(value(insn), imm5_value);
  }
 private:
  static uint32_t ComputeDecodeImmShift(uint32_t shift_type,
                                        uint32_t imm5_value);
  NACL_DISALLOW_COPY_AND_ASSIGN(ShiftTypeBits5To6Interface);
};

// Interface class to pull out the condition in bits 28 through 31
class ConditionBits28To31Interface {
 public:
  inline ConditionBits28To31Interface() {}
  inline uint32_t value(const Instruction& i) const {
    return i.bits(31, 28);
  }
  inline bool defined(const Instruction& i) const {
    return value(i) != 0xF;
  }
  inline bool undefined(const Instruction& i) const {
    return !defined(i);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ConditionBits28To31Interface);
};

// Interface class to pull out Register D from bits 12 through 15.
class RegDBits12To15Interface {
 public:
  inline RegDBits12To15Interface() {}
  inline uint32_t number(const Instruction& i) const {
    return i.bits(15, 12);
  }
  inline Register reg(const Instruction& i) const {
    return Register(number(i));
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(RegDBits12To15Interface);
};

// Interface class to pull out Register M from bits 0 through 3.
class RegMBits0To3Interface {
 public:
  inline RegMBits0To3Interface() {}
  inline uint32_t number(const Instruction& i) const {
    return i.bits(3, 0);
  }
  inline Register reg(const Instruction& i) const {
    return Register(number(i));
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(RegMBits0To3Interface);
};

// Interface class to pull out Register M from bits 8 through 11.
class RegMBits8To11Interface {
 public:
  inline RegMBits8To11Interface() {}
  inline uint32_t number(const Instruction& i) const {
    return i.bits(11, 8);
  }
  inline Register reg(const Instruction& i) const {
    return Register(number(i));
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(RegMBits8To11Interface);
};

// Interface class to pull out Register N from bits 0 through 3.
class RegNBits0To3Interface {
 public:
  inline RegNBits0To3Interface() {}
  inline uint32_t number(const Instruction& i) const {
    return i.bits(3, 0);
  }
  inline Register reg(const Instruction& i) const {
    return Register(number(i));
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(RegNBits0To3Interface);
};

// Interface class to pull out Register n from bits 16 through 19.
class RegNBits16To19Interface {
 public:
  inline RegNBits16To19Interface() {}
  inline uint32_t number(const Instruction& i) const {
    return i.bits(19, 16);
  }
  inline Register reg(const Instruction& i) const {
    return Register(number(i));
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(RegNBits16To19Interface);
};

// Interface class to pull out Register S from bits 8 through 11.
class RegSBits8To11Interface {
 public:
  inline RegSBits8To11Interface() {}
  inline uint32_t number(const Instruction& i) const {
    return i.bits(11, 8);
  }
  inline Register reg(const Instruction& i) const {
    return Register(number(i));
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(RegSBits8To11Interface);
};

// Interface class to pull out an immediate value in bits 0 through 11.
class Imm12Bits0To11Interface {
 public:
  inline Imm12Bits0To11Interface() {}
  inline uint32_t value(const Instruction& i) const {
    return i.bits(11, 0);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Imm12Bits0To11Interface);
};

// Interface class to pull out an immediate value in bits 7 through 11.
class Imm5Bits7To11Interface {
 public:
  inline Imm5Bits7To11Interface() {}
  inline uint32_t value(const Instruction& i) const {
    return i.bits(11, 7);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Imm5Bits7To11Interface);
};

// Interface class to pull out an immediate value in bits 16 through 19.
class Imm4Bits16To19Interface {
 public:
  inline Imm4Bits16To19Interface() {}
  inline uint32_t value(const Instruction& i) const {
    return i.bits(19, 16);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Imm4Bits16To19Interface);
};

// Interface class to pull out S (update) bit from bit 20, which
// defines if the flags register is updated by the instruction.
class UpdatesFlagsRegisterBit20Interface {
 public:
  inline UpdatesFlagsRegisterBit20Interface() {}
  // Returns true if bit is set that states that the flags register is updated.
  inline bool is_updated(const Instruction i) const {
    return i.bit(20);
  }
  // Returns the flags register if it is used.
  inline Register reg_if_updated(const Instruction i) const {
    return is_updated(i) ? kRegisterFlags : kRegisterNone;
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(UpdatesFlagsRegisterBit20Interface);
};

// A class decoder is designed to decode a set of instructions that
// have the same semantics, in terms of what the validator needs. This
// includes the bit ranges in the instruction that correspond to
// assigned registers.  as well as whether the instruction is safe to
// use within the validator.
//
// The important property of these class decoders is that the
// corresponding DecoderState (defined in decoder.h) will inspect the
// instruction bits and then dispatch the appropriate class decoder.
//
// The virtuals defined in this class are intended to be used solely
// for the purpose of the validator. For example, for virtual "defs",
// the class decoder will look at the bits defining the assigned
// register of the instruction (typically in bits 12 through 15) and
// add that register to the set of registers returned by the "defs"
// virtual.
//
// There is an underlying assumption that class decoders are constant
// and only provide implementation details for the instructions they
// should be applied to. In general, class decoders should not be
// copied or assigned. Hence, only a no-argument constructor should be
// provided.
class ClassDecoder {
 public:
  // Checks how safe this instruction is, in isolation.
  // This will detect any violation in the ARMv7 spec -- undefined encodings,
  // use of registers that are unpredictable -- and the most basic constraints
  // in our SFI model.  Because ClassDecoders are referentially-transparent and
  // cannot touch global state, this will not check things that may vary with
  // ABI version.
  //
  // The most positive result this can return is called MAY_BE_SAFE because it
  // is necessary, but not sufficient: the validator has the final say.
  virtual SafetyLevel safety(Instruction i) const = 0;

  // Gets the set of registers affected when an instruction executes.  This set
  // is complete, and includes
  //  - explicit destination register(s),
  //  - changes to flags,
  //  - indexed-addressing writeback,
  //  - changes to r15 by branches,
  //  - implicit register results, like branch-with-link.
  //
  // The default implementation returns a ridiculous bitmask that suggests that
  // all possible side effects will occur -- override if this is not
  // appropriate. :-)
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterListEverything;
  }

  // Gets the set of registers that this instruction defines through immediate
  // indexed addressing writeback -- a subset of the defs() set.
  //
  // This distinction is useful for operations like SP-relative loads, because
  // the maximum displacement that immediate addressing can produce is small.
  //
  // Note that this does not include defs produced by *register* indexed
  // addressing writeback, since they have no useful properties in our model.
  //
  // Stubbed to indicate that no such addressing occurs.
  virtual RegisterList immediate_addressing_defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }

  //
  // Checks whether the instruction can write to memory.  Note that we only
  // permit base+immediate addressing stores, so if the safety() looks good
  // and writes_memory() is true, you can assume base+immediate addressing is
  // being used.
  //
  // Stubbed to return 'false', which is the common case.
  virtual bool writes_memory(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return false;
  }

  // For instructions that can read or write memory, gets the register used as
  // the base for generating the effective address.
  //
  // It is stubbed to return nonsense.
  virtual Register base_address_register(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }

  // Checks whether the instruction computes its read or write address as
  // base address + immediate.
  //
  // It is stubbed to return false.
  virtual bool offset_is_immediate(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return false;
  }

  // For indirect branch instructions, returns the register being moved into
  // r15.  Otherwise, reports kRegisterNone.
  //
  // Note that this exclusively describes instructions that write r15 from a
  // register, unmodified.  This means BX, BLX, and MOV without shift.  Not
  // even BIC, which we allow to write to r15, is modeled this way.
  //
  virtual Register branch_target_register(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }

  // Checks whether the instruction is a direct relative branch -- meaning it
  // adds a constant offset to r15.
  virtual bool is_relative_branch(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return false;
  }

  // For relative branches, gets the offset added to the instruction's
  // virtual address to find the target.  The results are bogus unless
  // is_relative_branch() returns true.
  //
  // Note that this is different than the offset added to r15 at runtime, since
  // r15 reads as 8 bytes ahead.  This function does the math so you don't have
  // to.
  virtual int32_t branch_target_offset(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return 0;
  }

  // Checks whether this instruction is the special bit sequence that marks
  // the start of a literal pool.
  virtual bool is_literal_pool_head(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return false;
  }

  // Checks that an instruction clears a certain pattern of bits in all its
  // (non-flag) result registers.  The mask should include 1s in the positions
  // that should be cleared.
  virtual bool clears_bits(Instruction i, uint32_t mask) const {
    UNREFERENCED_PARAMETER(i);
    UNREFERENCED_PARAMETER(mask);
    return false;
  }

  // Checks that an instruction will set Z if certain bits in r (chosen by 1s in
  // the mask) are clear.
  //
  // Note that the inverse does not hold: the actual instruction i may require
  // *more* bits to be clear to set Z.  This is fine.
  virtual bool sets_Z_if_bits_clear(Instruction i,
                                    Register r,
                                    uint32_t mask) const {
    UNREFERENCED_PARAMETER(i);
    UNREFERENCED_PARAMETER(r);
    UNREFERENCED_PARAMETER(mask);
    return false;
  }

  // Many instructions define control bits in bits 20-24. The useful bits
  // are defined here.

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
  inline ClassDecoder() {}
  virtual ~ClassDecoder() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ClassDecoder);
};

//----------------------------------------------------------------
// The following class decoders define common cases, defining a
// concept that simply associates a non MAY_BE_SAFE with the
// instructions it processes. As such, they provide default
// implementation that returns the corresponding safety value, and
// assumes nothing else interesting happens.
//----------------------------------------------------------------

class UnsafeClassDecoder : public ClassDecoder {
 public:
  inline explicit UnsafeClassDecoder(SafetyLevel safety)
      : safety_(safety) {}
  virtual ~UnsafeClassDecoder() {}

  // Return the safety associated with this class.
  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return safety_;
  }

  // Switch off the def warnings -- it's already forbidden!
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }

 private:
  SafetyLevel safety_;
  NACL_DISALLOW_COPY_AND_ASSIGN(UnsafeClassDecoder);
};

class Forbidden : public UnsafeClassDecoder {
 public:
  inline Forbidden() : UnsafeClassDecoder(FORBIDDEN) {}
  virtual ~Forbidden() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Forbidden);
};

// Represents the undefined space in the instruction encoding.
class Undefined : public UnsafeClassDecoder {
 public:
  inline Undefined() : UnsafeClassDecoder(UNDEFINED) {}
  virtual ~Undefined() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Undefined);
};

// Represents instructions that have been deprecated in ARMv7.
class Deprecated : public UnsafeClassDecoder {
 public:
  inline Deprecated() : UnsafeClassDecoder(DEPRECATED) {}
  virtual ~Deprecated() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Deprecated);
};

// Represents an unpredictable encoding.  Note that many instructions may
// *become* unpredictable based on their operands -- this is used only for
// the case where a large space of the instruction set is unpredictable.
class Unpredictable : public UnsafeClassDecoder {
 public:
  inline Unpredictable() : UnsafeClassDecoder(UNPREDICTABLE) {}
  virtual ~Unpredictable() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unpredictable);
};

//------------------------------------------------------------------
// The following classes are the OLD implementations of class
// decoders. They are deprecated, and are in the process of being
// removed (or at least reduced).
//------------------------------------------------------------------

// An instruction that, for modeling purposes, is a no-op.  It has no
// side effects that are significant to SFI, and has no operand combinations
// that cause it to become unsafe.
//
// Uses of this class must be carefully evaluated, because it bypasses all
// further validation.
//
// Includes:
// NOP, YIELD, WFE, WFI, SEV, DBG, PLDW(immediate), PLD(immediate),
// PLD(literal), CLREX, DSB, DMB, ISB, PLI(register), PLDW(register),
// PLD(register), VEXT, VTBL, VTBX, VHADD, VQADD, VRHADD, VAND(register),
// VBIC(register), VORR(register), VORN(register), VEOR(register), VBSL,
// VBIT, VBIF, VHSUB, VQSUB, VCGT(register), VCGE(register), VSHL(register),
// VQSHL(register), VRSHL(register), VQRSHL(register), VMAX, VMIN (integer),
// VABD, VABDL (integer), VABA, VABAL, VADD(integer), VSUB(integer),
// VTST(integer), VCEQ(integer), VMLA, VMLAL, VMLS, VMLSL (integer),
// VMUL, VMULL(integer/poly), VPMAX, VPMIN(integer), VQDMULH, VQRDMULH,
// VPADD(integer), VADD(float), VSUB(float), VPADD(float), VABD(float),
// VMLA, VMLS(float), VMUL(float), VCEQ(register), VCGE(register),
// VCGT(register), VACGE, VACGT, VACLE, VACLT, VMAX, VMIN(float),
// VPMAX, VPMIN(float), VRECPS, VRSQRTS, VADDL, VSUBL, VADDHN, VRADDHN,
// VABA, VABAL, VSUBHN, VRSUBHN, VABD, VABDL(integer), VMLA, VMLAL,
// VMLS, VMLSL (integer), VQDMLAL, VQDMLSL, VMUL, VMULL (integer), VQDMULL,
// VMUL, VMULL (polynomial), VMLA, VMLS (scalar), VMLAL, VMLSL (scalar),
// VQDMLAL, VMQDLSL, VMUL(scalar), VMULL(scalar), VQDMULL, VQDMULH, VQRDMULH,
// VSHR, VSRA, VRSHR, VRSRA, VSRI, VSHL(immediate), VSLI,
// VQSHL, VQSHLU(immediate), VSHRN, VRSHRN, VQSHRUN, VQRSHRUN, VQSHRN, VQRSHRN,
// VSHLL, VMOVL, VCVT (floating- and fixed-point), VREV64, VREV32, VREV16,
// VPADDL, VCLS, VCLZ, VCNT, VMVN(register), VPADAL, VQABS, VQNEG,
// VCGT (immediate #0), VCGE (immediate #0), VCEQ (immediate #0),
// VCLE (immediate #0), VCLT (immediate #0), VABS, VNEG, VSWP, VTRN, VUZP,
// VZIP, VMOVN, VQMOVUN, VQMOVN, VSHLL, VCVT (half- and single-precision),
// VRECPE, VRSQRTE, VCVT (float and integer), VMOV(immediate),
// VORR(immediate), VMOV(immediate), VORR(immediate), VMOV(immediate),
// VMVN(immediate), VBIC(immediate), VMVN(immediate), VBIC(immediate),
// VMVN(immediate), VMOV(immediate)
//
class EffectiveNoOp : public ClassDecoder {
 public:
  inline EffectiveNoOp() {}
  virtual ~EffectiveNoOp() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(EffectiveNoOp);
};

// Models all instructions that reliably trap, preventing execution from falling
// through to the next instruction.  Note that roadblocks currently have no
// special role in the SFI model, so Breakpoints are distinguished below.
class Roadblock : public ClassDecoder {
 public:
  inline Roadblock() {}
  virtual ~Roadblock() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return kRegisterNone;
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Roadblock);
};

// BKPT
// We model this mostly so we can use it to recognize literal pools -- untrusted
// code isn't expected to use it, but it's not unsafe, and there are cases where
// we may generate it.
class Breakpoint : public Roadblock {
 public:
  inline Breakpoint() {}
  virtual ~Breakpoint() {}
  virtual bool is_literal_pool_head(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Breakpoint);
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
  static const Imm12Bits0To11Interface imm12_;
  static const RegDBits12To15Interface d_;
  static const Imm4Bits16To19Interface imm4_;
  static const UpdatesFlagsRegisterBit20Interface flags_;
  static const ConditionBits28To31Interface cond_;

  // Methods for class.
  inline Unary1RegisterImmediateOp() : ClassDecoder() {}
  virtual ~Unary1RegisterImmediateOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

  // The immediate value stored in the instruction.
  inline uint32_t ImmediateValue(const Instruction& i) const {
    return (imm4_.value(i) << 12) | imm12_.value(i);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterImmediateOp);
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
  static const RegMBits0To3Interface m_;
  static const RegDBits12To15Interface d_;
  static const UpdatesFlagsRegisterBit20Interface flags_;
  static const ConditionBits28To31Interface cond_;

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
  static const RegNBits0To3Interface n_;
  static const RegMBits8To11Interface m_;
  static const RegDBits12To15Interface d_;
  static const UpdatesFlagsRegisterBit20Interface flags_;
  static const ConditionBits28To31Interface cond_;

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
  static const RegMBits0To3Interface m_;
  static const ShiftTypeBits5To6Interface shift_type_;
  static const Imm5Bits7To11Interface imm_;
  static const RegDBits12To15Interface d_;
  static const UpdatesFlagsRegisterBit20Interface flags_;
  static const ConditionBits28To31Interface cond_;

  // Methods for class.
  inline Unary2RegisterImmedShiftedOp() : ClassDecoder() {}
  virtual ~Unary2RegisterImmedShiftedOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

  // The immediate value stored in the instruction.
  inline uint32_t ImmediateValue(const Instruction& i) const {
    return shift_type_.DecodeImmShift(i, imm_.value(i));
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
  static const RegMBits0To3Interface m_;
  static const ShiftTypeBits5To6Interface shift_type_;
  static const RegSBits8To11Interface s_;
  static const RegDBits12To15Interface d_;
  static const UpdatesFlagsRegisterBit20Interface flags_;
  static const ConditionBits28To31Interface cond_;

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
  static const RegMBits0To3Interface m_;
  static const ShiftTypeBits5To6Interface shift_type_;
  static const Imm5Bits7To11Interface imm_;
  static const RegDBits12To15Interface d_;
  static const RegNBits16To19Interface n_;
  static const UpdatesFlagsRegisterBit20Interface flags_;
  static const ConditionBits28To31Interface cond_;

  // Methods for class.
  inline Binary3RegisterImmedShiftedOp() : ClassDecoder() {}
  virtual ~Binary3RegisterImmedShiftedOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // The shift value to use.
  inline uint32_t ShiftValue(const Instruction& i) const {
    return shift_type_.DecodeImmShift(i, imm_.value(i));
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
  static const RegMBits0To3Interface m_;
  static const RegSBits8To11Interface s_;
  static const RegDBits12To15Interface d_;
  static const RegNBits16To19Interface n_;
  static const UpdatesFlagsRegisterBit20Interface flags_;
  static const ConditionBits28To31Interface cond_;

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
  static const RegMBits0To3Interface m_;
  static const ShiftTypeBits5To6Interface shift_type_;
  static const Imm5Bits7To11Interface imm_;
  static const RegNBits16To19Interface n_;
  static const UpdatesFlagsRegisterBit20Interface flags_;
  static const ConditionBits28To31Interface cond_;

  // Methods for class.
  inline Binary2RegisterImmedShiftedTest() : ClassDecoder() {}
  virtual ~Binary2RegisterImmedShiftedTest() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // The shift value to use.
  inline uint32_t ShiftValue(const Instruction& i) const {
    return shift_type_.DecodeImmShift(i, imm_.value(i));
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
  static const RegMBits0To3Interface m_;
  static const ShiftTypeBits5To6Interface shift_type_;
  static const RegSBits8To11Interface s_;
  static const RegNBits16To19Interface n_;
  static const UpdatesFlagsRegisterBit20Interface flags_;
  static const ConditionBits28To31Interface cond_;

  // Methods for class.
  inline Binary3RegisterShiftedTest() : ClassDecoder() {}
  virtual ~Binary3RegisterShiftedTest() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Binary3RegisterShiftedTest);
};

// Models the most common class of data processing instructions.  We use this
// for any operation that
//  - writes a single register, specified in 15:12;
//  - does not write memory,
//  - should not be permitted to cause a jump by writing r15,
//  - writes flags when bit 20 is set.
//
// Includes:
// MOVT,
// ROR(register),
// AND(immediate),
// EOR(immediate), SUB(immediate), ADR, RSB(immediate), ADD(immediate), ADR,
// ADC(immediate), SBC(immediate), RSC(immediate), ORR(immediate),
// MOV(immediate), MVN(immediate), MRS, CLZ, SBFX, BFC, BFI, UBFX, UADD16,
// UASX, USAX, USUB16, UADD8, USUB8, UQADD16, UQASX, UQSAX, UQSUB16, UQADD8,
// UQSUB8, UHADD16, UHASX, UHSAX, UHSUB16, UHADD8, UHSUB8
class DataProc : public ClassDecoder {
 public:
  inline DataProc() : ClassDecoder() {}
  virtual ~DataProc() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // Defines the destination register of the data operation.
  inline Register Rd(const Instruction& i) const {
    return i.reg(15, 12);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(DataProc);
};

// Models the few data-processing instructions that *don't* produce a result,
// but may still set flags.
//
// Includes:
// TST(register), TEQ(register),
// CMP(register), CMN(register),
// TST(immediate), TEQ(immediate), CMP(immediate), CMN(immediate)
class Test : public DataProc {
 public:
  inline Test() {}
  virtual ~Test() {}
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Test);
};

// Specifically models the TST register-immediate instruction, which is
// important to our conditional store sandbox.
class TestImmediate : public Test {
 public:
  inline TestImmediate() {}
  virtual ~TestImmediate() {}

  virtual bool sets_Z_if_bits_clear(Instruction i,
                                    Register r,
                                    uint32_t mask) const;
  // Defines the operand register.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(TestImmediate);
};

// A special-case data processing instruction: the immediate BIC.  We consider
// this the only instruction that reliably clears bits in its result, so we
// model it separately from other logic ops.
class ImmediateBic : public DataProc {
 public:
  inline ImmediateBic() {}
  virtual ~ImmediateBic() {}

  // ImmediateBic is exempted from the writes-r15 check.
  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual bool clears_bits(Instruction i, uint32_t mask) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ImmediateBic);
};

// Models the Pack/Saturate/Reverse instructions, which
//  - Write a register identified by 15:12,
//  - Are not permitted to affect r15,
//  - Always set flags.
//
//  Note that not all of the Pack/Sat/Rev instructions really set flags, but
//  we deliberately err on the side of caution to simplify modeling (because we
//  don't care, in practice, about their flag effects).
//
// Includes:
// PKH, SSAT, USAT, SXTAB16, SXTB16, SEL, SSAT16, SXTAB, SXTB, REV,
// SXTAH, SXTH, REV16, UXTAB16, UXTB16, USAT16, UXTAB, UXTB, RBIT,
// UXTAH, UXTH, REVSH
//
class PackSatRev : public ClassDecoder {
 public:
  inline PackSatRev() {}
  virtual ~PackSatRev() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // Defines the destination register.
  inline Register Rd(const Instruction& i) const {
    return i.reg(15, 12);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PackSatRev);
};


// Models multiply instructions, which
//  - Write a register identified by 19:16,
//  - Are not permitted to affect r15,
//  - Always set flags.
//
// Note that multiply instructions don't *really* always set the flags.  We
// deliberately take this shortcut to simplify modeling of the many, many
// multiply instructions -- some of which always set flags, some of which never
// do, and some of which set conditionally.
//
// Includes:
// MUL, MLA, MLS, SMLABB et al., SMLAWB et al., SMULWB et al., SMULBB et al.,
// USAD8, USADA8, SMLAD, SMUAD, SMLSD, SMUSD, SMMLA, SMMUL, SMMLS
class Multiply : public ClassDecoder {
 public:
  inline Multiply() {}
  virtual ~Multiply() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // Defines the destination register.
  inline Register Rd(const Instruction& i) const {
    return i.reg(19, 16);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Multiply);
};

// Models double-word multiply instructions, which
//  - Write TWO registers, identified by 19:16 and 15:12,
//  - Are not permitted to affect r15,
//  - Always set flags.
//
// Note that multiply instructions don't *really* always set the flags.  We
// deliberately take this shortcut to simplify modeling of the many, many
// multiply instructions -- some of which always set flags, some of which never
// do, and some of which set conditionally.
//
// Includes:
// UMAAL, UMULL, UMLAL, SMULL, SMLAL, SMLALBB, SMLALD, SMLSLD
//
class LongMultiply : public Multiply {
 public:
  inline LongMultiply() {}
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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LongMultiply);
};

// Saturating adds and subtracts.  Conceptually equivalent to DataProc,
// except for the use of the S bit (bit 20) -- they always set flags.
//
// Includes:
// QADD, QSUB, QDADD, QDSUB
//
class SatAddSub : public DataProc {
 public:
  inline SatAddSub() {}
  virtual ~SatAddSub() {}

  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(SatAddSub);
};

// Move to Status Register.  Used from application code to alter or restore
// condition flags in APSR.  The side effects are similar to the Test class,
// but we model it separately so we can catch code trying to poke the reserved
// APSR bits.
//
// Includes:
// MSR(immediate), MSR(immediate), MSR(register)
//
class MoveToStatusRegister : public ClassDecoder {
 public:
  inline MoveToStatusRegister() {}
  virtual ~MoveToStatusRegister() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MoveToStatusRegister);
};

// A base+immediate store, of unspecified width.  (We don't care whether it
// stores one byte or 64.)
//
// Includes:
// STRH(immediate), STRD(immediate), STR(immediate), STRB(immediate),
// STMDA / STMED, STM / STMIA / STMEA, STMDB / STMFD, STMIB / STMFA
class StoreImmediate : public ClassDecoder {
 public:
  inline StoreImmediate() {}
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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreImmediate);
};

// A base+register store, of unspecified width.  (We don't care whether it
// stores one byte or 64.)  Note that only register-post-indexing will pass
// safety checks -- register pre-indexing is unpredictable to us.
//
// Includes:
// STRH(register), STRD(register), STR(register), STRB(register)
class StoreRegister : public ClassDecoder {
 public:
  inline StoreRegister() {}
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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreRegister);
};

// STREX - a lot like a store, but with restricted addressing modes and more
// register writes.  Unfortunately the encodings aren't compatible, so they
// don't share code.
//
// Includes:
// STREX, STREXD, STREXB, STREXH
class StoreExclusive : public ClassDecoder {
 public:
  inline StoreExclusive() {}
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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreExclusive);
};

// Abstract base class for single- and double-register load instructions,
// below.  These instructions have common characteristics:
// - They aren't permitted to alter PC.
// - They produce a result in reg(15:12).
class AbstractLoad : public ClassDecoder {
 public:
  inline AbstractLoad() {}
  virtual ~AbstractLoad() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 protected:
  bool writeback(Instruction i) const;
  // Defines the destination register.
  inline Register Rt(const Instruction& i) const {
    return i.reg(15, 12);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(AbstractLoad);
};

// Loads using a register displacement, which may affect Rt (the destination)
// and Rn (the base address, if writeback is used).
//
// Notice we do not care about the width of the loaded value, because it doesn't
// affect addressing.
//
// Includes:
// LDRH(register), LDRSB(register), LDRSH(register), LDR(register),
// LDRB(register)
class LoadRegister : public AbstractLoad {
 public:
  inline LoadRegister() {}
  virtual ~LoadRegister() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadRegister);
};

// Loads using an immediate displacement, which may affect Rt (the destination)
// and Rn (the base address, if writeback is used).
//
// Notice we do not care about the width of the loaded value, because it doesn't
// affect addressing.
//
// Includes:
// LDRH(immediate), LDRH(literal), LDRSB(immediate), LDRSB(literal),
// LDRSH(immediate), LDRSH(literal), LDR(immediate), LDR(literal),
// LDRB(immediate), LDRB(literal)
class LoadImmediate : public AbstractLoad {
 public:
  inline LoadImmediate() {}
  virtual ~LoadImmediate() {}

  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  virtual bool offset_is_immediate(Instruction i) const;
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadImmediate);
};

// Two-register immediate-offset load, which also writes Rt+1.
//
// Includes:
// LDRD(immediate), LDRD(literal)
class LoadDoubleI : public LoadImmediate {
 public:
  inline LoadDoubleI() {}
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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadDoubleI);
};

// Two-register register-offset load, which also writes Rt+1.
//
// Includes:
// LDRD(register)
class LoadDoubleR : public LoadRegister {
 public:
  inline LoadDoubleR() {}
  virtual ~LoadDoubleR() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the second destination register.
  inline Register Rt2(const Instruction& i) const {
    return Register(Rt(i).number() + 1);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadDoubleR);
};

// LDREX and friends, where writeback is unavailable.
//
// Includes:
// LDREX, LDREXB, LDREXH
class LoadExclusive : public AbstractLoad {
 public:
  inline LoadExclusive() {}
  virtual ~LoadExclusive() {}
  virtual Register base_address_register(Instruction i) const;
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadExclusive);
};

// LDREXD, which also writes Rt+1.
class LoadDoubleExclusive : public LoadExclusive {
 public:
  inline LoadDoubleExclusive() {}
  virtual ~LoadDoubleExclusive() {}

  virtual RegisterList defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the second destination register.
  inline Register Rt2(const Instruction& i) const {
    return Register(Rt(i).number() + 1);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadDoubleExclusive);
};

// And, finally, the oddest class of loads: LDM.  In addition to the base
// register, this may write every other register, subject to a bitmask.
//
// Includes:
// LDMDA / LDMFA, LDM / LDMIA / LDMFD, LDMDB / LDMEA, LDMIB / LDMED
class LoadMultiple : public ClassDecoder {
 public:
  inline LoadMultiple() {}
  virtual ~LoadMultiple() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadMultiple);
};

// A load to a vector register.  Like LoadCoprocessor below, the only visible
// side effect is from writeback. Note: Implements VLD1, VLD2, VLD3, and VLD4.
//
// Includes:
// VLD1(multiple), VLD2(multiple), VLD3(multiple), VLD4(multiple),
// VLD1(single), VLD1(single, all lanes), VLD2(single),
// VLD2(single, all lanes), VLD3(single), VLD3(single, all lanes),
// VLD4(single), VLD4(single, all lanes)
class VectorLoad : public ClassDecoder {
 public:
  inline VectorLoad() {}
  virtual ~VectorLoad() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the base address for the access.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
  // Contains an address offset applied after the access.
  inline Register Rm(const Instruction& i) const {
    return i.reg(3, 0);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoad);
};

// A store from a vector register.
//
// Includes:
// VST1(multiple), VST2(multiple), VST3(multiple), VST4(multiple),
// VST1(single), VST2(single), VST3(single), VST4(single)
class VectorStore : public ClassDecoder {
 public:
  inline VectorStore() {}
  virtual ~VectorStore() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual bool writes_memory(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the base address for the access.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }
  // Contains an address offset applied after the access.
  inline Register Rm(const Instruction& i) const {
    return i.reg(3, 0);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorStore);
};

// A generic coprocessor instruction that (by default) has no side effects.
// These instructions are:
// - Permitted only for whitelisted coprocessors that we've analyzed;
// - Not permitted to update r15.
//
// Coprocessor ops with visible side-effects should extend and override this.
//
// Includes:
// MCRR, MCRR2, CDP, CDP2, MCR, MCR2, MCRR, MCRR2, CDP, CDP2, MCR, MCR2
class CoprocessorOp : public ClassDecoder {
 public:
  inline CoprocessorOp() {}
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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(CoprocessorOp);
};

// LDC/LDC2, which load data from memory directly into a coprocessor.
// The only visible side effect of this is optional indexing writeback,
// controlled by bit 21.
//
// Includes:
// LDC(immediate), LDC2(immediate), LDC(literal), LDC2(literal),
// LDC(immed), LDC2(immed), LDC(literal), LDC2(literal),
// LDC(literal), LDC2(literal)
class LoadCoprocessor : public CoprocessorOp {
 public:
  inline LoadCoprocessor() {}
  virtual ~LoadCoprocessor() {}

  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  // Contains the base address for the access.
  inline Register Rn(const Instruction& i) const {
    return i.reg(19, 16);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadCoprocessor);
};

// STC/STC2, which store data from a coprocessor into RAM.  Fortunately the
// base address is not a coprocessor register!
//
// The side effects of this are identical to LoadCoprocessor, except of course
// that it writes memory.  Unfortunately, the size of the memory write is not
// defined in the ISA, but by the coprocessor.  Thus, we wind up having to
// whitelist certain cases of this on known coprocessor types (see the impl).
class StoreCoprocessor : public CoprocessorOp {
 public:
  inline StoreCoprocessor() {}
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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreCoprocessor);
};

// MRC/MRC2, which load a single register from a coprocessor register.
class MoveFromCoprocessor : public CoprocessorOp {
 public:
  inline MoveFromCoprocessor() {}
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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MoveFromCoprocessor);
};

// MRRC/MRRC2, which load pairs of registers from a coprocessor register.
class MoveDoubleFromCoprocessor : public CoprocessorOp {
 public:
  inline MoveDoubleFromCoprocessor() {}
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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MoveDoubleFromCoprocessor);
};

// BX and BLX - everyone's favorite register-indirect branch.
// This implementation makes assumptions about where the L bit is located, and
// should not be used for other indirect branches (such as mov pc, rN).
// Hence the cryptic name.
class BxBlx : public ClassDecoder {
 public:
  inline BxBlx() {}
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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BxBlx);
};

// B and BL
//
// This implementation makes assumptions about where the L bit is located, but
// the assumption holds for all non-illegal direct branches.
class Branch : public ClassDecoder {
 public:
  inline Branch() {}
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

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Branch);
};

}  // namespace

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_INST_CLASSES_H_
