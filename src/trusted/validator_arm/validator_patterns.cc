/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
