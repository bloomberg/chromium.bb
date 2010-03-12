/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Model pattern(s) to recognize nops, as well as histogramming other
 * useful instructions.
 */

#include <set>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/validator_arm/nop_patterns.h"
#include "native_client/src/trusted/validator_arm/arm_insts_rt.h"
#include "native_client/src/trusted/validator_arm/arm_validate.h"
#include "native_client/src/trusted/validator_arm/register_set_use.h"
#include "native_client/src/trusted/validator_arm/validator_patterns.h"
#include "native_client/src/shared/utils/flags.h"

Bool FLAGS_show_counted_instructions = FALSE;

// A ValidatorPattern that is only unsafe if the flag above is true.
class CountedValidatorPattern : public ValidatorPattern {
 public:
  CountedValidatorPattern(const nacl::string &name, int length, int index)
      : ValidatorPattern(name, length, index) {}

  virtual bool IsSafe(const NcDecodeState &state) {
    UNREFERENCED_PARAMETER(state);
    return !FLAGS_show_counted_instructions;
  }
};

// Class to count nops.
class NopPattern : public CountedValidatorPattern {
 public:
  NopPattern()
      : CountedValidatorPattern("nop", 1, 0) {}

  // Recognizes instructions that should be treated as no-operations.
  // TODO(cbiffle): note that this doesn't recognize actual NOPs in ARMv7
  virtual bool MayBeUnsafe(const NcDecodeState &state) {
    if (state.CurrentInstructionIs(ARM_MOV)) {
      return !GetBit(state.CurrentInstruction().inst, ARM_S_BIT) &&
          &kArmDataRegister ==
          state.CurrentInstruction().matched_inst->inst_access &&
          state.CurrentInstruction().values.arg2 ==
          state.CurrentInstruction().values.arg4;
    }
    return false;
  }
};

// Class to count instructions.
class CountPattern : public ValidatorPattern {
 public:
  CountPattern() : ValidatorPattern("instructions", 1, 0) {}

  virtual bool MayBeUnsafe(const NcDecodeState &state) {
    UNREFERENCED_PARAMETER(state);
    return true;
  }

  virtual bool IsSafe(const NcDecodeState &state) {
    UNREFERENCED_PARAMETER(state);
    return true;
  }
};

// Class to count loads.
class CountLoadPattern : public CountedValidatorPattern {
 public:
  CountLoadPattern() : CountedValidatorPattern("loads", 1, 0) {}

  virtual bool MayBeUnsafe(const NcDecodeState &state) {
    const NcDecodedInstruction& inst = state.CurrentInstruction();
    const OpInfo* matched_inst = inst.matched_inst;
    switch (matched_inst->inst_kind) {
      case ARM_LDR:
      case ARM_LDREX:
      case ARM_LDR_DOUBLEWORD:
      case ARM_LDR_UNSIGNED_HALFWORD:
      case ARM_LDR_SIGNED_BYTE:
      case ARM_LDR_SIGNED_HALFWORD:
      case ARM_LDM_1:
      case ARM_LDM_1_MODIFY:
      case ARM_LDM_2:
      case ARM_LDM_3:
      case ARM_LDC:
        return true;
      default:
        return false;
    }
  }
};

// Class to count stores.
class CountStorePattern : public CountedValidatorPattern {
 public:
  CountStorePattern() : CountedValidatorPattern("stores", 1, 0) {}

  virtual bool MayBeUnsafe(const NcDecodeState &state) {
    switch (state.CurrentInstruction().matched_inst->inst_kind) {
      case ARM_STR:
      case ARM_STREX:
      case ARM_STR_DOUBLEWORD:
      case ARM_STR_HALFWORD:
      case ARM_STM_1:
      case ARM_STM_1_MODIFY:
      case ARM_STM_2:
      case ARM_STC:
        return true;
      default:
        return false;
    }
  }
};

// Class to count stack indexed stores.
class CountStackIndexedStorePattern : public CountedValidatorPattern {
 public:
  CountStackIndexedStorePattern()
      : CountedValidatorPattern("stack indexed stores", 1, 0) {}

  virtual bool MayBeUnsafe(const NcDecodeState &state) {
    switch (state.CurrentInstruction().matched_inst->inst_kind) {
      case ARM_STM_1:
      case ARM_STM_1_MODIFY:
      case ARM_STM_2:
        return state.CurrentInstruction().values.arg1 == SP_INDEX;
      case ARM_STR:
      case ARM_STR_DOUBLEWORD:
      case ARM_STR_HALFWORD:
        return IsStackRelativeUpdate(state);
      default:
        return false;
    }
  }

 private:
  bool IsStackRelativeUpdate(const NcDecodeState &state) {
    if (state.CurrentInstruction().values.arg1 == SP_INDEX) {
      const ArmAccessMode* access =
          state.CurrentInstruction().matched_inst->inst_access;
      return access == &kArmLsImmediateOffset ||
          access == &kArmLsImmediatePreIndexed ||
          access == &kArmLsImmediatePostIndexed ||
          access == &kArmMlsImmediateOffset ||
          access == &kArmMlsImmediatePreIndexed ||
          access == &kArmMlsImmediatePostIndexed;
    } else {
      return false;
    }
  }
};

// Class to count branches.
class CountBranchPattern : public CountedValidatorPattern {
 public:
  CountBranchPattern() : CountedValidatorPattern("branches", 1, 0) {}

  virtual bool MayBeUnsafe(const NcDecodeState &state) {
    switch (state.CurrentInstruction().matched_inst->inst_kind) {
      case ARM_BRANCH_INST:
      case ARM_BRANCH_AND_LINK:
      case ARM_BRANCH_WITH_LINK_AND_EXCHANGE_1:
      case ARM_BRANCH_WITH_LINK_AND_EXCHANGE_2:
      case ARM_BRANCH_AND_EXCHANGE:
      case ARM_BRANCH_AND_EXCHANGE_TO_JAZELLE:
        return true;
      default:
        return GetBit(RegisterSets(&(state.CurrentInstruction())), PC_INDEX);
    }
  }
};

// Class to count indirect branches.
class CountIndirectBranchPattern : public CountedValidatorPattern {
 public:
  CountIndirectBranchPattern()
      : CountedValidatorPattern("indirect branches", 1, 0) {}

  virtual bool MayBeUnsafe(const NcDecodeState &state) {
    switch (state.CurrentInstruction().matched_inst->inst_kind) {
      case ARM_BRANCH_WITH_LINK_AND_EXCHANGE_2:
      case ARM_BRANCH_AND_EXCHANGE:
      case ARM_BRANCH_AND_EXCHANGE_TO_JAZELLE:
        return true;
      case ARM_BRANCH_INST:
      case ARM_BRANCH_AND_LINK:
      case ARM_BRANCH_WITH_LINK_AND_EXCHANGE_1:
        return false;
      default:
        return GetBit(RegisterSets(&(state.CurrentInstruction())), PC_INDEX);
    }
  }
};

// Class to count branch and link (i.e. call) to get an estimate of the
// number of returns
class CountCallPattern : public CountedValidatorPattern {
 public:
  CountCallPattern()
      : CountedValidatorPattern("bl(x) calls", 1, 0) {}

  virtual bool MayBeUnsafe(const NcDecodeState &state) {
    switch (state.CurrentInstruction().matched_inst->inst_kind) {
      case ARM_BRANCH_AND_LINK:
      case ARM_BRANCH_WITH_LINK_AND_EXCHANGE_1:
      case ARM_BRANCH_WITH_LINK_AND_EXCHANGE_2:
        return true;
      default:
        return false;
    }
  }
};

// Class to count the number of return (i.e. bx lr) instructions.
class CountReturnPattern : public CountedValidatorPattern {
 public:
  CountReturnPattern() : CountedValidatorPattern("bx lr returns", 1, 0) {}

  virtual bool MayBeUnsafe(const NcDecodeState &state) {
    switch (state.CurrentInstruction().matched_inst->inst_kind) {
      case ARM_BRANCH_AND_EXCHANGE:
        return GetBit(RegisterUses(&(state.CurrentInstruction())), LINK_INDEX);
      default:
        return false;
    }
  }
};

// Class to count unique (static) call sites.
class CountUniqueStaticCallSitesPattern : public CountedValidatorPattern {
 public:
  CountUniqueStaticCallSitesPattern()
      : CountedValidatorPattern("static called sites", 1, 0),
        call_sites_()
  {}

  virtual bool MayBeUnsafe(const NcDecodeState &state) {
    switch (state.CurrentInstruction().matched_inst->inst_kind) {
      case ARM_BRANCH_AND_LINK:
      case ARM_BRANCH_WITH_LINK_AND_EXCHANGE_1:
        return call_sites_.insert(
            RealAddress(state.CurrentPc(),
                        state.CurrentInstruction().values.immediate)).second;
      default:
        return false;
    }
  }
 private:
  std::set<uint32_t> call_sites_;
};


void InstallInstructionCounterPatterns() {
  RegisterValidatorPattern(
      new NopPattern());
  RegisterValidatorPattern(
      new CountPattern());
  RegisterValidatorPattern(
      new CountLoadPattern());
  RegisterValidatorPattern(
      new CountStorePattern());
  RegisterValidatorPattern(
      new CountBranchPattern());
  RegisterValidatorPattern(
      new CountIndirectBranchPattern());
  RegisterValidatorPattern(
      new CountCallPattern());
  RegisterValidatorPattern(
      new CountReturnPattern());
  RegisterValidatorPattern(
      new CountUniqueStaticCallSitesPattern());
  RegisterValidatorPattern(
      new CountStackIndexedStorePattern());
}
