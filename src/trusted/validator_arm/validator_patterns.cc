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
 * Model validation paterns. Used to validate Arm code meets
 * Native Client Rules.
 */

#include <stdio.h>
#include "native_client/src/trusted/validator_arm/validator_patterns.h"
#include "native_client/src/trusted/validator_arm/arm_validate.h"

ValidatorPattern::~ValidatorPattern() {}

uint32_t ValidatorPattern::StartPc(const NcDecodeState &state) const {
  uint32_t instructions_before = _reporting_index;
  return state.CurrentPc() - (instructions_before * ARM_WORD_LENGTH);
}

uint32_t ValidatorPattern::EndPc(const NcDecodeState &state) const {
  return StartPc(state) + (_length * ARM_WORD_LENGTH);
}

void ValidatorPattern::ReportUnsafeError(const NcDecodeState &state) {
  ValidateError("Unsafe use of %s instruction:\n\t%s",
                _name.c_str(),
                InstructionLine(&(state.CurrentInstruction())).c_str());
}

void ValidatorPattern::ReportUnsafeBranchTo(const NcDecodeState &state,
                                            uint32_t address) {
  ValidateError("Unsafe branch into %s pattern (at %x):\n\t%s",
                _name.c_str(),
                address,
                InstructionLine(&(state.CurrentInstruction())).c_str());
}

void ValidatorPattern::ReportCrossesBlockBoundary(const NcDecodeState &state) {
  ValidateError("The %s pattern crosses block boundaries:\n\t%s",
                _name.c_str(),
                InstructionLine(&(state.CurrentInstruction())).c_str());
}

static std::vector<ValidatorPattern*>* registered_patterns = NULL;

inline std::vector<ValidatorPattern*>* GetRegisteredPatterns() {
  if (NULL == registered_patterns) {
    registered_patterns = new std::vector<ValidatorPattern*>();
  }
  return registered_patterns;
}

void RegisterValidatorPattern(ValidatorPattern* pattern) {
  GetRegisteredPatterns()->push_back(pattern);
}

std::vector<ValidatorPattern*>::iterator RegisteredValidatorPatternsBegin() {
  return GetRegisteredPatterns()->begin();
}

std::vector<ValidatorPattern*>::iterator RegisteredValidatorPatternsEnd() {
  return GetRegisteredPatterns()->end();
}
