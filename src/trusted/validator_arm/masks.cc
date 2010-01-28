/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Routines for checking address-sandboxing masks.
 */

#include "native_client/src/trusted/validator_arm/masks.h"
#include "native_client/src/trusted/validator_arm/arm_insts_rt.h"
#include "native_client/src/trusted/validator_arm/register_set_use.h"

static const uint32_t kDataMask = 0x80000000;
bool CheckDataMask(const NcDecodeState &state, int32_t reg, int32_t condition) {
  /*
   * Many ARM instructions could conceivably produce a sandboxed data address,
   * but we only allow one: an immediate-operand BIC.
   */
  if (state.CurrentInstructionIs(ARM_BIC)) {
    const NcDecodedInstruction &inst = state.CurrentInstruction();

    // This is not strictly necessary, but we never generate a mask that sets
    // flags, so we disallow it until it proves necessary.
    if (GetBit(inst.inst, ARM_S_BIT)) return false;

    // We don't check that this is a register-immediate BIC, but that's okay:
    // the immediate field is 0 in other cases and will fail this check.
    uint32_t rhs = ImmediateRotateRight(inst.values.immediate,
        inst.values.shift * 2);
    return rhs == kDataMask
        && inst.values.arg2 == reg
        && inst.values.cond == condition;
  } else {
    return false;
  }
}

static const uint32_t kControlMask = 0xF000000F;
bool CheckControlMask(const NcDecodeState &state,
                      int32_t reg,
                      int32_t condition) {
  const NcDecodedInstruction &inst = state.CurrentInstruction();

  if (state.CurrentInstructionIs(ARM_BIC)) {
    // We don't check that this is a register-immediate BIC, because the
    // immediate field will be 0 in all other cases.
    uint32_t rhs =
        ImmediateRotateRight(inst.values.immediate, inst.values.shift * 2);

    bool conditions_match = (condition == kIgnoreCondition)
        || (inst.values.cond == condition);

    return rhs == kControlMask
        && conditions_match
        && inst.values.arg2 == reg;
  } else {
    return false;
  }
}
