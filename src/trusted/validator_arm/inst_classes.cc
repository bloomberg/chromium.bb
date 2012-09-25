/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_arm/inst_classes.h"

#include <assert.h>
#include <string.h>

// Implementations of instruction classes, for those not completely defined in
// in the header.

namespace nacl_arm_dec {


static const SafetyLevel kSafetyLevel[] = {
  UNKNOWN,
  UNDEFINED,
  UNPREDICTABLE,
  DEPRECATED,
  FORBIDDEN,
  FORBIDDEN_OPERANDS,
  MAY_BE_SAFE,
};

SafetyLevel Int2SafetyLevel(uint32_t i) {
  if (i <= nacl_arm_dec::MAY_BE_SAFE) {
    return kSafetyLevel[i];
  }
  return UNKNOWN;
}

uint32_t ShiftTypeBits5To6Interface::ComputeDecodeImmShift(
    uint32_t shift_type, uint32_t imm5_value) {
  switch (shift_type) {
    case 0:
      return imm5_value;
    case 1:
    case 2:
      return imm5_value == 0 ? 32 : imm5_value;
    case 3:
      return imm5_value == 0 ? 1 : imm5_value;
    default:
      assert(false);  // This should not happen, shift_type is two bits.
      return imm5_value;
  }
}

// This encoding is described in Section A5.2.4.
uint32_t Imm12Bits0To11Interface::get_modified_immediate(Instruction i) {
  int rotation = i.Bits(11, 8) * 2;
  uint32_t value = i.Bits(7, 0);

  if (rotation == 0) return value;

  return (value >> rotation) | (value << (32 - rotation));
}

// ClassDecoder
RegisterList ClassDecoder::defs(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return kRegisterListEverything;
}

RegisterList ClassDecoder::immediate_addressing_defs(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

Register ClassDecoder::base_address_register(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return kRegisterNone;
}

bool ClassDecoder::is_literal_load(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return false;
}

Register ClassDecoder::branch_target_register(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return kRegisterNone;
}

bool ClassDecoder::is_relative_branch(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return false;
}

int32_t ClassDecoder::branch_target_offset(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return 0;
}

bool ClassDecoder::is_literal_pool_head(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return false;
}

bool ClassDecoder::clears_bits(Instruction i, uint32_t mask) const {
  UNREFERENCED_PARAMETER(i);
  UNREFERENCED_PARAMETER(mask);
  return false;
}

bool ClassDecoder::sets_Z_if_bits_clear(Instruction i,
                                        Register r,
                                        uint32_t mask) const {
  UNREFERENCED_PARAMETER(i);
  UNREFERENCED_PARAMETER(r);
  UNREFERENCED_PARAMETER(mask);
  return false;
}

// UnsafeClassDecoder
SafetyLevel UnsafeClassDecoder::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return safety_;
}

RegisterList UnsafeClassDecoder::defs(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

// CoprocessorOp
SafetyLevel CoprocessorOp::safety(const Instruction i) const {
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;
  switch (coproc.value(i)) {
    default: return FORBIDDEN;

    case 10:
    case 11:  // NEON/VFP
      return MAY_BE_SAFE;
  }
}

RegisterList CoprocessorOp::defs(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

}  // namespace nacl_arm_dec
