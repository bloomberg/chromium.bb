/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 * Copyright 2009, Google Inc.
 */

#include "native_client/src/trusted/validator_arm/v2/inst_classes.h"

/*
 * Implementations of instruction classes, for those not completely defined in
 * the header.
 */

namespace nacl_arm_dec {


bool Breakpoint::is_literal_pool_head(const Instruction i) const {
  return i.condition() == Instruction::AL
      && i.bits(19, 8) == 0x777
      && i.bits(3, 0) == 0x7;
}


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


bool ImmediateBic::clears_bits(const Instruction i, uint32_t mask) const {
  // This implements the Modified Immediate field described in Section A5.2.4.
  // We haven't pulled this out into a utility function because we only use it
  // once.
  int rotation = i.bits(11, 8) * 2;
  uint32_t value = i.bits(7, 0);

  uint32_t immediate = (value >> rotation) | (value << (32 - rotation));
  return (immediate & mask) == mask;
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


RegisterList LongMultiply15_8::defs(const Instruction i) const {
  return kRegisterFlags + i.reg(15, 12) + i.reg(11, 8);
}


RegisterList SatAddSub::defs(const Instruction i) const {
  return DataProc::defs(i) + kRegisterFlags;
}


RegisterList MoveToStatusRegister::defs(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return kRegisterFlags;
}


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


SafetyLevel Load::safety(const Instruction i) const {
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList Load::defs(const Instruction i) const {
  return i.reg(15, 12) + immediate_addressing_defs(i);
}

RegisterList Load::immediate_addressing_defs(const Instruction i) const {
  bool p = i.bit(24);
  bool w = i.bit(21);
  if (!p) return i.reg(19, 16);
  if (w) return i.reg(19, 16);
  return kRegisterNone;
}


RegisterList LoadDouble::defs(const Instruction i) const {
  // For rN in bits 15:12, this writes both rN and rN + 1
  return i.reg(15, 12) + Register(i.bits(15, 12) + 1)
      + immediate_addressing_defs(i);
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


SafetyLevel VectorLoad::safety(Instruction i) const {
  /*
   * The vector addressing mode uses PC and SP as fake registers to indicate
   * no displacement and no-displacement-post-increment, respectively.
   */
  Register displacement = i.reg(3, 0);
  if (displacement != kRegisterPc && displacement != kRegisterStack) {
    return FORBIDDEN_OPERANDS;
  }
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList VectorLoad::defs(Instruction i) const {
  return immediate_addressing_defs(i);
}

RegisterList VectorLoad::immediate_addressing_defs(Instruction i) const {
  // Rm == SP indicates post-increment addressing.
  if (i.reg(3, 0) == kRegisterStack) {
    return i.reg(19, 16);
  }

  return kRegisterNone;
}


SafetyLevel VectorStore::safety(Instruction i) const {
  /*
   * The vector addressing mode uses PC and SP as fake registers to indicate
   * no displacement and no-displacement-post-increment, respectively.
   */
  Register displacement = i.reg(3, 0);
  if (displacement != kRegisterPc && displacement != kRegisterStack) {
    return FORBIDDEN_OPERANDS;
  }
  if (defs(i)[kRegisterPc]) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList VectorStore::defs(Instruction i) const {
  return immediate_addressing_defs(i);
}

RegisterList VectorStore::immediate_addressing_defs(Instruction i) const {
  // Rm == SP indicates post-increment addressing.
  if (i.reg(3, 0) == kRegisterStack) {
    return base_address_register(i);
  }

  return kRegisterNone;
}

bool VectorStore::writes_memory(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return true;
}

Register VectorStore::base_address_register(Instruction i) const {
  return i.reg(19, 16);
}


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


RegisterList BxBlx::defs(const Instruction i) const {
  return kRegisterPc + (i.bit(5)? kRegisterLink : kRegisterNone);
}

Register BxBlx::branch_target_register(const Instruction i) const {
  return i.reg(3, 0);
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


RegisterList Branch::defs(const Instruction i) const {
  return kRegisterPc + (i.bit(24)? kRegisterLink : kRegisterNone);
}

int32_t Branch::branch_target_offset(const Instruction i) const {
  // Sign extend and shift left 2:
  int32_t offset = (int32_t)(i.bits(23, 0) << 8) >> 6;
  return offset + 8;  // because r15 reads as 8 bytes ahead
}

}  // namespace
