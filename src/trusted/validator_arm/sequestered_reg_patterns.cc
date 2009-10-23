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
