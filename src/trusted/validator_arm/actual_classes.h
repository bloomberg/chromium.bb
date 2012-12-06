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

// Defines a class decoder that we don't care about, and doesn't
// change any general purpose registers.
class DontCareInst : public ClassDecoder {
 public:
  DontCareInst() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(DontCareInst);
};

// Defines a DontCareInst where register Rd is not Pc.
//
// Note: Some instructions may use other names for the registers,
// but they have the same placement within the instruction, and
// hence do not need a separate class decoder.
class DontCareInstRdNotPc : public DontCareInst {
 public:
  // We use the following Rd to capture the register being set.
  static const RegBits12To15Interface d;

  DontCareInstRdNotPc() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(DontCareInstRdNotPc);
};

// Defines a DontCareInst where regsiters Rn, Rs, and Rm are not PC.
//
// Note: Some instructions may use other names for the registers,
// but they have the same placement within the instruction, and
// hence do not need a separate class decoder.
class DontCareInstRnRsRmNotPc : public DontCareInst {
 public:
  // We use the following Rm, Rs, and Rn to capture the
  // registers that need checking.
  static const RegBits0To3Interface m;
  static const RegBits8To11Interface s;
  static const RegBits16To19Interface n;

  DontCareInstRnRsRmNotPc() : DontCareInst() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(DontCareInstRnRsRmNotPc);
};

// Defines a class decoder that only worries about whether condition
// flags (S - bit(20)) is set.
class MaybeSetsConds : public ClassDecoder {
 public:
  static const UpdatesConditionsBit20Interface conditions;

  MaybeSetsConds() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MaybeSetsConds);
};

// Defines a default (assignment) class decoder, which assumes that
// the instruction is unsafe if it assigns register PC.
class NoPcAssignClassDecoder : public MaybeSetsConds {
 public:
  NoPcAssignClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(NoPcAssignClassDecoder);
};

// Defines a default (assignment) class decoder where we don't
// track if condition flags are updated.
class NoPcAssignCondsDontCare : public DontCareInst {
 public:
  NoPcAssignCondsDontCare() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(NoPcAssignCondsDontCare);
};

// Computes a value and stores in in Rd(15:12). Doesn't
// allow Rd=PC.
class Defs12To15 : public NoPcAssignClassDecoder {
 public:
  Defs12To15() {}

  // We use the following Rd to capture the register being set.
  static const RegBits12To15Interface d;

  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs12To15);
};

// Computes a value and stores in in Rd (bits 12-15). Doesn't
// allow Rd=PC, and doesn't care about tracking condition flags.
class Defs12To15CondsDontCare : public NoPcAssignCondsDontCare {
 public:
  // We use the following Rd to capture the register being set.
  static const RegBits12To15Interface d;

  Defs12To15CondsDontCare() {}
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs12To15CondsDontCare);
};

// Defs12To15CondsDontCare where registers Rd(15:12) and Rn(3:0) are not PC.
//
// Note: Some instructions may use other names for the registers,
// but they have the same placement within the instruction, and
// hence do not need a separate class decoder.
class Defs12To15CondsDontCareRdRnNotPc : public Defs12To15CondsDontCare {
 public:
  // We use the following Rd and Rn to capture the
  // registers that need checking.
  static const RegBits0To3Interface n;
  static const RegBits12To15Interface d;

  Defs12To15CondsDontCareRdRnNotPc() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs12To15CondsDontCareRdRnNotPc);
};

// Extra bitfield insert checks:
//   if msb < lsb then UNPREDICTABLE
class Defs12To15CondsDontCareMsbGeLsb : public Defs12To15CondsDontCare {
 public:
  static const Imm5Bits7To11Interface lsb;
  static const Imm5Bits16To20Interface msb;

  Defs12To15CondsDontCareMsbGeLsb() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs12To15CondsDontCareMsbGeLsb);
};

// Extra bitfield extract checks:
//   msbit = lsbit + widthminus1
//   if msbit > 31 then UNPREDICTABLE
class Defs12To15CondsDontCareRdRnNotPcBitfieldExtract
    : public Defs12To15CondsDontCareRdRnNotPc {
 public:
  static const Imm5Bits7To11Interface lsb;
  static const Imm5Bits16To20Interface widthm1;

  Defs12To15CondsDontCareRdRnNotPcBitfieldExtract() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Defs12To15CondsDontCareRdRnNotPcBitfieldExtract);
};

// Defs12To15 where registers Rd(15:12) and Rn(3:0) are not PC.
//
// Note: Some instructions may use other names for the registers,
// but they have the same placement within the instruction, and
// hence do not need a separate class decoder.
class Defs12To15RdRnNotPc : public Defs12To15 {
 public:
  // We use the following Rd and Rn to capture the
  // registers that need checking.
  static const RegBits0To3Interface n;
  static const RegBits12To15Interface d;

  Defs12To15RdRnNotPc() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs12To15RdRnNotPc);
};

// Defs12To15 where registers Rd(15:12), Rm(11:8), and Rn(3:0) are not
// Pc.
//
// Note: Some instructions may use other names for the registers,
// but they have the same placement within the instruction, and
// hence do not need a separate class decoder.
class Defs12To15RdRmRnNotPc : public Defs12To15 {
 public:
  // We use the following Rd, Rm, and Rn to capture the
  // registers that need checking.
  static const RegBits0To3Interface n;
  static const RegBits8To11Interface m;
  static const RegBits12To15Interface d;

  Defs12To15RdRmRnNotPc() : Defs12To15() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs12To15RdRmRnNotPc);
};

// Defs12To15 where registers Rn(19:16), Rd(15:12), Rs(11:8),
// and Rm(3:0) are not Pc.
//
// Disallows any of Rn, Rd, Rs, or Rm to be PC.
//
// Note: Some instructions may use other names for the registers,
// but they have the same placement within the instruction, and
// hence do not need a separate class decoder.
class Defs12To15RdRnRsRmNotPc : public Defs12To15 {
 public:
  // We use the following Rm, Rs, and Rn to capture the
  // registers that need checking.
  static const RegBits0To3Interface m;
  static const RegBits8To11Interface s;
  static const RegBits16To19Interface n;

  Defs12To15RdRnRsRmNotPc() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs12To15RdRnRsRmNotPc);
};

// Defs12To15CondsDontCare where registers Rn(19:16), Rs(11:8), and
// Rm(3:0) are not Pc.
//
// Note: Some instructions may use other names for the registers,
// but they have the same placement within the instruction, and
// hence do not need a separate class decoder.
class Defs12To15CondsDontCareRdRnRsRmNotPc
    : public Defs12To15CondsDontCare {
 public:
  // We use the following Rm, Rs, and Rn to capture the
  // registers that need checking.
  static const RegBits0To3Interface m;
  static const RegBits8To11Interface s;
  static const RegBits16To19Interface n;

  Defs12To15CondsDontCareRdRnRsRmNotPc()
      : Defs12To15CondsDontCare() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs12To15CondsDontCareRdRnRsRmNotPc);
};

// Computes a value and stores it in Rd (bits 16-19). Doesn't allow
// Rd=Pc, and doesn't care about tracking condition flags.
class Defs16To19CondsDontCare : public NoPcAssignClassDecoder {
 public:
  // We use the following Rd to capture the register being set.
  static const RegBits16To19Interface d;

  Defs16To19CondsDontCare() {}
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs16To19CondsDontCare);
};

// Defs16To19CondsDontCare where registers Rd, Rm, and Rn are not Pc.
class Defs16To19CondsDontCareRdRmRnNotPc : public Defs16To19CondsDontCare {
 public:
  // We use the following interfaces to capture the registers used.
  static const RegBits0To3Interface n;
  static const RegBits8To11Interface m;

  Defs16To19CondsDontCareRdRmRnNotPc() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs16To19CondsDontCareRdRmRnNotPc);
};

// Defs16To19CondsDontCare where registers Rd, Ra, Rm, and Rn are not Pc.
class Defs16To19CondsDontCareRdRaRmRnNotPc : public Defs16To19CondsDontCare {
 public:
  // We use the following interfaces to capture the registers used.
  static const RegBits0To3Interface n;
  static const RegBits8To11Interface m;
  static const RegBits12To15Interface a;

  Defs16To19CondsDontCareRdRaRmRnNotPc() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs16To19CondsDontCareRdRaRmRnNotPc);
};

// Computes a value and stores it in Rd_hi(16-19) an Rd_lo(12-15),
// doesn't allow PC for either, and doesn't care about tracking
// condition flags. Also doesn't allow Rd_hi == Rd_lo.
class Defs12To19CondsDontCare : public NoPcAssignClassDecoder {
 public:
  // We use the following interfaces to capture the registers being set.
  static const RegBits12To15Interface d_lo;
  static const RegBits16To19Interface d_hi;

  Defs12To19CondsDontCare() {}
  virtual RegisterList defs(Instruction i) const;
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs12To19CondsDontCare);
};

// Defs12To19CondsDontCare where registers Rd_hi, Rd_lo, Rm, and Rn
// are not PC.
class Defs12To19CondsDontCareRdRmRnNotPc : public Defs12To19CondsDontCare {
 public:
  // We use the following interfaces to capture the registers being tested.
  static const RegBits0To3Interface n;
  static const RegBits8To11Interface m;

  Defs12To19CondsDontCareRdRmRnNotPc() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs12To19CondsDontCareRdRmRnNotPc);
};

// Defs12To15 where registers Rn(16, 19), Rd(12, 15), and Rm(3, 0)
// are not Pc. We also don't care about tracking condition flags.
//
// Disallows any of Rn, Rd, or Rm to be PC.
class Defs12To15CondsDontCareRnRdRmNotPc : public Defs12To15CondsDontCare {
 public:
  // We use the following Rm, Rd, and Rn to capture the registers
  // that need checking.
  static const RegBits0To3Interface m;
  static const RegBits12To15Interface d;
  static const RegBits16To19Interface n;

  Defs12To15CondsDontCareRnRdRmNotPc() : Defs12To15CondsDontCare() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Defs12To15CondsDontCareRnRdRmNotPc);
};

// Defines test masking instruction to be used with a
// load/store (eq condition) to access memory.
class TestIfAddressMasked : public NoPcAssignClassDecoder {
 public:
  static const Imm12Bits0To11Interface imm12;
  static const RegBits16To19Interface n;

  TestIfAddressMasked() {}
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

  MaskAddress() {}
  virtual bool clears_bits(Instruction i, uint32_t mask) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MaskAddress);
};

// A generic VFP instruction that (by default) only effects vector
// register banks. Hence, they do not change general purpose registers.
// These instructions are:
// - Permitted only for whitelisted coprocessors (101x) that define VFP
//   operations.
// - Not permitted to update r15.
//
// Coprocessor ops with visible side-effects on the APSR conditions flags, or
// general purpose register should extend and override this.
class VfpOp : public CoprocessorOp {
 public:
  // Accessor to non-vector register fields.
  static const Imm4Bits8To11Interface coproc;

  VfpOp() {}
  // Default assumes the coprocessor instruction acts like a DontCareInst.
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VfpOp);
};

// Defines a register based memory class decoder.
class BasedAddressUsingRn : public ClassDecoder {
 public:
  // The base address register.
  static const RegBits16To19Interface n;

  BasedAddressUsingRn() {}
  virtual Register base_address_register(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BasedAddressUsingRn);
};

// Defines a based memory load on register Rn, class decoder.
class LoadBasedMemory : public BasedAddressUsingRn {
 public:
  // The base address register.
  static const RegBits16To19Interface n;
  // The destination register (May be named different than t,
  // but appears in the same bit locations).
  static const RegBits12To15Interface t;

  LoadBasedMemory() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadBasedMemory);
};

// Defines a LoadBasedMemory class decoder that is double wide (i.e.
// uses Rt2 as defined by Rt).
class LoadBasedMemoryDouble : public LoadBasedMemory {
 public:
  static const RegBits12To15Plus1Interface t2;

  LoadBasedMemoryDouble() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadBasedMemoryDouble);
};

// Defines a based memory load with writeback.
class LoadBasedMemoryWithWriteBack : public LoadBasedMemory {
 public:
  // Interfaces for components defining writeback.
  static const WritesBit21Interface writes;
  static const PrePostIndexingBit24Interface indexing;
  bool HasWriteBack(const Instruction i) const {
    return indexing.IsPostIndexing(i) || writes.IsDefined(i);
  }

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 protected:
  LoadBasedMemoryWithWriteBack() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadBasedMemoryWithWriteBack);
};

// Defines a based memory load on register Rn, assuming it can
// be modified by a small literal offset, and optionally, the
// value defined in register Rm.
class LoadBasedOffsetMemory : public LoadBasedMemoryWithWriteBack {
 public:
  // The offset register.
  static const RegBits0To3Interface m;

  LoadBasedOffsetMemory() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadBasedOffsetMemory);
};

// Defines a LoadBasedOffsetMemory, which also writes to Rt2.
class LoadBasedOffsetMemoryDouble : public LoadBasedOffsetMemory {
 public:
  static const RegBits12To15Plus1Interface t2;

  LoadBasedOffsetMemoryDouble() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadBasedOffsetMemoryDouble);
};

// Defines a based memory load on register Rn, assuming it can
// be modified by an immediate offset.
class LoadBasedImmedMemory : public LoadBasedMemoryWithWriteBack {
 public:
  LoadBasedImmedMemory() {}
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual bool is_literal_load(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadBasedImmedMemory);
};

// Defines a LoadBasedImmedMemory, which also writes to Rt2.
class LoadBasedImmedMemoryDouble : public LoadBasedImmedMemory {
 public:
  static const RegBits12To15Plus1Interface t2;

  LoadBasedImmedMemoryDouble() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LoadBasedImmedMemoryDouble);
};

// Defines a based memory store from register Rt(3:0).
class StoreBasedMemoryRtBits0To3 : public BasedAddressUsingRn {
 public:
  static const RegBits0To3Interface t;
  static const RegBits12To15Interface d;

  StoreBasedMemoryRtBits0To3() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreBasedMemoryRtBits0To3);
};

// Defines a StoreBasedMemoryRtBits0To3 class decoder that is double
// wide (i.e uses Rt2 as defined by Rt).
class StoreBasedMemoryDoubleRtBits0To3 : public StoreBasedMemoryRtBits0To3 {
 public:
  static const RegBits0To3Plus1Interface t2;

  StoreBasedMemoryDoubleRtBits0To3() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreBasedMemoryDoubleRtBits0To3);
};

// Defines a based memory store from register Rt(15, 12), with writeback.
class StoreBasedMemoryWithWriteBack : public BasedAddressUsingRn {
 public:
  static const RegBits12To15Interface t;
  static const WritesBit21Interface writes;
  static const PrePostIndexingBit24Interface indexing;
  bool HasWriteBack(const Instruction i) const {
    return indexing.IsPostIndexing(i) || writes.IsDefined(i);
  }

  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 protected:
  StoreBasedMemoryWithWriteBack() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreBasedMemoryWithWriteBack);
};

// Defines a based memory store using register Rn, assuming it can
// be modified by a small literal offset, and optionally, the value
// defined in register Rm.
class StoreBasedOffsetMemory : public StoreBasedMemoryWithWriteBack {
 public:
  static const RegBits0To3Interface m;

  StoreBasedOffsetMemory() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreBasedOffsetMemory);
};

// Defines a StoreBasedOffsetMemory, which also uses Rt2.
class StoreBasedOffsetMemoryDouble : public StoreBasedOffsetMemory {
 public:
  static const RegBits12To15Plus1Interface t2;

  StoreBasedOffsetMemoryDouble() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreBasedOffsetMemoryDouble);
};

// Defines a based memory load on register Rn, assuming it can be
// modified by an immediate offset.
class StoreBasedImmedMemory : public StoreBasedMemoryWithWriteBack {
 public:
  StoreBasedImmedMemory() {}
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreBasedImmedMemory);
};

// Defines a StoreBasedImmedMemory, which also uses Rt2
class StoreBasedImmedMemoryDouble : public StoreBasedImmedMemory {
 public:
  static const RegBits12To15Plus1Interface t2;

  StoreBasedImmedMemoryDouble() {}
  virtual SafetyLevel safety(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreBasedImmedMemoryDouble);
};

// **************************************************************
//      O L D    C L A S S    D E C O D E R S
// **************************************************************

class OldClassDecoder : public ClassDecoder {
 public:
  OldClassDecoder() : ClassDecoder() {}

  // Many instructions define control bits in bits 20-24. The useful bits
  // are defined here.

  // True if S (updates condition flags in APSR register) flag is defined.
  bool UpdatesConditions(const Instruction& i) const {
    return i.Bit(20);
  }

  // True if W (does write) flag is defined.
  bool WritesFlag(const Instruction& i) const {
    return i.Bit(21);
  }
  // True if P (pre-indexing) flag is defined.
  bool PreindexingFlag(const Instruction& i) const {
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
  EffectiveNoOp() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(EffectiveNoOp);
};

// UDF
// Permanently undefined in the ARM ISA.
class PermanentlyUndefined : public OldClassDecoder {
 public:
  PermanentlyUndefined() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PermanentlyUndefined);
};

// BKPT
// We model this mostly so we can use it to recognize literal pools -- untrusted
// code isn't expected to use it, but it's not unsafe, and there are cases where
// we may generate it.
class Breakpoint : public OldClassDecoder {
 public:
  Breakpoint() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual bool is_literal_pool_head(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Breakpoint);
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
  PackSatRev() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

  // Defines the destination register.
  Register Rd(const Instruction& i) const {
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
  Multiply() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

  // Defines the destination register.
  Register Rd(const Instruction& i) const {
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
  LongMultiply() {}
  virtual RegisterList defs(Instruction i) const;

  // Supplies the lower 32 bits for the destination result.
  Register RdLo(const Instruction& i) const {
    return i.Reg(15, 12);
  }
  // Supplies the other 32 bits of the destination result.
  Register RdHi(const Instruction& i) const {
    return i.Reg(19, 16);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LongMultiply);
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
  MoveToStatusRegister() {}
  virtual SafetyLevel safety(Instruction i) const;
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
  StoreImmediate() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual Register base_address_register(Instruction i) const;

  // Defines the base register.
  Register Rn(const Instruction& i) const {
    return i.Reg(19, 16);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreImmediate);
};

// And, finally, the oddest class of loads: LDM.  In addition to the base
// register, this may write every other register, subject to a bitmask.
//
// Includes:
// LDMDA / LDMFA, LDM / LDMIA / LDMFD, LDMDB / LDMEA, LDMIB / LDMED
class LoadMultiple : public OldClassDecoder {
 public:
  LoadMultiple() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual Register base_address_register(Instruction i) const;

  // Defines the base register.
  Register Rn(const Instruction& i) const {
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
  VectorLoad() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual Register base_address_register(Instruction i) const;

  // Defines the base address for the access.
  Register Rn(const Instruction& i) const {
    return i.Reg(19, 16);
  }
  // Contains an address offset applied after the access.
  Register Rm(const Instruction& i) const {
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
  VectorStore() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual Register base_address_register(Instruction i) const;

  static const WritesBit21Interface writes;
  // Defines the base address for the access.
  Register Rn(const Instruction& i) const {
    return i.Reg(19, 16);
  }
  // Contains an address offset applied after the access.
  Register Rm(const Instruction& i) const {
    return i.Reg(3, 0);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(VectorStore);
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
  static const WritesBit21Interface writes;

  LoadCoprocessor() {}
  virtual RegisterList defs(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;

  // Contains the base address for the access.
  Register Rn(const Instruction& i) const {
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
  StoreCoprocessor() {}
  virtual RegisterList defs(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual Register base_address_register(Instruction i) const;

  static const WritesBit21Interface writes;
  // Contains the base address for the access.
  Register Rn(const Instruction& i) const {
    return i.Reg(19, 16);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StoreCoprocessor);
};

// MRC/MRC2, which load a single register from a coprocessor register.
class MoveFromCoprocessor : public CoprocessorOp {
 public:
  MoveFromCoprocessor() {}
  virtual RegisterList defs(Instruction i) const;

  // Defines the destination core register.
  Register Rt(const Instruction& i) const {
    Register rt(i.Reg(15, 12));
    if (rt.Equals(Register::Pc())) {
      // Per ARM ISA spec: r15 here is a synonym for the flags register!
      return Register::Conditions();
    }
    return rt;
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MoveFromCoprocessor);
};

// BX and BLX - everyone's favorite register-indirect branch.
// This implementation makes assumptions about where the L bit is located, and
// should not be used for other indirect branches (such as mov pc, rN).
// Hence the cryptic name.
class BxBlx : public OldClassDecoder {
 public:
  static const RegBits0To3Interface m;
  static const UpdatesLinkRegisterBit5Interface link_register;

  BxBlx() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual Register branch_target_register(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BxBlx);
};

// B and BL
//
// This implementation makes assumptions about where the L bit is located, but
// the assumption holds for all non-illegal direct branches.
class Branch : public OldClassDecoder {
 public:
  Branch() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual bool is_relative_branch(Instruction i) const;
  virtual int32_t branch_target_offset(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Branch);
};

//-------------------------------------------------------------------
// The following are class decoders that are used in the baseline, as
// well as the actual class decoders. Hence, they do not need to be
// duplicated in files baseline_classes.{h,cc}.
//-------------------------------------------------------------------

// Models a store into Rd(15, 12).
// MRS<c> <Rd>, <spec_reg>
// +--------+----------+--+------------+--------+-----------------------+
// |31302928|2726252423|22|212019181716|15141312|111 9 8 7 6 5 4 3 2 1 0|
// +--------+----------+--+------------+--------+-----------------------+
// |  cond  |          | R|            |   Rd   |                       |
// +--------+----------+--+------------+--------+-----------------------+
// If Rd is R15, the instruction is unpredictable.
// If R=1, then UNPREDICTABLE if mode is User or System.
class Unary1RegisterSet : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits12To15Interface d;
  static const FlagBit22Interface read_spsr;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Unary1RegisterSet() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterSet);
};

// Models a move of Rn to the APSR register.
// MSR<c> <spec_reg>, <Rn>
// +--------+---------------+----+----------------------------+--------+
// |31302928|272625423222120|1918|1716151413121110 9 8 7 6 5 4| 3 2 1 0|
// +--------+---------------+----+----------------------------+--------+
// |  cond  |               |mask|                            |   Rn   |
// +--------+---------------+----+----------------------------+--------+
// If mask<1>=1, then update conditions flags(NZCVQ) in APSR
// If mask<0>=1, then update GE(bits(19:16)) in APSR.
// If mask=0, then UNPREDICTABLE.
// If Rn=15, then UNPREDICTABLE.
//
// write_nzcvq := mask(1)=1; write_g := mask(0)=1;
// defs := {NZCV if write_nzcvq else None}
// uses := {Rn};
class Unary1RegisterUse : public ClassDecoder {
 public:
  // Interfaces for components in the instruction.
  static const RegBits0To3Interface n;
  static const Imm2Bits18To19Interface mask;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Unary1RegisterUse() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterUse);
};

// Models a 1-register unary operation with two immediate 5 values
// defining a bit range.
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
// If Rd is R15 or msbit < lsbit, the instruction is unpredictable.
// NaCl disallows writing to PC to cause a jump.
// Note: Currently, only implements bfc.
class Unary1RegisterBitRangeMsbGeLsb : public ClassDecoder {
 public:
  // Interface for components of the instruction.
  static const Imm5Bits7To11Interface lsb;
  static const RegBits12To15Interface d;
  static const Imm5Bits16To20Interface msb;
  static const ConditionBits28To31Interface cond;

  // Methods for class.
  Unary1RegisterBitRangeMsbGeLsb() : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList defs(Instruction i) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Unary1RegisterBitRangeMsbGeLsb);
};

}  // namespace nacl_arm_dec

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_ACTUAL_CLASSES_H_
