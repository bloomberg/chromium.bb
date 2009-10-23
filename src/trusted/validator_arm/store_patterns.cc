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
 * Model control flow patterns.
 */

#include "native_client/src/trusted/validator_arm/store_patterns.h"
#include "native_client/src/trusted/validator_arm/arm_insts_rt.h"
#include "native_client/src/trusted/validator_arm/arm_validate.h"
#include "native_client/src/trusted/validator_arm/register_set_use.h"
#include "native_client/src/trusted/validator_arm/validator_patterns.h"
#include "native_client/src/trusted/validator_arm/masks.h"

// Recognizes a store instruction.
static bool isStore(const NcDecodeState &state) {
  return state.CurrentInstructionIs(ARM_STR)
      || state.CurrentInstructionIs(ARM_STREX)
      || state.CurrentInstructionIs(ARM_STR_HALFWORD)
      || state.CurrentInstructionIs(ARM_STR_DOUBLEWORD)
      || state.CurrentInstructionIs(ARM_STM_1)
      || state.CurrentInstructionIs(ARM_STM_1_MODIFY)
      || state.CurrentInstructionIs(ARM_STM_2);
  // TODO(cbiffle): our instruction definitions don't match the ARMv7
  // docs, making it difficult to tell whether this is comprehensive!
  // The instruction model should model memory writes!
}

/*
 * Validator pattern that recognizes and rejects stores that generate an
 * effective address by adding two registers.
 *
 * This is kind of an odd use for a validator pattern: normally we would just
 * mark these instructions as unsafe in the decode tables.  However, the
 * decoder currently derives its representation of stores from its
 * representation of loads -- so we can't kill the offending stores without
 * collateral damage.
 */
class RegisterOffsetStorePattern : public ValidatorPattern {
 public:
  RegisterOffsetStorePattern()
      : ValidatorPattern("register-register store", 1, 0) {}
  virtual ~RegisterOffsetStorePattern() {}

  virtual bool MayBeUnsafe(const NcDecodeState &state) {
    // We fire on all stores that use a Register Offset (RO).
    if (state.CurrentInstructionIs(ARM_LS_RO)) {
      return isStore(state);
    } else {
      return false;
    }
  }

  virtual bool IsSafe(const NcDecodeState &state) {
    UNREFERENCED_PARAMETER(state);

    // A tad roundabout, yes.
    return false;
  }
};

/*
 * Validator pattern for a safe mask-and-store sequence.
 */
class SafeStorePattern : public ValidatorPattern {
 public:
  SafeStorePattern() : ValidatorPattern("store without address mask", 2, 1) {}
  virtual ~SafeStorePattern() {}

  virtual bool MayBeUnsafe(const NcDecodeState &state) {
    /*
     * We consider only Immediate-Offset and Multiple Stores.  (Register-offset
     * is handled by the pattern above.)
     */
    if (state.CurrentInstructionIs(ARM_LS_IO)
        || state.CurrentInstructionIs(ARM_LS_MULT)) {
      if (!isStore(state)) return false;

      const NcDecodedInstruction &store = state.CurrentInstruction();

      // We exempt stores relative to SP from this analysis.
      if (store.values.arg1 == SP_INDEX) {
        return false;
      }

      return true;
    } else {
      return false;
    }
  }

  virtual bool IsSafe(const NcDecodeState &state) {
    NcDecodeState pred(state);
    pred.PreviousInstruction();

    // Ensure that we haven't walked out of the text region.
    if (!pred.HasValidPc()) return false;

    const NcDecodedInstruction &store = state.CurrentInstruction();
    return CheckDataMask(pred, store.values.arg1, store.values.cond);
  }
};

void InstallStorePatterns() {
  RegisterValidatorPattern(new SafeStorePattern());
  RegisterValidatorPattern(new RegisterOffsetStorePattern());
}
