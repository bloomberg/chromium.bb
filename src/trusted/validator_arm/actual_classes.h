/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_ACTUAL_CLASSES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_ACTUAL_CLASSES_H_

#include "native_client/src/trusted/validator_arm/inst_classes.h"

/*
 * Models the "instruction classes" that the decoder produces. Note:
 * In general, we merge baseline classes (see baseline_classes.h for
 * more details) into a single actual decoder class. The goal of the
 * actual classes is to capture the virtual API needed by the
 * validator, and nothing more. This allows us to (potentially) use a
 * smaller set of class decoders than the baseline. See
 * actual_vs_basline.h for details on how we test the actual class
 * decoders do what we expect.
 *
 * Note: This file is currently under reconstruction. It reflects the
 * class hierachy used before we introduced a baseline, which has
 * shown some issues with the choices made here. Class decoders will
 * be added, removed, or editted as we find what choices work.
 *
 * TODO(karl): finish updating this file to match what we want for the
 *             arm validator.
 */
namespace nacl_arm_dec {

// **************************************************************
//      N E W    C L A S S    D E C O D E R S
// **************************************************************

// Defines a class decoder that only worries about whether condition
// flags (S - bit(20)) is set.
class MaybeSetsConds : public ClassDecoder {
 public:
  inline MaybeSetsConds() {}
  virtual ~MaybeSetsConds() {}

  static const UpdatesConditionsBit20Interface conditions;

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MaybeSetsConds);
};

// Defines a default (assignment) class decoder, which assumes that
// the instruction is unsafe if it assigns register PC.
class NoPcAssignClassDecoder : public MaybeSetsConds {
 public:
  inline NoPcAssignClassDecoder() {}
  virtual ~NoPcAssignClassDecoder() {}

  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(NoPcAssignClassDecoder);
};

// Computes a value and stores in in Rd (bits 12-15). Doesn't
// allow Rd=PC.
class Defs12To15 : public NoPcAssignClassDecoder {
 public:
  inline Defs12To15() {}
  virtual ~Defs12To15() {}

  // We use the following Rd to capture the register being set.
  static const RegDBits12To15Interface d;

  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs12To15);
};

// Defs12To15 where registers Rn, Rd, Rs, and Rm are not Pc.
// Note: Some instructions may use other names for the registers,
// but they have the same placement within the instruction, and
// hence do not need a separate class decoder.
class Defs12To15RdRnRsRmNotPc : public Defs12To15 {
 public:
  // We use the following Rm, Rs, and Rn to capture the
  // registers that need checking.
  static const RegMBits0To3Interface m;
  static const RegSBits8To11Interface s;
  static const RegNBits16To19Interface n;

  inline Defs12To15RdRnRsRmNotPc() : Defs12To15() {}
  virtual ~Defs12To15RdRnRsRmNotPc() {}

  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs12To15RdRnRsRmNotPc);
};

// Defines test masking instruction to be used with a
// load/store (eq condition) to access memory.
class TestIfAddressMasked : public NoPcAssignClassDecoder {
 public:
  static const Imm12Bits0To11Interface imm12;
  static const RegNBits16To19Interface n;

  inline TestIfAddressMasked() {}
  virtual ~TestIfAddressMasked() {}
  virtual RegisterList defs(Instruction i) const;
  virtual bool sets_Z_if_bits_clear(Instruction i,
                                    Register r,
                                    uint32_t mask) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(TestIfAddressMasked);
};

// Defines the masking instruction to be used with
//  a load/store to conditionally access memory.
class MaskAddress : public Defs12To15 {
 public:
  static const Imm12Bits0To11Interface imm12;

  inline MaskAddress() {}
  virtual ~MaskAddress() {}
  virtual bool clears_bits(Instruction i, uint32_t mask) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MaskAddress);
};

// **************************************************************
//      O L D    C L A S S    D E C O D E R S
// **************************************************************

class OldClassDecoder : public ClassDecoder {
 public:
  inline OldClassDecoder() : ClassDecoder() {}
  virtual ~OldClassDecoder() {}

  // Many instructions define control bits in bits 20-24. The useful bits
  // are defined here.

  // True if S (updates condition flags in APSR register) flag is defined.
  inline bool UpdatesConditions(const Instruction& i) const {
    return i.Bit(20);
  }

  // True if W (does write) flag is defined.
  inline bool WritesFlag(const Instruction& i) const {
    return i.Bit(21);
  }
  // True if P (pre-indexing) flag is defined.
  inline bool PreindexingFlag(const Instruction& i) const {
    return i.Bit(24);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(OldClassDecoder);
};

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
class EffectiveNoOp : public OldClassDecoder {
 public:
  inline EffectiveNoOp() {}
  virtual ~EffectiveNoOp() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return RegisterList();
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(EffectiveNoOp);
};

// Models all instructions that reliably trap, preventing execution from falling
// through to the next instruction.  Note that roadblocks currently have no
// special role in the SFI model, so Breakpoints are distinguished below.
class Roadblock : public OldClassDecoder {
 public:
  inline Roadblock() {}
  virtual ~Roadblock() {}

  virtual SafetyLevel safety(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return MAY_BE_SAFE;
  }
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return RegisterList();
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
class DataProc : public OldClassDecoder {
 public:
  inline DataProc() : OldClassDecoder() {}
  virtual ~DataProc() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // Defines the destination register of the data operation.
  inline Register Rd(const Instruction& i) const {
    return i.Reg(15, 12);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(DataProc);
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
class PackSatRev : public OldClassDecoder {
 public:
  inline PackSatRev() {}
  virtual ~PackSatRev() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // Defines the destination register.
  inline Register Rd(const Instruction& i) const {
    return i.Reg(15, 12);
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
class Multiply : public OldClassDecoder {
 public:
  inline Multiply() {}
  virtual ~Multiply() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  // Defines the destination register.
  inline Register Rd(const Instruction& i) const {
    return i.Reg(19, 16);
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
    return i.Reg(15, 12);
  }
  // Supplies the other 32 bits of the destination result.
  inline Register RdHi(const Instruction& i) const {
    return i.Reg(19, 16);
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
class MoveToStatusRegister : public OldClassDecoder {
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
class StoreImmediate : public OldClassDecoder {
 public:
  inline StoreImmediate() {}
  virtual ~StoreImmediate() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.Reg(19, 16);
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
class StoreRegister : public OldClassDecoder {
 public:
  inline StoreRegister() {}
  virtual ~StoreRegister() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.Reg(19, 16);
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
class StoreExclusive : public OldClassDecoder {
 public:
  inline StoreExclusive() {}
  virtual ~StoreExclusive() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.Reg(19, 16);
  }
  // Defines the destination register.
  inline Register Rd(const Instruction& i) const {
    return i.Reg(15, 12);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreExclusive);
};

// Abstract base class for single- and double-register load instructions,
// below.  These instructions have common characteristics:
// - They aren't permitted to alter PC.
// - They produce a result in reg(15:12).
class AbstractLoad : public OldClassDecoder {
 public:
  inline AbstractLoad() {}
  virtual ~AbstractLoad() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 protected:
  bool writeback(Instruction i) const;
  // Defines the destination register.
  inline Register Rt(const Instruction& i) const {
    return i.Reg(15, 12);
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
    return i.Reg(19, 16);
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
    return i.Reg(19, 16);
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
    return i.Reg(19, 16);
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
    return i.Reg(19, 16);
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
class LoadMultiple : public OldClassDecoder {
 public:
  inline LoadMultiple() {}
  virtual ~LoadMultiple() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the base register.
  inline Register Rn(const Instruction& i) const {
    return i.Reg(19, 16);
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
class VectorLoad : public OldClassDecoder {
 public:
  inline VectorLoad() {}
  virtual ~VectorLoad() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the base address for the access.
  inline Register Rn(const Instruction& i) const {
    return i.Reg(19, 16);
  }
  // Contains an address offset applied after the access.
  inline Register Rm(const Instruction& i) const {
    return i.Reg(3, 0);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorLoad);
};

// A store from a vector register.
//
// Includes:
// VST1(multiple), VST2(multiple), VST3(multiple), VST4(multiple),
// VST1(single), VST2(single), VST3(single), VST4(single)
class VectorStore : public OldClassDecoder {
 public:
  inline VectorStore() {}
  virtual ~VectorStore() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList immediate_addressing_defs(Instruction i) const;
  virtual Register base_address_register(Instruction i) const;
  // Defines the base address for the access.
  inline Register Rn(const Instruction& i) const {
    return i.Reg(19, 16);
  }
  // Contains an address offset applied after the access.
  inline Register Rm(const Instruction& i) const {
    return i.Reg(3, 0);
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
class CoprocessorOp : public OldClassDecoder {
 public:
  inline CoprocessorOp() {}
  virtual ~CoprocessorOp() {}

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const {
    UNREFERENCED_PARAMETER(i);
    return RegisterList();
  }
  // Returns the name (i.e. index) of the coprocessor referenced.
  inline uint32_t CoprocIndex(const Instruction& i) const {
    return i.Bits(11, 8);
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
    return i.Reg(19, 16);
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
  virtual Register base_address_register(Instruction i) const;
  // Contains the base address for the access.
  inline Register Rn(const Instruction& i) const {
    return i.Reg(19, 16);
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
    Register rt(i.Reg(15, 12));
    if (rt.Equals(kRegisterPc)) {
      // Per ARM ISA spec: r15 here is a synonym for the flags register!
      return kConditions;
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
    return i.Reg(15, 12);
  }
  // Contains the second destination core register.
  inline Register Rt2(const Instruction& i) const {
    return i.Reg(19, 16);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MoveDoubleFromCoprocessor);
};

// BX and BLX - everyone's favorite register-indirect branch.
// This implementation makes assumptions about where the L bit is located, and
// should not be used for other indirect branches (such as mov pc, rN).
// Hence the cryptic name.
class BxBlx : public OldClassDecoder {
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
    return i.Bit(5);
  }
  // Contains branch target address and instruction set selection bit.
  inline Register Rm(const Instruction& i) const {
    return i.Reg(3, 0);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BxBlx);
};

// B and BL
//
// This implementation makes assumptions about where the L bit is located, but
// the assumption holds for all non-illegal direct branches.
class Branch : public OldClassDecoder {
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

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_ACTUAL_CLASSES_H_
