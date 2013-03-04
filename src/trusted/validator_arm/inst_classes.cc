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

SafetyLevel Int2SafetyLevel(uint32_t i) {
  switch (i) {
    case UNINITIALIZED:
      return UNINITIALIZED;
    case UNKNOWN:
      return UNKNOWN;
    default:
    case UNDEFINED:
      return UNDEFINED;
    case NOT_IMPLEMENTED:
      return NOT_IMPLEMENTED;
    case UNPREDICTABLE:
      return UNPREDICTABLE;
    case DEPRECATED:
      return DEPRECATED;
    case FORBIDDEN:
      return FORBIDDEN;
    case FORBIDDEN_OPERANDS:
      return FORBIDDEN_OPERANDS;
    case DECODER_ERROR:
      return DECODER_ERROR;
    case MAY_BE_SAFE:
      return MAY_BE_SAFE;
  }
}

// ClassDecoder
RegisterList ClassDecoder::defs(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList::Everything();
}

RegisterList ClassDecoder::uses(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

bool ClassDecoder::base_address_register_writeback_small_immediate(
    Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return false;
}

Register ClassDecoder::base_address_register(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return Register::None();
}

bool ClassDecoder::is_literal_load(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return false;
}

Register ClassDecoder::branch_target_register(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return Register::None();
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

bool ClassDecoder::is_load_thread_address_pointer(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return false;
}

Instruction ClassDecoder::
dynamic_code_replacement_sentinel(Instruction i) const {
  return i;
}

}  // namespace nacl_arm_dec
