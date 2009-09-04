/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
