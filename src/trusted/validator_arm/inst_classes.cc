/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_arm/inst_classes.h"

/*
 * Implementations of instruction classes, for those not completely defined in
 * the header.
 */

namespace nacl_arm_dec {

/*
 * A utility function: given a modified-immediate-form instruction, extracts
 * the immediate value.  This is used to analyze BIC and TST.
 *
 * This encoding is described in Section A5.2.4.
 */
static uint32_t get_modified_immediate(Instruction i) {
  int rotation = i.bits(11, 8) * 2;
  uint32_t value = i.bits(7, 0);

  if (rotation == 0) return value;

  return (value >> rotation) | (value << (32 - rotation));
}

/*
 * Breakpoint
 */

bool Breakpoint::is_literal_pool_head(const Instruction i) const {
  return i.condition() == Instruction::AL
      && i.bits(19, 8) == 0x777
      && i.bits(3, 0) == 0x7;
}


/*
 * Data processing and arithmetic
 */

SafetyLevel DataProc::safety(const Instruction i) const {
  if (defs(i)[kRegisterPc]) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList DataProc::defs(const Instruction i) const {
  return i.reg(15, 12) + (i.bit(20)? kRegisterFlags : kRegisterNone);
}


RegisterList Test::defs(const Instruction i) const {
  return (i.bit(20)? kRegisterFlags : kRegisterNone);
}


bool TestImmediate::sets_Z_if_bits_clear(Instruction i,
                                         Register r,
                                         uint32_t mask) const {
  // Rn = 19:16 for TST(immediate) - section A8.6.230
  return i.reg(19, 16) == r
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
  return i.reg(15, 12) + kRegisterFlags;
}


SafetyLevel Multiply::safety(const Instruction i) const {
  if (defs(i)[kRegisterPc]) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList Multiply::defs(const Instruction i) const {
  return kRegisterFlags + i.reg(19, 16);
}


RegisterList LongMultiply::defs(const Instruction i) const {
  return Multiply::defs(i) + i.reg(15, 12);
}


RegisterList SatAddSub::defs(const Instruction i) const {
  return DataProc::defs(i) + kRegisterFlags;
}


/*
 * MSR
 */

RegisterList MoveToStatusRegister::defs(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return kRegisterFlags;
}


/*
 * Stores
 */

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
  bool p = i.bit(24);
  bool w = i.bit(21);
  if (!p) return base_address_register(i);
  if (w) return base_address_register(i);
  return kRegisterNone;
}

Register StoreImmediate::base_address_register(const Instruction i) const {
  return i.reg(19, 16);
}


SafetyLevel StoreRegister::safety(const Instruction i) const {
  bool pre_index = i.bit(24);
  if (pre_index) {
    // Computes base address by adding two registers -- cannot predict!
    return FORBIDDEN;
  }

  // Don't let addressing writeback alter PC.
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList StoreRegister::defs(const Instruction i) const {
  /*
   * Only one form of register-register store doesn't writeback its base:
   *   str rT, [rN, rM]
   * We ban this form.  Thus, every safe form alters its base address reg.
   */
  return base_address_register(i);
}

Register StoreRegister::base_address_register(const Instruction i) const {
  return i.reg(19, 16);
}


SafetyLevel StoreExclusive::safety(const Instruction i) const {
  // Don't let addressing writeback alter PC.
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList StoreExclusive::defs(const Instruction i) const {
  return i.reg(15, 12);
}

Register StoreExclusive::base_address_register(const Instruction i) const {
  return i.reg(19, 16);
}


/*
 * Loads
 */

bool AbstractLoad::writeback(const Instruction i) const {
  // Algorithm defined in pseudocode in ARM instruction set spec
  // Valid for LDR, LDR[BH], LDRD
  // Not valid for LDREX and kin
  bool p = i.bit(24);
  bool w = i.bit(21);
  return !p | w;
}

SafetyLevel AbstractLoad::safety(const Instruction i) const {
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList AbstractLoad::defs(const Instruction i) const {
  return i.reg(15, 12) + immediate_addressing_defs(i);
}


SafetyLevel LoadRegister::safety(const Instruction i) const {
  bool pre_index = i.bit(24);
  if (pre_index) {
    // Computes base address by adding two registers -- cannot predict!
    return FORBIDDEN;
  }

  // Don't let addressing writeback alter PC.
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList LoadRegister::defs(const Instruction i) const {
  if (writeback(i)) {
    Register rn(i.bits(19, 16));
    return AbstractLoad::defs(i) + rn;
  } else {
    return AbstractLoad::defs(i);
  }
}

Register LoadRegister::base_address_register(const Instruction i) const {
  return i.reg(19, 16);
}


RegisterList LoadImmediate::immediate_addressing_defs(const Instruction i)
    const {
  if (writeback(i)) {
    Register rn(i.bits(19, 16));
    return rn;
  } else {
    return kRegisterNone;
  }
}

Register LoadImmediate::base_address_register(const Instruction i) const {
  return i.reg(19, 16);
}

bool LoadImmediate::offset_is_immediate(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return true;
}


RegisterList LoadDoubleI::defs(const Instruction i) const {
  return LoadImmediate::defs(i) + Register(i.bits(15, 12) + 1);
}

Register LoadDoubleI::base_address_register(const Instruction i) const {
  return i.reg(19, 16);
}

bool LoadDoubleI::offset_is_immediate(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return true;
}


SafetyLevel LoadDoubleR::safety(const Instruction i) const {
  bool pre_index = i.bit(24);
  if (pre_index) {
    // Computes base address by adding two registers -- cannot predict!
    return FORBIDDEN;
  }

  // Don't let addressing writeback alter PC.
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList LoadDoubleR::defs(const Instruction i) const {
  return LoadRegister::defs(i) + Register(i.bits(15, 12) + 1);
}

Register LoadDoubleR::base_address_register(const Instruction i) const {
  return i.reg(19, 16);
}

RegisterList LoadDoubleExclusive::defs(const Instruction i) const {
  return LoadExclusive::defs(i) + Register(i.bits(15, 12) + 1);
}

Register LoadDoubleExclusive::base_address_register(const Instruction i) const {
  return i.reg(19, 16);
}


SafetyLevel LoadMultiple::safety(const Instruction i) const {
  uint32_t rn = i.bits(19, 16);
  if (i.bit(21) && i.bit(rn)) {
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
  return i.bit(21)? i.reg(19, 16) : kRegisterNone;
}

Register LoadMultiple::base_address_register(const Instruction i) const {
  return i.reg(19, 16);
}


/*
 * Vector load/stores
 */

SafetyLevel VectorLoad::safety(Instruction i) const {
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList VectorLoad::defs(Instruction i) const {
  // Rm == PC indicates no address writeback.  Otherwise Rn is affected.
  if (i.reg(3, 0) != kRegisterPc) {
    return i.reg(19, 16);
  }

  return kRegisterNone;
}

RegisterList VectorLoad::immediate_addressing_defs(Instruction i) const {
  // Rm == SP indicates automatic update based on size of load.
  if (i.reg(3, 0) == kRegisterStack) {
    // Rn is updated by a small static displacement.
    return i.reg(19, 16);
  }

  // Any writeback is not treated as immediate otherwise.
  return kRegisterNone;
}

Register VectorLoad::base_address_register(const Instruction i) const {
  return i.reg(19, 16);
}


SafetyLevel VectorStore::safety(Instruction i) const {
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList VectorStore::defs(Instruction i) const {
  // Rm == PC indicates no address writeback.  Otherwise Rn is affected.
  if (i.reg(3, 0) != kRegisterPc) {
    return base_address_register(i);
  }

  return kRegisterNone;
}

RegisterList VectorStore::immediate_addressing_defs(Instruction i) const {
  // Rm == SP indicates automatic update based on size of store.
  if (i.reg(3, 0) == kRegisterStack) {
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
  return i.reg(19, 16);
}


/*
 * Coprocessors
 */

SafetyLevel CoprocessorOp::safety(Instruction i) const {
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;
  uint32_t cpid = i.bits(11, 8);

  switch (cpid) {
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
  return i.bit(21)? i.reg(19, 16) : kRegisterNone;
}


RegisterList StoreCoprocessor::defs(Instruction i) const {
  return immediate_addressing_defs(i);
}

RegisterList StoreCoprocessor::immediate_addressing_defs(Instruction i) const {
  return i.bit(21)? base_address_register(i) : kRegisterNone;
}

Register StoreCoprocessor::base_address_register(Instruction i) const {
  return i.reg(19, 16);
}


RegisterList MoveFromCoprocessor::defs(Instruction i) const {
  Register rd = i.reg(15, 12);
  if (rd == kRegisterPc) {
    // Per ARM ISA spec: r15 here is a synonym for the flags register!
    return kRegisterFlags;
  }
  return rd;
}


RegisterList MoveDoubleFromCoprocessor::defs(Instruction i) const {
  return i.reg(15, 12) + i.reg(19, 16);
}


/*
 * Control flow
 */

RegisterList BxBlx::defs(const Instruction i) const {
  return kRegisterPc + (i.bit(5)? kRegisterLink : kRegisterNone);
}

Register BxBlx::branch_target_register(const Instruction i) const {
  return i.reg(3, 0);
}


RegisterList Branch::defs(const Instruction i) const {
  return kRegisterPc + (i.bit(24)? kRegisterLink : kRegisterNone);
}

int32_t Branch::branch_target_offset(const Instruction i) const {
  // Sign extend and shift left 2:
  int32_t offset = (int32_t)(i.bits(23, 0) << 8) >> 6;
  return offset + 8;  // because r15 reads as 8 bytes ahead
}

}  // namespace
