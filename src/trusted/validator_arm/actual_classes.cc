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
  return kCondsDontCare;
}

SafetyLevel DontCareInstRnRsRmNotPc::safety(Instruction i) const {
  if (RegisterList(m.reg(i)).Add(s.reg(i)).Add(n.reg(i)).Contains(kRegisterPc))
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
  if (defs(i).Contains(kRegisterPc)) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

SafetyLevel NoPcAssignCondsDontCare::safety(const Instruction i) const {
  if (defs(i).Contains(kRegisterPc)) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList Defs12To15::defs(const Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

RegisterList Defs12To15CondsDontCare::defs(const Instruction i) const {
  return RegisterList(kCondsDontCare).Add(d.reg(i));
}

SafetyLevel Defs12To15CondsDontCareRdRnNotPc::safety(Instruction i) const {
  if (RegisterList(d.reg(i)).Add(n.reg(i)).Contains(kRegisterPc))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

SafetyLevel Defs12To15RdRnNotPc::safety(Instruction i) const {
  if (RegisterList(d.reg(i)).Add(n.reg(i)).Contains(kRegisterPc))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

SafetyLevel Defs12To15RdRmRnNotPc::safety(Instruction i) const {
  if (RegisterList(d.reg(i)).Add(m.reg(i)).Add(n.reg(i)).Contains(kRegisterPc))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

SafetyLevel Defs12To15RdRnRsRmNotPc::safety(
    const Instruction i) const {
  if (RegisterList(d.reg(i)).Add(n.reg(i)).Add(s.reg(i)).
      Add(m.reg(i)).Contains(kRegisterPc))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

SafetyLevel Defs12To15CondsDontCareRdRnRsRmNotPc::safety(
    const Instruction i) const {
  if (RegisterList(d.reg(i)).Add(n.reg(i)).Add(s.reg(i)).
      Add(m.reg(i)).Contains(kRegisterPc))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Defs16To19CondsDontCare::defs(const Instruction i) const {
  return RegisterList(kCondsDontCare).Add(d.reg(i));
}

SafetyLevel Defs16To19CondsDontCareRdRmRnNotPc::
safety(const Instruction i) const {
  if (RegisterList(d.reg(i)).Add(m.reg(i)).Add(n.reg(i)).Contains(kRegisterPc))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

SafetyLevel Defs16To19CondsDontCareRdRaRmRnNotPc::
safety(const Instruction i) const {
  if (RegisterList(d.reg(i)).Add(a.reg(i)).Add(m.reg(i)).Add(n.reg(i)).
      Contains(kRegisterPc))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Defs12To19CondsDontCare::defs(const Instruction i) const {
  return RegisterList(kCondsDontCare).Add(d_hi.reg(i)).Add(d_lo.reg(i));
}

SafetyLevel Defs12To19CondsDontCare::safety(const Instruction i) const {
  if (d_hi.reg(i).Equals(d_lo.reg(i)))
    return UNPREDICTABLE;
  if (defs(i).Contains(kRegisterPc))
    return FORBIDDEN;
  return MAY_BE_SAFE;
}

SafetyLevel Defs12To19CondsDontCareRdRmRnNotPc::
safety(const Instruction i) const {
  if (d_hi.reg(i).Equals(d_lo.reg(i)))
    return UNPREDICTABLE;
  if (defs(i).Add(n.reg(i)).Add(m.reg(i)).Contains(kRegisterPc))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

SafetyLevel Defs12To15CondsDontCareRnRdRmNotPc::safety(
    const Instruction i) const {
  if (RegisterList(n.reg(i)).Add(d.reg(i)).Add(m.reg(i)).
      Contains(kRegisterPc))
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
      && defs(i).Contains(kConditions);
}

bool MaskAddress::clears_bits(const Instruction i, uint32_t mask) const {
  return (imm12.get_modified_immediate(i) & mask) == mask;
}

SafetyLevel VfpOp::safety(const Instruction i) const {
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;
  switch (coproc.value(i)) {
    default: return FORBIDDEN;

    case 10:
    case 11:  // NEON/VFP
      return MAY_BE_SAFE;
  }
}

Register BasedAddressUsingRn::base_address_register(Instruction i) const {
  return n.reg(i);
}

SafetyLevel LoadBasedMemory::safety(const Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(n.reg(i)).Add(t.reg(i)).Contains(kRegisterPc)) {
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
  if (RegisterList(n.reg(i)).Add(t2.reg(i)).Contains(kRegisterPc)) {
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
      Contains(kRegisterPc)) {
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
      Contains(kRegisterPc)) {
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
  if (t.reg(i).Equals(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(kRegisterPc) || n.reg(i).Equals(t.reg(i)))) {
    return UNPREDICTABLE;
  }

  // Don't let addressing writeback alter PC (NaCl constraint).
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList LoadBasedMemoryWithWriteBack::defs(const Instruction i) const {
  return RegisterList(t.reg(i)).Add(HasWriteBack(i) ? n.reg(i) : kRegisterNone);
}

SafetyLevel LoadBasedOffsetMemory::safety(const Instruction i) const {
  // If pre-indexing is set, the address of the load is computed as the sum
  // of the two register parameters.  We have checked that the first register
  // is within the sandbox, but this would allow adding an arbitrary value
  // to it, so it is not safe. (NaCl constraint).
  if (indexing.IsPreIndexing(i))
    return FORBIDDEN;

  // Arm restrictions for this instruction.
  if (m.reg(i).Equals(kRegisterPc))
    return UNPREDICTABLE;

  return LoadBasedMemoryWithWriteBack::safety(i);
}

SafetyLevel LoadBasedOffsetMemoryDouble::safety(const Instruction i) const {
  // Arm double width restrictions for this instruction.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (t2.reg(i).Equals(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) && n.reg(i).Equals(t2.reg(i))) {
    return UNPREDICTABLE;
  }

  // Now apply non-double width restrictions for this instruction.
  return LoadBasedOffsetMemory::safety(i);
}

RegisterList LoadBasedOffsetMemoryDouble::defs(Instruction i) const {
  return LoadBasedOffsetMemory::defs(i).Add(t2.reg(i));
}

RegisterList LoadBasedImmedMemory::
immediate_addressing_defs(const Instruction i) const {
  return RegisterList(HasWriteBack(i) ? n.reg(i) : kRegisterNone);
}

bool LoadBasedImmedMemory::offset_is_immediate(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return true;
}

SafetyLevel LoadBasedImmedMemoryDouble::safety(const Instruction i) const {
  // Arm double width restrictions for this instruction.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (t2.reg(i).Equals(kRegisterPc)) {
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
  if (t.reg(i).Equals(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(kRegisterPc))) {
    // NOTE: The manual states that that it is also unpredictable
    // when HasWriteBack(i) and Rn=Rt. However, the compilers
    // may not check for this. For the moment, we are changing
    // the code to ignore this case for stores.
    // TODO(karl): Should we not allow this?
    return UNPREDICTABLE;
  }

  // Don't let addressing writeback alter PC (NaCl constraint).
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList StoreBasedMemoryWithWriteBack::defs(const Instruction i) const {
  return RegisterList(HasWriteBack(i) ? n.reg(i) : kRegisterNone);
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
  if (m.reg(i).Equals(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(kRegisterPc))) {
    // NOTE: The manual states that that it is also unpredictable
    // when HasWriteBack(i) and Rn=Rt. However, the compilers
    // may not check for this. For the moment, we are changing
    // the code to ignore this case for stores.
    // TODO(karl): Should we not allow this?
    return UNPREDICTABLE;
  }

  return StoreBasedMemoryWithWriteBack::safety(i);
}

SafetyLevel StoreBasedOffsetMemoryDouble::safety(const Instruction i) const {
  // Arm double width restrictions for this instruction.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (RegisterList(t2.reg(i)).Add(m.reg(i)).Contains(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  // NOTE: The manual states that that it is also unpredictable
  // when HasWriteBack(i) and Rn=Rt or Rn=Rt2. However, the compilers
  // may not check for this. For the moment, we are changing
  // the code to ignore this case for stores.
  // TODO(karl): Should we not allow this?
  // if (HasWriteBack(i) &&
  //     (n.reg(i).Equals(kRegisterPc))) {
  //   return UNPREDICTABLE;
  // }

  // Now apply non-double width restrictions for this instruction.
  return StoreBasedOffsetMemory::safety(i);
}

RegisterList StoreBasedImmedMemory::
immediate_addressing_defs(const Instruction i) const {
  return RegisterList(HasWriteBack(i) ? n.reg(i) : kRegisterNone);
}

SafetyLevel StoreBasedImmedMemoryDouble::safety(const Instruction i) const {
  // Arm double width restrictions for this instruction.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (t2.reg(i).Equals(kRegisterPc)) {
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

// Breakpoint

bool Breakpoint::is_literal_pool_head(const Instruction i) const {
  return i.GetCondition() == Instruction::AL
      && i.Bits(19, 8) == 0x777
      && i.Bits(3, 0) == 0x7;
}

// Data processing and arithmetic
SafetyLevel DataProc::safety(const Instruction i) const {
  if (defs(i).Contains(kRegisterPc)) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList DataProc::defs(const Instruction i) const {
  return RegisterList(Rd(i)).
      Add(UpdatesConditions(i) ? kConditions : kRegisterNone);
}

SafetyLevel PackSatRev::safety(const Instruction i) const {
  if (defs(i).Contains(kRegisterPc)) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList PackSatRev::defs(const Instruction i) const {
  return RegisterList(Rd(i)).Add(kConditions);
}


SafetyLevel Multiply::safety(const Instruction i) const {
  if (defs(i).Contains(kRegisterPc)) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList Multiply::defs(const Instruction i) const {
  return RegisterList(Rd(i)).Add(kConditions);
}


RegisterList LongMultiply::defs(const Instruction i) const {
  return RegisterList(RdHi(i)).Add(RdLo(i));
}


RegisterList SatAddSub::defs(const Instruction i) const {
  return DataProc::defs(i).Add(kConditions);
}


// MSR

RegisterList MoveToStatusRegister::defs(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList(kConditions);
}


// Stores

SafetyLevel StoreImmediate::safety(const Instruction i) const {
  // Don't let addressing writeback alter PC.
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

RegisterList StoreImmediate::defs(const Instruction i) const {
  return immediate_addressing_defs(i);
}

RegisterList StoreImmediate::immediate_addressing_defs(
    const Instruction i) const {
  return RegisterList((!PreindexingFlag(i) || WritesFlag(i))
                      ? base_address_register(i) : kRegisterNone);
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
  if (Rn(i).Equals(kRegisterPc) || (i.Bits(15, 0) == 0)) {
    return UNPREDICTABLE;
  }
  if (defs(i).Contains(kRegisterPc)) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList LoadMultiple::defs(const Instruction i) const {
  return RegisterList(i.Bits(15, 0)).Union(immediate_addressing_defs(i));
}

RegisterList LoadMultiple::immediate_addressing_defs(
    const Instruction i) const {
  return RegisterList(WritesFlag(i) ? Rn(i) : kRegisterNone);
}

Register LoadMultiple::base_address_register(const Instruction i) const {
  return Rn(i);
}

// Vector load/stores

SafetyLevel VectorLoad::safety(Instruction i) const {
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

RegisterList VectorLoad::defs(Instruction i) const {
  // Rm == PC indicates no address writeback.  Otherwise Rn is affected.
  return RegisterList(!Rm(i).Equals(kRegisterPc) ? Rn(i) : kRegisterNone);
}

RegisterList VectorLoad::immediate_addressing_defs(Instruction i) const {
  // Rm == SP indicates automatic update based on size of load, and
  // updated by small static displacement.
  return RegisterList(Rm(i).Equals(kRegisterStack) ? Rn(i) : kRegisterNone);
}

Register VectorLoad::base_address_register(const Instruction i) const {
  return Rn(i);
}


SafetyLevel VectorStore::safety(Instruction i) const {
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

RegisterList VectorStore::defs(Instruction i) const {
  // Rm == PC indicates no address writeback.  Otherwise Rn is affected.
  return RegisterList(!Rm(i).Equals(kRegisterPc)
                      ? base_address_register(i) : kRegisterNone);
}

RegisterList VectorStore::immediate_addressing_defs(Instruction i) const {
  // Rm == SP indicates automatic update based on size of store.
  return RegisterList(Rm(i).Equals(kRegisterStack)
                      ? base_address_register(i) : kRegisterNone);
}

Register VectorStore::base_address_register(Instruction i) const {
  return Rn(i);
}


// Coprocessors

SafetyLevel CoprocessorOp::safety(Instruction i) const {
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;
  switch (CoprocIndex(i)) {
    default: return FORBIDDEN;

    case 10:
    case 11:  // NEON/VFP
      return MAY_BE_SAFE;
  }
}

RegisterList LoadCoprocessor::defs(Instruction i) const {
  return immediate_addressing_defs(i);
}

RegisterList LoadCoprocessor::immediate_addressing_defs(Instruction i) const {
  return RegisterList(WritesFlag(i) ? Rn(i) : kRegisterNone);
}

RegisterList StoreCoprocessor::defs(Instruction i) const {
  return immediate_addressing_defs(i);
}

RegisterList StoreCoprocessor::immediate_addressing_defs(Instruction i) const {
  return RegisterList(WritesFlag(i) ? base_address_register(i) : kRegisterNone);
}

Register StoreCoprocessor::base_address_register(Instruction i) const {
  return Rn(i);
}


RegisterList MoveFromCoprocessor::defs(Instruction i) const {
  return RegisterList(Rt(i));
}


RegisterList MoveDoubleFromCoprocessor::defs(Instruction i) const {
  return RegisterList(Rt(i)).Add(Rt2(i));
}

// Control flow

SafetyLevel BxBlx::safety(Instruction i) const {
  if (link_register.IsUpdated(i) && m.reg(i).Equals(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  return MAY_BE_SAFE;
}

RegisterList BxBlx::defs(const Instruction i) const {
  return RegisterList(kRegisterPc).
      Add(link_register.IsUpdated(i) ? kRegisterLink : kRegisterNone);
}

Register BxBlx::branch_target_register(const Instruction i) const {
  return m.reg(i);
}

RegisterList Branch::defs(const Instruction i) const {
  return RegisterList(kRegisterPc).
      Add(PreindexingFlag(i) ? kRegisterLink : kRegisterNone);
}

int32_t Branch::branch_target_offset(const Instruction i) const {
  // Sign extend and shift left 2:
  int32_t offset = (int32_t)(i.Bits(23, 0) << 8) >> 6;
  return offset + 8;  // because r15 reads as 8 bytes ahead
}

// Unary1RegisterSet
SafetyLevel Unary1RegisterSet::safety(const Instruction i) const {
  if (read_spsr.IsDefined(i)) {
    return UNPREDICTABLE;
  }

  if (d.reg(i).Equals(kRegisterPc)) {
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

  if (n.reg(i).Equals(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  return MAY_BE_SAFE;
}

RegisterList Unary1RegisterUse::defs(const Instruction i) const {
  return RegisterList((mask.value(i) < 2) ? kRegisterNone : kConditions);
}

// Unary1RegisterBitRange
SafetyLevel Unary1RegisterBitRange::safety(Instruction i) const {
  if (d.reg(i).Equals(kRegisterPc)) return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Unary1RegisterBitRange::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

bool Unary1RegisterBitRange::clears_bits(Instruction i, uint32_t mask) const {
  int msbit = msb.value(i);
  int lsbit = lsb.value(i);
  int width = msbit + 1 - lsbit;
  if (width == 32) {
    return mask == 0;
  } else {
    uint32_t bit_mask = (((1 << width) - 1) << lsbit);
    return (bit_mask & mask) == mask;
  }
}

}  // namespace
