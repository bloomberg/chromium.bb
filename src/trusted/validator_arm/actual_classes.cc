/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_arm/actual_classes.h"

#include <assert.h>
#include <string.h>

// Implementations of instruction classes, for those not completely defined in
// in the header.

namespace nacl_arm_dec {

// **************************************************************
//      N E W    C L A S S    D E C O D E R S
// **************************************************************

SafetyLevel DontCareInst::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList DontCareInst::defs(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList::CondsDontCare();
}

SafetyLevel DontCareInstRdNotPc::safety(Instruction i) const {
  if (d.reg(i).Equals(Register::Pc()))
    return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

SafetyLevel DontCareInstRnRsRmNotPc::safety(Instruction i) const {
  if (RegisterList(m.reg(i)).Add(s.reg(i)).Add(n.reg(i)).
      Contains(Register::Pc()))
    return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

SafetyLevel MaybeSetsConds::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList MaybeSetsConds::defs(const Instruction i) const {
  return RegisterList(conditions.conds_if_updated(i));
}

SafetyLevel NoPcAssignClassDecoder::safety(const Instruction i) const {
  if (defs(i).Contains(Register::Pc())) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

SafetyLevel NoPcAssignCondsDontCare::safety(const Instruction i) const {
  if (defs(i).Contains(Register::Pc())) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList Defs12To15::defs(const Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

RegisterList Defs12To15CondsDontCare::defs(const Instruction i) const {
  return RegisterList(RegisterList::CondsDontCare()).Add(d.reg(i));
}

SafetyLevel Defs12To15CondsDontCareRdRnNotPc::safety(Instruction i) const {
  if (RegisterList(d.reg(i)).Add(n.reg(i)).Contains(Register::Pc()))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

SafetyLevel Defs12To15CondsDontCareMsbGeLsb::
safety(Instruction i) const {
  return (msb.value(i) < lsb.value(i))
      ? UNPREDICTABLE
      : Defs12To15CondsDontCare::safety(i);
}

SafetyLevel Defs12To15CondsDontCareRdRnNotPcBitfieldExtract::
safety(Instruction i) const {
  return (lsb.value(i) + widthm1.value(i) > 31)
      ? UNPREDICTABLE
      : Defs12To15CondsDontCareRdRnNotPc::safety(i);
}

SafetyLevel Defs12To15RdRnNotPc::safety(Instruction i) const {
  if (RegisterList(d.reg(i)).Add(n.reg(i)).Contains(Register::Pc()))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

SafetyLevel Defs12To15RdRmRnNotPc::safety(Instruction i) const {
  if (RegisterList(d.reg(i)).Add(m.reg(i)).Add(n.reg(i)).
      Contains(Register::Pc()))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

SafetyLevel Defs12To15RdRnRsRmNotPc::safety(
    const Instruction i) const {
  if (RegisterList(d.reg(i)).Add(n.reg(i)).Add(s.reg(i)).
      Add(m.reg(i)).Contains(Register::Pc()))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

SafetyLevel Defs12To15CondsDontCareRdRnRsRmNotPc::safety(
    const Instruction i) const {
  if (RegisterList(d.reg(i)).Add(n.reg(i)).Add(s.reg(i)).
      Add(m.reg(i)).Contains(Register::Pc()))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Defs16To19CondsDontCare::defs(const Instruction i) const {
  return RegisterList(RegisterList::CondsDontCare()).Add(d.reg(i));
}

SafetyLevel Defs16To19CondsDontCareRdRmRnNotPc::
safety(const Instruction i) const {
  if (RegisterList(d.reg(i)).Add(m.reg(i)).Add(n.reg(i)).
      Contains(Register::Pc()))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

SafetyLevel Defs16To19CondsDontCareRdRaRmRnNotPc::
safety(const Instruction i) const {
  if (RegisterList(d.reg(i)).Add(a.reg(i)).Add(m.reg(i)).Add(n.reg(i)).
      Contains(Register::Pc()))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Defs12To19CondsDontCare::defs(const Instruction i) const {
  return RegisterList(RegisterList::CondsDontCare()).
      Add(d_hi.reg(i)).Add(d_lo.reg(i));
}

SafetyLevel Defs12To19CondsDontCare::safety(const Instruction i) const {
  if (d_hi.reg(i).Equals(d_lo.reg(i)))
    return UNPREDICTABLE;
  if (defs(i).Contains(Register::Pc()))
    return FORBIDDEN;
  return MAY_BE_SAFE;
}

SafetyLevel Defs12To19CondsDontCareRdRmRnNotPc::
safety(const Instruction i) const {
  if (d_hi.reg(i).Equals(d_lo.reg(i)))
    return UNPREDICTABLE;
  if (defs(i).Add(n.reg(i)).Add(m.reg(i)).Contains(Register::Pc()))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

SafetyLevel Defs12To15CondsDontCareRnRdRmNotPc::safety(
    const Instruction i) const {
  if (RegisterList(n.reg(i)).Add(d.reg(i)).Add(m.reg(i)).
      Contains(Register::Pc()))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList TestIfAddressMasked::defs(Instruction i) const {
  return RegisterList(conditions.conds_if_updated(i));
}

bool TestIfAddressMasked::sets_Z_if_bits_clear(Instruction i,
                                               Register r,
                                               uint32_t mask) const {
  return n.reg(i).Equals(r)
      && (imm12.get_modified_immediate(i) & mask) == mask
      && defs(i).Contains(Register::Conditions());
}

bool MaskAddress::clears_bits(const Instruction i, uint32_t mask) const {
  return (imm12.get_modified_immediate(i) & mask) == mask;
}

RegisterList VfpOp::defs(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList::CondsDontCare();
}

Register BasedAddressUsingRn::base_address_register(Instruction i) const {
  return n.reg(i);
}

SafetyLevel LoadBasedMemory::safety(const Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(n.reg(i)).Add(t.reg(i)).Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rt in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList LoadBasedMemory::defs(Instruction i) const {
  return RegisterList(t.reg(i));
}

SafetyLevel LoadBasedMemoryDouble::safety(const Instruction i) const {
  // Arm restrictions for this instruction, based on double width.
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(n.reg(i)).Add(t2.reg(i)).Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  // Note: We would restrict out PC as well for Rt in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList LoadBasedMemoryDouble::defs(Instruction i) const {
  return RegisterList(t.reg(i)).Add(t2.reg(i));
}

SafetyLevel StoreBasedMemoryRtBits0To3::safety(const Instruction i) const {
  // Arm restrictions for this instruction.
  if (RegisterList(d.reg(i)).Add(t.reg(i)).Add(n.reg(i)).
      Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  if (d.reg(i).Equals(n.reg(i)) || d.reg(i).Equals(t.reg(i))) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList StoreBasedMemoryRtBits0To3::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

SafetyLevel StoreBasedMemoryDoubleRtBits0To3::
safety(const Instruction i) const {
  // Arm restrictions for this instruction.
  if (RegisterList(d.reg(i)).Add(t2.reg(i)).Add(n.reg(i)).
      Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (d.reg(i).Equals(n.reg(i)) ||
      d.reg(i).Equals(t.reg(i)) ||
      d.reg(i).Equals(t2.reg(i))) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

SafetyLevel LoadBasedMemoryWithWriteBack::safety(const Instruction i) const {
  // Arm restrictions for this instruction.
  if (t.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(Register::Pc()) || n.reg(i).Equals(t.reg(i)))) {
    return UNPREDICTABLE;
  }

  // Above implies literal loads can't writeback, the following checks the
  // ARM restriction that literal loads can't have P == W.
  // This should always decode to another instruction, but checking it is good.
  if (n.reg(i).Equals(Register::Pc()) &&
      (indexing.IsDefined(i) == writes.IsDefined(i))) {
    return UNPREDICTABLE;
  }

  // Don't let addressing writeback alter PC (NaCl constraint).
  if (defs(i).Contains(Register::Pc())) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList LoadBasedMemoryWithWriteBack::defs(const Instruction i) const {
  return RegisterList(t.reg(i)).Add(HasWriteBack(i) ?
                                    n.reg(i) : Register::None());
}

SafetyLevel LoadBasedOffsetMemory::safety(const Instruction i) const {
  // If pre-indexing is set, the address of the load is computed as the sum
  // of the two register parameters.  We have checked that the first register
  // is within the sandbox, but this would allow adding an arbitrary value
  // to it, so it is not safe. (NaCl constraint).
  if (indexing.IsPreIndexing(i))
    return FORBIDDEN;

  // Arm restrictions for this instruction.
  if (m.reg(i).Equals(Register::Pc()))
    return UNPREDICTABLE;

  return LoadBasedMemoryWithWriteBack::safety(i);
}

SafetyLevel LoadBasedOffsetMemoryDouble::safety(const Instruction i) const {
  // Arm double width restrictions for this instruction.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (t2.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) && n.reg(i).Equals(t2.reg(i))) {
    return UNPREDICTABLE;
  }

  if (m.reg(i).Equals(t.reg(i)) || m.reg(i).Equals(t2.reg(i))) {
    return UNPREDICTABLE;
  }

  // Now apply non-double width restrictions for this instruction.
  return LoadBasedOffsetMemory::safety(i);
}

RegisterList LoadBasedOffsetMemoryDouble::defs(Instruction i) const {
  return LoadBasedOffsetMemory::defs(i).Add(t2.reg(i));
}

bool LoadBasedImmedMemory::base_address_register_writeback_small_immediate(
    Instruction i) const {
  return HasWriteBack(i);
}

bool LoadBasedImmedMemory::is_literal_load(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return base_address_register(i).Equals(Register::Pc());
}

SafetyLevel LoadBasedImmedMemoryDouble::safety(const Instruction i) const {
  // Arm double width restrictions for this instruction.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (t2.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) && n.reg(i).Equals(t2.reg(i))) {
    return UNPREDICTABLE;
  }

  // Now apply non-double width restrictions for this instruction.
  return LoadBasedImmedMemory::safety(i);
}

RegisterList LoadBasedImmedMemoryDouble::defs(const Instruction i) const {
  return LoadBasedImmedMemory::defs(i).Add(t2.reg(i));
}

SafetyLevel StoreBasedMemoryWithWriteBack::safety(const Instruction i) const {
  // Arm restrictions for this instruction.
  if (t.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(Register::Pc()))) {
    // NOTE: The manual states that that it is also unpredictable
    // when HasWriteBack(i) and Rn=Rt. However, the compilers
    // may not check for this. For the moment, we are changing
    // the code to ignore this case for stores.
    // TODO(karl): Should we not allow this?
    // TODO(jfb) Fix this.
    return UNPREDICTABLE;
  }

  // Don't let addressing writeback alter PC (NaCl constraint).
  if (defs(i).Contains(Register::Pc())) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList StoreBasedMemoryWithWriteBack::defs(const Instruction i) const {
  return RegisterList(HasWriteBack(i) ? n.reg(i) : Register::None());
}

SafetyLevel StoreBasedOffsetMemory::safety(const Instruction i) const {
  if (indexing.IsPreIndexing(i)) {
    // If pre-indexing is set, the address of the load is computed as the sum
    // of the two register parameters.  We have checked that the first register
    // is within the sandbox, but this would allow adding an arbitrary value
    // to it, so it is not safe. (NaCl constraint).
    return FORBIDDEN;
  }

  // Arm restrictions for this instruction.
  if (m.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(Register::Pc()))) {
    // NOTE: The manual states that that it is also unpredictable
    // when HasWriteBack(i) and Rn=Rt. However, the compilers
    // may not check for this. For the moment, we are changing
    // the code to ignore this case for stores.
    // TODO(karl): Should we not allow this?
    // TODO(jfb) Fix this.
    return UNPREDICTABLE;
  }

  // TODO(jfb) if ArchVersion() < 6 && wback && m == n then UNPREDICTABLE;

  return StoreBasedMemoryWithWriteBack::safety(i);
}

SafetyLevel StoreBasedOffsetMemoryDouble::safety(const Instruction i) const {
  // Arm double width restrictions for this instruction.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (RegisterList(t2.reg(i)).Add(m.reg(i)).Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  // NOTE: The manual states that that it is also unpredictable
  // when HasWriteBack(i) and Rn=Rt or Rn=Rt2. However, the compilers
  // may not check for this. For the moment, we are changing
  // the code to ignore this case for stores.
  // TODO(karl): Should we not allow this?
  // if (HasWriteBack(i) &&
  //     (n.reg(i).Equals(Register::Pc()))) {
  //   return UNPREDICTABLE;
  // }

  // Now apply non-double width restrictions for this instruction.
  return StoreBasedOffsetMemory::safety(i);
}

bool StoreBasedImmedMemory::base_address_register_writeback_small_immediate(
    Instruction i) const {
  return HasWriteBack(i);
}

SafetyLevel StoreBasedImmedMemoryDouble::safety(const Instruction i) const {
  // Arm double width restrictions for this instruction.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (t2.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  // NOTE: The manual states that that it is also unpredictable
  // when HasWriteBack(i) and Rn=Rt or Rn=Rt2. However, the compilers
  // may not check for this. For the moment, we are changing
  // the code to ignore this case for stores.
  // TODO(karl): Should we not allow this?
  // if (HasWriteBack(i) && n.reg(i).Equals(t2.reg(i))) {
  //   return UNPREDICTABLE;
  // }

  // Now apply non-double width restrictions for this instruction.
  return StoreBasedImmedMemory::safety(i);
}

// **************************************************************
//      O L D    C L A S S    D E C O D E R S
// **************************************************************

// EffectiveNoOp
SafetyLevel EffectiveNoOp::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList EffectiveNoOp::defs(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

// PermanentlyUndefined
SafetyLevel PermanentlyUndefined::safety(Instruction i) const {
  // Restrict UDF's encoding to values we've chosen as safe.
  if ((i.Bits(31, 0) == kHaltFill) ||
      (i.Bits(31, 0) == kAbortNow))
    return MAY_BE_SAFE;
  return FORBIDDEN_OPERANDS;
}

RegisterList PermanentlyUndefined::defs(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

// Breakpoint
SafetyLevel Breakpoint::safety(Instruction i) const {
  if (i.GetCondition() != Instruction::AL)
    return UNPREDICTABLE;
  // Restrict BKPT's encoding to values we've chosen as safe.
  if ((i.Bits(31, 0) == kLiteralPoolHead) ||
      (i.Bits(31, 0) == kBreakpoint))
    return MAY_BE_SAFE;
  return FORBIDDEN_OPERANDS;
}

RegisterList Breakpoint::defs(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

bool Breakpoint::is_literal_pool_head(const Instruction i) const {
  return i.Bits(31, 0) == kLiteralPoolHead;
}

SafetyLevel PackSatRev::safety(const Instruction i) const {
  if (defs(i).Contains(Register::Pc())) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList PackSatRev::defs(const Instruction i) const {
  return RegisterList(Rd(i)).Add(Register::Conditions());
}


SafetyLevel Multiply::safety(const Instruction i) const {
  if (defs(i).Contains(Register::Pc())) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList Multiply::defs(const Instruction i) const {
  return RegisterList(Rd(i)).Add(Register::Conditions());
}


RegisterList LongMultiply::defs(const Instruction i) const {
  return RegisterList(RdHi(i)).Add(RdLo(i));
}


// MSR

SafetyLevel MoveToStatusRegister::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList MoveToStatusRegister::defs(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList(Register::Conditions());
}


// Stores

SafetyLevel StoreImmediate::safety(Instruction i) const {
  // Don't let addressing writeback alter PC.
  if (defs(i).Contains(Register::Pc())) return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

RegisterList StoreImmediate::defs(Instruction i) const {
  return RegisterList(base_small_writeback_register(i));
}

bool StoreImmediate::base_address_register_writeback_small_immediate(
    Instruction i) const {
  return !PreindexingFlag(i) || WritesFlag(i);
}

Register StoreImmediate::base_address_register(const Instruction i) const {
  return Rn(i);
}

// Loads

SafetyLevel LoadMultiple::safety(const Instruction i) const {
  // Bottom 16 bits is a register list.
  if (WritesFlag(i) && i.Bit(Rn(i).number())) {
    // In ARMv7, cannot update base register both by popping and by indexing.
    // (Pre-v7 this was still a weird thing to do.)
    return UNPREDICTABLE;
  }
  if (Rn(i).Equals(Register::Pc()) || (i.Bits(15, 0) == 0)) {
    return UNPREDICTABLE;
  }
  if (defs(i).Contains(Register::Pc())) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList LoadMultiple::defs(Instruction i) const {
  return RegisterList(i.Bits(15, 0)).Union(RegisterList(
      base_small_writeback_register(i)));
}

bool LoadMultiple::base_address_register_writeback_small_immediate(
    Instruction i) const {
  return WritesFlag(i);
}

Register LoadMultiple::base_address_register(const Instruction i) const {
  return Rn(i);
}

// Vector load/stores

SafetyLevel VectorLoad::safety(Instruction i) const {
  if (defs(i).Contains(Register::Pc())) return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

RegisterList VectorLoad::defs(Instruction i) const {
  // Rm == PC indicates no address writeback.  Otherwise Rn is affected.
  return RegisterList(!Rm(i).Equals(Register::Pc()) ? Rn(i) : Register::None());
}

bool VectorLoad::base_address_register_writeback_small_immediate(
    Instruction i) const {
  // Rm == SP indicates automatic update based on size of load, and
  // updated by small static displacement.
  return Rm(i).Equals(Register::Sp());
}

Register VectorLoad::base_address_register(const Instruction i) const {
  return Rn(i);
}


SafetyLevel VectorStore::safety(Instruction i) const {
  if (defs(i).Contains(Register::Pc())) return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

RegisterList VectorStore::defs(Instruction i) const {
  // Rm == PC indicates no address writeback.  Otherwise Rn is affected.
  return RegisterList(!Rm(i).Equals(Register::Pc())
                      ? base_address_register(i) : Register::None());
}

bool VectorStore::base_address_register_writeback_small_immediate(
    Instruction i) const {
  // Rm == SP indicates automatic update based on size of store.
  return Rm(i).Equals(Register::Sp());
}

Register VectorStore::base_address_register(Instruction i) const {
  return Rn(i);
}


// Coprocessors
/*
SafetyLevel CoprocessorOp::safety(Instruction i) const {
  if (defs(i).Contains(Register::Pc())) return FORBIDDEN_OPERANDS;
  switch (CoprocIndex(i)) {
    default: return FORBIDDEN;

    case 10:
    case 11:  // NEON/VFP
      return MAY_BE_SAFE;
  }
}
*/

RegisterList LoadCoprocessor::defs(Instruction i) const {
  return RegisterList(base_small_writeback_register(i));
}

bool LoadCoprocessor::base_address_register_writeback_small_immediate(
    Instruction i) const {
  return writes.IsDefined(i);
}

RegisterList StoreCoprocessor::defs(Instruction i) const {
  return RegisterList(base_small_writeback_register(i));
}

bool StoreCoprocessor::base_address_register_writeback_small_immediate(
    Instruction i) const {
  return writes.IsDefined(i);
}

Register StoreCoprocessor::base_address_register(Instruction i) const {
  return Rn(i);
}


RegisterList MoveFromCoprocessor::defs(Instruction i) const {
  return RegisterList(Rt(i));
}


// Control flow

SafetyLevel BxBlx::safety(Instruction i) const {
  // Extra NaCl constraint: can't branch to PC. This would branch to 8 bytes
  // after the current instruction. This instruction should be in an instruction
  // pair, the mask should therefore be to PC and fail checking, but there's
  // little harm in checking.
  if (m.reg(i).Equals(Register::Pc())) return FORBIDDEN_OPERANDS;

  // Redundant with the above, but this is actually UNPREDICTABLE. Expect DCE.
  if (link_register.IsUpdated(i) && m.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  return MAY_BE_SAFE;
}

RegisterList BxBlx::defs(const Instruction i) const {
  return RegisterList(Register::Pc()).
      Add(link_register.IsUpdated(i) ? Register::Lr() : Register::None());
}

Register BxBlx::branch_target_register(const Instruction i) const {
  return m.reg(i);
}

SafetyLevel Branch::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

bool Branch::is_relative_branch(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return true;
}

RegisterList Branch::defs(const Instruction i) const {
  return RegisterList(Register::Pc()).
      Add(PreindexingFlag(i) ? Register::Lr() : Register::None());
}

int32_t Branch::branch_target_offset(const Instruction i) const {
  // Sign extend and shift left 2:
  int32_t offset = (int32_t)(i.Bits(23, 0) << 8) >> 6;
  // The ARM manual states that "PC reads as the address ofthe current
  // instruction plus 8".
  return offset + 8;
}

// Unary1RegisterSet
SafetyLevel Unary1RegisterSet::safety(const Instruction i) const {
  if (read_spsr.IsDefined(i)) {
    return UNPREDICTABLE;
  }

  if (d.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Unary1RegisterSet::defs(const Instruction i) const {
  return RegisterList(d.reg(i));
}

// Unary1RegisterUse
SafetyLevel Unary1RegisterUse::safety(const Instruction i) const {
  if (mask.value(i) == 0) {
    return UNPREDICTABLE;
  }

  if (n.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  return MAY_BE_SAFE;
}

RegisterList Unary1RegisterUse::defs(const Instruction i) const {
  return RegisterList((mask.value(i) < 2) ?
                      Register::None() : Register::Conditions());
}

// Unary1RegisterBitRangeMsbGeLsb
SafetyLevel Unary1RegisterBitRangeMsbGeLsb::safety(Instruction i) const {
  if (d.reg(i).Equals(Register::Pc()) ||
      msb.value(i) < lsb.value(i))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Unary1RegisterBitRangeMsbGeLsb::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

bool Unary1RegisterBitRangeMsbGeLsb
::clears_bits(Instruction i, uint32_t mask) const {
  int msbit = msb.value(i);
  int lsbit = lsb.value(i);
  int width = msbit + 1 - lsbit;
  if (msbit < lsbit) {
    // This is UNPREDICTABLE, return the safest thing: no bits were cleared.
    return false;
  }
  else if (width == 32) {
    // Clears everything.
    return true;
  } else {
    uint32_t bit_mask = (((1 << width) - 1) << lsbit);
    return (bit_mask & mask) == mask;
  }
}

}  // namespace nacl_arm_dec
