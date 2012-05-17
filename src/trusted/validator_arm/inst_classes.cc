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

}  // namespace
