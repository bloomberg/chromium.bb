/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>
#include "native_client/src/trusted/validator_arm/inst_classes.h"

// Implementations of instruction classes, for those not completely defined in

namespace nacl_arm_dec {

bool ClassDecoder::IsInstanceOf(const char* class_name) const {
  return 0 == strcmp(name(), class_name);
}

// A utility function: given a modified-immediate-form instruction, extracts
// the immediate value.  This is used to analyze BIC and TST.
//
// This encoding is described in Section A5.2.4.
static uint32_t get_modified_immediate(Instruction i) {
  int rotation = i.bits(11, 8) * 2;
  uint32_t value = i.bits(7, 0);

  if (rotation == 0) return value;

  return (value >> rotation) | (value << (32 - rotation));
}

// Breakpoint

bool Breakpoint::is_literal_pool_head(const Instruction i) const {
  return i.condition() == Instruction::AL
      && i.bits(19, 8) == 0x777
      && i.bits(3, 0) == 0x7;
}

// Binary4RegisterShiftedOp
SafetyLevel Binary4RegisterShiftedOp::safety(Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if ((Rn(i) == kRegisterPc) ||
      (Rd(i) == kRegisterPc) ||
      (Rs(i) == kRegisterPc) ||
      (Rm(i) == kRegisterPc)) return UNPREDICTABLE;
  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Binary4RegisterShiftedOp::defs(const Instruction i) const {
  return Rd(i) + (UpdatesFlagsRegister(i) ? kRegisterFlags : kRegisterNone);
}

// Unary3RegisterShiftedOp
SafetyLevel Unary3RegisterShiftedOp::safety(Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if ((Rd(i) == kRegisterPc) ||
      (Rs(i) == kRegisterPc) ||
      (Rm(i) == kRegisterPc)) return UNPREDICTABLE;
  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Unary3RegisterShiftedOp::defs(const Instruction i) const {
  return Rd(i) + (UpdatesFlagsRegister(i) ? kRegisterFlags : kRegisterNone);
}

// Data processing and arithmetic
SafetyLevel DataProc::safety(const Instruction i) const {
  if (defs(i)[kRegisterPc]) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList DataProc::defs(const Instruction i) const {
  return Rd(i) + (UpdatesFlagsRegister(i) ? kRegisterFlags : kRegisterNone);
}

// Binary3RegisterShiftedTest
SafetyLevel Binary3RegisterShiftedTest::safety(Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if ((Rn(i) == kRegisterPc) ||
      (Rs(i) == kRegisterPc) ||
      (Rm(i) == kRegisterPc)) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

RegisterList Binary3RegisterShiftedTest::defs(const Instruction i) const {
  return (UpdatesFlagsRegister(i) ? kRegisterFlags : kRegisterNone);
}

RegisterList Test::defs(const Instruction i) const {
  return (UpdatesFlagsRegister(i) ? kRegisterFlags : kRegisterNone);
}


bool TestImmediate::sets_Z_if_bits_clear(Instruction i,
                                         Register r,
                                         uint32_t mask) const {
  // Rn = 19:16 for TST(immediate) - section A8.6.230
  return Rn(i) == r
      && (get_modified_immediate(i) & mask) == mask
      && defs(i)[kRegisterFlags];
}


bool ImmediateBic::clears_bits(const Instruction i, uint32_t mask) const {
  return (get_modified_immediate(i) & mask) == mask;
}


SafetyLevel PackSatRev::safety(const Instruction i) const {
  if (defs(i)[kRegisterPc]) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList PackSatRev::defs(const Instruction i) const {
  return Rd(i) + kRegisterFlags;
}


SafetyLevel Multiply::safety(const Instruction i) const {
  if (defs(i)[kRegisterPc]) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList Multiply::defs(const Instruction i) const {
  return kRegisterFlags + Rd(i);
}


RegisterList LongMultiply::defs(const Instruction i) const {
  return RdHi(i) + RdLo(i);
}


RegisterList SatAddSub::defs(const Instruction i) const {
  return DataProc::defs(i) + kRegisterFlags;
}


// MSR

RegisterList MoveToStatusRegister::defs(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return kRegisterFlags;
}


// Stores

SafetyLevel StoreImmediate::safety(const Instruction i) const {
  // Don't let addressing writeback alter PC.
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList StoreImmediate::defs(const Instruction i) const {
  return immediate_addressing_defs(i);
}

RegisterList StoreImmediate::immediate_addressing_defs(
    const Instruction i) const {
  if (!PreindexingFlag(i) || WritesFlag(i)) return base_address_register(i);
  return kRegisterNone;
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
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList StoreRegister::defs(const Instruction i) const {
  // Only one form of register-register store doesn't writeback its base:
  //   str rT, [rN, rM]
  // We ban this form.  Thus, every safe form alters its base address reg.
  return base_address_register(i);
}

Register StoreRegister::base_address_register(const Instruction i) const {
  return Rn(i);
}


SafetyLevel StoreExclusive::safety(const Instruction i) const {
  // Don't let addressing writeback alter PC.
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList StoreExclusive::defs(const Instruction i) const {
  return Rd(i);
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
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList AbstractLoad::defs(const Instruction i) const {
  return Rt(i) + immediate_addressing_defs(i);
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
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList LoadRegister::defs(const Instruction i) const {
  if (writeback(i)) {
    return AbstractLoad::defs(i) + Rn(i);
  } else {
    return AbstractLoad::defs(i);
  }
}

Register LoadRegister::base_address_register(const Instruction i) const {
  return Rn(i);
}


RegisterList LoadImmediate::immediate_addressing_defs(const Instruction i)
    const {
  if (writeback(i)) {
    return Rn(i);
  } else {
    return kRegisterNone;
  }
}

Register LoadImmediate::base_address_register(const Instruction i) const {
  return Rn(i);
}

bool LoadImmediate::offset_is_immediate(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return true;
}


RegisterList LoadDoubleI::defs(const Instruction i) const {
  return LoadImmediate::defs(i) + Rt2(i);
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

  // Don't let addressing writeback alter PC.
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList LoadDoubleR::defs(const Instruction i) const {
  return LoadRegister::defs(i) + Rt2(i);
}

Register LoadDoubleR::base_address_register(const Instruction i) const {
  return Rn(i);
}

Register LoadExclusive::base_address_register(const Instruction i) const {
  return Rn(i);
}

RegisterList LoadDoubleExclusive::defs(const Instruction i) const {
  return LoadExclusive::defs(i) + Rt2(i);
}

Register LoadDoubleExclusive::base_address_register(const Instruction i) const {
  return Rn(i);
}


SafetyLevel LoadMultiple::safety(const Instruction i) const {
  // Bottom 16 bits is a register list.
  if (WritesFlag(i) && i.bit(Rn(i).number())) {
    // In ARMv7, cannot update base register both by popping and by indexing.
    // (Pre-v7 this was still a weird thing to do.)
    return UNPREDICTABLE;
  }

  if (defs(i)[kRegisterPc]) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList LoadMultiple::defs(const Instruction i) const {
  return RegisterList(i.bits(15, 0)) + immediate_addressing_defs(i);
}

RegisterList LoadMultiple::immediate_addressing_defs(
    const Instruction i) const {
  return WritesFlag(i) ? Rn(i) : kRegisterNone;
}

Register LoadMultiple::base_address_register(const Instruction i) const {
  return Rn(i);
}


// Vector load/stores

SafetyLevel VectorLoad::safety(Instruction i) const {
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList VectorLoad::defs(Instruction i) const {
  // Rm == PC indicates no address writeback.  Otherwise Rn is affected.
  if (Rm(i) != kRegisterPc) {
    return Rn(i);
  }
  return kRegisterNone;
}

RegisterList VectorLoad::immediate_addressing_defs(Instruction i) const {
  // Rm == SP indicates automatic update based on size of load.
  if (Rm(i) == kRegisterStack) {
    // Rn is updated by a small static displacement.
    return Rn(i);
  }

  // Any writeback is not treated as immediate otherwise.
  return kRegisterNone;
}

Register VectorLoad::base_address_register(const Instruction i) const {
  return Rn(i);
}


SafetyLevel VectorStore::safety(Instruction i) const {
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList VectorStore::defs(Instruction i) const {
  // Rm == PC indicates no address writeback.  Otherwise Rn is affected.
  if (Rm(i) != kRegisterPc) {
    return base_address_register(i);
  }

  return kRegisterNone;
}

RegisterList VectorStore::immediate_addressing_defs(Instruction i) const {
  // Rm == SP indicates automatic update based on size of store.
  if (Rm(i) == kRegisterStack) {
    // Rn is updated by a small static displacement.
    return base_address_register(i);
  }

  // Any writeback is not treated as immediate otherwise.
  return kRegisterNone;
}

bool VectorStore::writes_memory(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return true;
}

Register VectorStore::base_address_register(Instruction i) const {
  return Rn(i);
}


// Coprocessors

SafetyLevel CoprocessorOp::safety(Instruction i) const {
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;
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
  return WritesFlag(i) ? Rn(i) : kRegisterNone;
}

RegisterList StoreCoprocessor::defs(Instruction i) const {
  return immediate_addressing_defs(i);
}

RegisterList StoreCoprocessor::immediate_addressing_defs(Instruction i) const {
  return WritesFlag(i) ? base_address_register(i) : kRegisterNone;
}

Register StoreCoprocessor::base_address_register(Instruction i) const {
  return Rn(i);
}


RegisterList MoveFromCoprocessor::defs(Instruction i) const {
  return Rt(i);
}


RegisterList MoveDoubleFromCoprocessor::defs(Instruction i) const {
  return Rt(i) + Rt2(i);
}

// Control flow

RegisterList BxBlx::defs(const Instruction i) const {
  return kRegisterPc + (UsesLinkRegister(i) ? kRegisterLink : kRegisterNone);
}

Register BxBlx::branch_target_register(const Instruction i) const {
  return Rm(i);
}


RegisterList Branch::defs(const Instruction i) const {
  return kRegisterPc + (PreindexingFlag(i) ? kRegisterLink : kRegisterNone);
}

int32_t Branch::branch_target_offset(const Instruction i) const {
  // Sign extend and shift left 2:
  int32_t offset = (int32_t)(i.bits(23, 0) << 8) >> 6;
  return offset + 8;  // because r15 reads as 8 bytes ahead
}

}  // namespace
