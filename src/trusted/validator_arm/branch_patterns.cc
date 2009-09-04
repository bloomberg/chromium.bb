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

#include "native_client/src/trusted/validator_arm/branch_patterns.h"
#include "native_client/src/trusted/validator_arm/arm_insts_rt.h"
#include "native_client/src/trusted/validator_arm/arm_validate.h"
#include "native_client/src/trusted/validator_arm/register_set_use.h"
#include "native_client/src/trusted/validator_arm/validator_patterns.h"

// The expected mask for safe indirect branches.
static const uint32_t kControlFlowMask = 0xF000000F;

// A function that recognizes a proper control flow BIC sequence.
static bool isControlFlowBic(const NcDecodeState &state) {
  if (state.CurrentInstructionIs(ARM_BIC)) {
    const NcDecodedInstruction &inst = state.CurrentInstruction();
    // We don't check that this is a register-immediate BIC, but that's okay:
    // the immediate field is 0 in other cases and will fail this check.
    uint32_t rhs = ImmediateRotateRight(inst.values.immediate,
        inst.values.shift * 2);
    return rhs == kControlFlowMask;
  } else {
    return false;
  }
}

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

    if (!pred.HasValidPc() || !isControlFlowBic(pred)) return false;

    const NcDecodedInstruction &branch = state.CurrentInstruction();
    const NcDecodedInstruction &mask = pred.CurrentInstruction();

    if (branch.values.cond == mask.values.cond
        && branch.values.arg4 == mask.values.arg2) {
      // Note: we don't care whether the BIC sets flags.  If it does, we may
      // mask without branching, which is basically a no-op.
      return true;
    }

    return false;
  }
};

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
        && !state.CurrentInstructionIs(ARM_BRANCH_RS)
        && !state.CurrentInstructionIs(ARM_BRANCH);
  }
  virtual bool IsSafe(const NcDecodeState &state) {
    return isControlFlowBic(state);
  }
};

void InstallBranchPatterns() {
  RegisterValidatorPattern(new SafeIndirectBranchPattern());
  RegisterValidatorPattern(new NonBranchPcUpdatePattern());
}
