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

RegisterList Defs12To15::defs(const Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

SafetyLevel Defs12To15RdRnRsRmNotPc::safety(const Instruction i) const {
  if (RegisterList(d.reg(i)).Add(n.reg(i)).Add(s.reg(i)).
      Add(m.reg(i)).Contains(kRegisterPc))
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


SafetyLevel StoreRegister::safety(const Instruction i) const {
  if (PreindexingFlag(i)) {
    // Computes base address by adding two registers -- cannot predict!
    return FORBIDDEN;
  }

  // Don't let addressing writeback alter PC.
  if (defs(i).Contains(kRegisterPc))
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList StoreRegister::defs(const Instruction i) const {
  // Only one form of register-register store doesn't writeback its base:
  //   str rT, [rN, rM]
  // We ban this form.  Thus, every safe form alters its base address reg.
  return RegisterList(base_address_register(i));
}

Register StoreRegister::base_address_register(const Instruction i) const {
  return Rn(i);
}

SafetyLevel StrRegister::safety(const Instruction i) const {
  if (indexing.IsPreIndexing(i)) {
    // If pre-indexing is set, the address of the load is computed as the sum
    // of the two register parameters.  We have checked that the first register
    // is within the sandbox, but this would allow adding an arbitrary value
    // to it, so it is not safe. (NaCl constraint).
    return FORBIDDEN;
  }

  // Arm restrictions for this instruction.
  if (RegisterList(t.reg(i)).Add(m.reg(i)).Contains(kRegisterPc)) {
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

SafetyLevel StoreExclusive::safety(const Instruction i) const {
  // Don't let addressing writeback alter PC.
  if (defs(i).Contains(kRegisterPc))
    return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

RegisterList StoreExclusive::defs(const Instruction i) const {
  return RegisterList(Rd(i));
}

Register StoreExclusive::base_address_register(const Instruction i) const {
  return Rn(i);
}


// Loads

bool AbstractLoad::writeback(const Instruction i) const {
  // Algorithm defined in pseudocode in ARM instruction set spec
  // Valid for LDR, LDR[BH], LDRD
  // Not valid for LDREX and kin
  return !PreindexingFlag(i) | WritesFlag(i);
}

SafetyLevel AbstractLoad::safety(const Instruction i) const {
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList AbstractLoad::defs(const Instruction i) const {
  return immediate_addressing_defs(i).Add(Rt(i));
}


SafetyLevel LoadRegister::safety(const Instruction i) const {
  bool pre_index = PreindexingFlag(i);
  if (pre_index) {
    // If pre_index is set, the address of the load is computed as the sum
    // of the two register parameters.  We have checked that the first register
    // is within the sandbox, but this would allow adding an arbitrary value
    // to it, so it is not safe.
    return FORBIDDEN;
  }

  // Don't let addressing writeback alter PC.
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList LoadRegister::defs(const Instruction i) const {
  return RegisterList(writeback(i) ? Rn(i) : kRegisterNone);
}

Register LoadRegister::base_address_register(const Instruction i) const {
  return Rn(i);
}

SafetyLevel LdrRegister::safety(const Instruction i) const {
  if (indexing.IsPreIndexing(i)) {
    // If pre-indexing is set, the address of the load is computed as the sum
    // of the two register parameters.  We have checked that the first register
    // is within the sandbox, but this would allow adding an arbitrary value
    // to it, so it is not safe. (NaCl constraint).
    return FORBIDDEN;
  }

  // Arm restrictions for this instruction.
  if (RegisterList(t.reg(i)).Add(m.reg(i)).Contains(kRegisterPc)) {
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

RegisterList LoadImmediate::immediate_addressing_defs(const Instruction i)
    const {
  return RegisterList(writeback(i) ? Rn(i) : kRegisterNone);
}

Register LoadImmediate::base_address_register(const Instruction i) const {
  return Rn(i);
}

bool LoadImmediate::offset_is_immediate(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return true;
}


RegisterList LoadDoubleI::defs(const Instruction i) const {
  return LoadImmediate::defs(i).Add(Rt2(i));
}

Register LoadDoubleI::base_address_register(const Instruction i) const {
  return Rn(i);
}

bool LoadDoubleI::offset_is_immediate(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return true;
}

SafetyLevel LoadDoubleR::safety(const Instruction i) const {
  if (PreindexingFlag(i)) {
    // If pre_index is set, the address of the load is computed as the sum
    // of the two register parameters.  We have checked that the first register
    // is within the sandbox, but this would allow adding an arbitrary value
    // to it, so it is not safe.
    return FORBIDDEN;
  }

  // Arm restrictions for this instruction.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (RegisterList(t2.reg(i)).Add(m.reg(i)).Contains(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(kRegisterPc) || n.reg(i).Equals(t.reg(i)) ||
       n.reg(i).Equals(t2.reg(i)))) {
    return UNPREDICTABLE;
  }

  // Don't let addressing writeback alter PC.
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList LoadDoubleR::defs(const Instruction i) const {
  return LoadRegister::defs(i).Add(Rt2(i));
}

Register LoadDoubleR::base_address_register(const Instruction i) const {
  // TODO(karl) Decide if this is already covered through inheritance.
  return Rn(i);
}

SafetyLevel StoreDoubleR::safety(const Instruction i) const {
  if (PreindexingFlag(i)) {
    // If pre_index is set, the address of the load is computed as the sum
    // of the two register parameters.  We have checked that the first register
    // is within the sandbox, but this would allow adding an arbitrary value
    // to it, so it is not safe.
    return FORBIDDEN;
  }

  // Arm restrictions for this instruction.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (RegisterList(t2.reg(i)).Add(m.reg(i)).Contains(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(kRegisterPc) || n.reg(i).Equals(t.reg(i)) ||
       n.reg(i).Equals(t2.reg(i)))) {
    return UNPREDICTABLE;
  }

  // Don't let addressing writeback alter PC.
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

Register LoadExclusive::base_address_register(const Instruction i) const {
  return Rn(i);
}

RegisterList LoadDoubleExclusive::defs(const Instruction i) const {
  return LoadExclusive::defs(i).Add(Rt2(i));
}

Register LoadDoubleExclusive::base_address_register(const Instruction i) const {
  return Rn(i);
}


SafetyLevel LoadMultiple::safety(const Instruction i) const {
  // Bottom 16 bits is a register list.
  if (WritesFlag(i) && i.Bit(Rn(i).number())) {
    // In ARMv7, cannot update base register both by popping and by indexing.
    // (Pre-v7 this was still a weird thing to do.)
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

RegisterList BxBlx::defs(const Instruction i) const {
  return RegisterList(kRegisterPc).
      Add(UsesLinkRegister(i) ? kRegisterLink : kRegisterNone);
}

Register BxBlx::branch_target_register(const Instruction i) const {
  return Rm(i);
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

}  // namespace
