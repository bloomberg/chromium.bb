/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Model stack adjustment patterns.
 */

#include "native_client/src/trusted/validator_arm/validator_patterns.h"
#include "native_client/src/trusted/validator_arm/arm_insts_rt.h"
#include "native_client/src/trusted/validator_arm/arm_validate.h"
#include "native_client/src/trusted/validator_arm/register_set_use.h"
#include "native_client/src/trusted/validator_arm/stack_adjust_patterns.h"

// Define the stack clear mask expected by stack adjustments.
static const uint32_t kSpClearBitMask = 0x80000000;

// Define pattern that recognizes an add/subtract on stack pointer
// followed by a corresponding bit clear.
class ValidatorStackAdjustPattern : public ValidatorPattern {
 public:
  ValidatorStackAdjustPattern();
  virtual ~ValidatorStackAdjustPattern() {}
  virtual bool MayBeUnsafe(const NcDecodeState &);
  virtual bool IsSafe(const NcDecodeState &);
  virtual void ReportUnsafeError(const NcDecodeState &);
};

ValidatorStackAdjustPattern::ValidatorStackAdjustPattern()
    : ValidatorPattern("stack adjustment", 2, 0) {}

/**
 * Checks whether the instruction pointed to in the given state matches the
 * form of an in-place SP mask:
 *    bic<cc> r13, r13, #0x80000000
 */
static bool isInPlaceStackMask(const NcDecodeState &state) {
  const NcDecodedInstruction& inst = state.CurrentInstruction();
  const InstValues& values = inst.values;
  uint32_t mask = ImmediateRotateRight(values.immediate, values.shift * 2);

  return state.CurrentInstructionIs(ARM_BIC) &&
      state.CurrentInstructionIs(ARM_DP_I) &&
      SP_INDEX == inst.values.arg1 &&
      inst.values.arg1 == inst.values.arg2 &&
      mask == kSpClearBitMask;
}

bool ValidatorStackAdjustPattern::MayBeUnsafe(const NcDecodeState &state) {
  const NcDecodedInstruction& inst = state.CurrentInstruction();
  // Check that sp modified, and not special mask instruction.
  if (!GetBit(RegisterSets(&inst), SP_INDEX) ||
      GetBit(RegisterSetIncremented(&inst), SP_INDEX)) {
    return false;
  }

  return !isInPlaceStackMask(state);
}

bool ValidatorStackAdjustPattern::IsSafe(const NcDecodeState &state) {
  // If this instruction is the mask sequence itself, it's always safe.
  if (isInPlaceStackMask(state)) {
    return true;
  }

  // Only safe if the second instruction in the sequence is
  // a bit clear on the expected range of (data) addresses.
  NcDecodeState next(state);
  next.NextInstruction();

  return isInPlaceStackMask(next) && state.ConditionMatches(next);
}

void ValidatorStackAdjustPattern::ReportUnsafeError(
    const NcDecodeState &state) {
  ValidateError(
      "Unsafe use of %s instruction (not followed by BIC on %08x):\n\t%s",
      _name.c_str(),
      kSpClearBitMask,
      InstructionLine(&(state.CurrentInstruction())).c_str());
}

void InstallStackAdjustPatterns() {
  RegisterValidatorPattern(new ValidatorStackAdjustPattern());
}
