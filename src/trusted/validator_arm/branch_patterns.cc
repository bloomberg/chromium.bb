/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Model control flow patterns.
 */

#include "native_client/src/trusted/validator_arm/branch_patterns.h"
#include "native_client/src/trusted/validator_arm/arm_insts_rt.h"
#include "native_client/src/trusted/validator_arm/arm_validate.h"
#include "native_client/src/trusted/validator_arm/register_set_use.h"
#include "native_client/src/trusted/validator_arm/validator_patterns.h"
#include "native_client/src/trusted/validator_arm/masks.h"

/*
 * Validator pattern for a safe mask-and-branch sequence.
 */
class SafeIndirectBranchPattern : public ValidatorPattern {
 public:
  SafeIndirectBranchPattern() : ValidatorPattern("indirect branch", 2, 1) {}
  virtual ~SafeIndirectBranchPattern() {}

  virtual bool MayBeUnsafe(const NcDecodeState &state) {
    return state.CurrentInstructionIs(ARM_BRANCH_RS);
  }
  virtual bool IsSafe(const NcDecodeState &state) {
    NcDecodeState pred(state);
    pred.PreviousInstruction();

    // Make sure we haven't walked out of the text segment.
    if (!pred.HasValidPc()) return false;

    const NcDecodedInstruction &branch = state.CurrentInstruction();

    return CheckControlMask(pred, branch.values.arg4, branch.values.cond);
  }
};

static bool isNaclHalt(const NcDecodeState &state) {
  if (state.CurrentInstructionIs(ARM_MOV)
      && state.CurrentInstructionIs(ARM_DP_I)) {
    const NcDecodedInstruction &inst = state.CurrentInstruction();
    // We don't need to process the shift field: 0 << x == 0.
    return inst.values.immediate == 0;
  } else {
    return false;
  }
}

/*
 * Validator pattern for non-branch writes to PC.
 */
class NonBranchPcUpdatePattern : public ValidatorPattern {
 public:
  NonBranchPcUpdatePattern()
      : ValidatorPattern("non-branch PC update", 1, 0) {}
  virtual ~NonBranchPcUpdatePattern() {}

  virtual bool MayBeUnsafe(const NcDecodeState &state) {
    const NcDecodedInstruction &inst = state.CurrentInstruction();
    return GetBit(RegisterSets(&inst), PC_INDEX)
        && !isNaclHalt(state)
        && !state.CurrentInstructionIs(ARM_BRANCH_RS)
        && !state.CurrentInstructionIs(ARM_BRANCH);
  }
  virtual bool IsSafe(const NcDecodeState &state) {
    return CheckControlMask(state, PC_INDEX, kIgnoreCondition);
  }
};

void InstallBranchPatterns() {
  RegisterValidatorPattern(new SafeIndirectBranchPattern());
  RegisterValidatorPattern(new NonBranchPcUpdatePattern());
}
