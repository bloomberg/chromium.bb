/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Patterns for analyzing use of sequestered registers.
 */

#include "native_client/src/trusted/validator_arm/sequestered_reg_patterns.h"
#include "native_client/src/trusted/validator_arm/arm_insts_rt.h"
#include "native_client/src/trusted/validator_arm/arm_validate.h"
#include "native_client/src/trusted/validator_arm/register_set_use.h"
#include "native_client/src/trusted/validator_arm/validator_patterns.h"
#include "native_client/src/trusted/validator_arm/masks.h"

/*
 * Validator pattern that recognizes and rejects any untrusted instruction that
 * attempts to alter r9, the thread state register.
 *
 * TODO(cbiffle): single-instruction validator patterns that only say "NO" are
 * a real stretch of the model.
 */
class UpdateR9Pattern : public ValidatorPattern {
 public:
  UpdateR9Pattern() : ValidatorPattern("update to r9", 1, 0) {}
  virtual ~UpdateR9Pattern() {}

  virtual bool MayBeUnsafe(const NcDecodeState &state) {
    return GetBit(RegisterSets(&state.CurrentInstruction()), 9);
  }

  virtual bool IsSafe(const NcDecodeState &state) {
    UNREFERENCED_PARAMETER(state);
    return false;
  }
};

void InstallSequesteredRegisterPatterns() {
  RegisterValidatorPattern(new UpdateR9Pattern());
}
