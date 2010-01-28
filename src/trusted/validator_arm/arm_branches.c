/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Defines ARM branches.
 */

#include "native_client/src/trusted/validator_arm/arm_branches.h"
#include "native_client/src/trusted/validator_arm/arm_insts.h"
#include "native_client/src/trusted/validator_arm/arm_inst_modeling.h"

/*
 * Model branching instructions.
 */
static const ModeledOpInfo kBranchOps[] = {
  { "b",
    ARM_BRANCH_INST,
    kArmUndefinedAccessModeName,
    ARM_BRANCH,
    TRUE,
    "%C\t%a",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO , 0x5, 0x0, NOT_USED, NOT_USED, NOT_USED,
      NOT_USED, NOT_USED, ANY }},

  { "bl",
    ARM_BRANCH_AND_LINK,
    kArmUndefinedAccessModeName,
    ARM_BRANCH,
    TRUE,
    "%C\t%a",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x5, 0x1, NOT_USED, NOT_USED, NOT_USED,
      NOT_USED, NOT_USED, ANY }},

  { "blx",
    ARM_BRANCH_WITH_LINK_AND_EXCHANGE_1,
    kArmUndefinedAccessModeName,
    ARM_BRANCH,
    FALSE,
    "%n\t%A",
    ARM_WORD_LENGTH,
    NULLName,
    /* TODO: special processing of H bit (opcode). */
    { 0xF, 0x5, ANY, NOT_USED, NOT_USED, NOT_USED, NOT_USED, NOT_USED, ANY }},

  /* TODO(kschimpf): Don't allow non-word alignment (ignore bit 0). */
  /* TODO(kschimpf): Don't allow R15. */
  { "blx",
    ARM_BRANCH_WITH_LINK_AND_EXCHANGE_2,
    kArmUndefinedAccessModeName,
    ARM_BRANCH_RS,
    TRUE,
    "%C\t%z",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x0, 0x12, 0xF, 0xF, 0xF, ANY, 0x3, NOT_USED }},

  /* TODO(kschimpf): How to handle bit[0]==1 of arg4 (register), which
     implies thumb branch. */
  { "bx",
    ARM_BRANCH_AND_EXCHANGE,
    kArmUndefinedAccessModeName,
    ARM_BRANCH_RS,
    TRUE,
    "%C\t%z",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x0, 0x12, 0xF, 0xF, 0xF, ANY, 0x1, NOT_USED }},

  /* TODO(kschimpf): Don't allow non-word alignment (ignore bit 0). */
  /* TODO(kschimpf): same as BX except if Jazelle is available and enabled. */
  /* otherwise acts as BX. */
  { "bxj",
    ARM_BRANCH_AND_EXCHANGE_TO_JAZELLE,
    kArmUndefinedAccessModeName,
    ARM_BRANCH_RS,
    FALSE,
    "%C\t%z",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x0, 0x12, 0xF, 0xF, 0xF, ANY, 0x2, NOT_USED }},

  END_OPINFO_LIST
};

void BuildArmBranchInstructions() {
  int i;
  for (i = 0; ; ++i) {
    const ModeledOpInfo* op = &(kBranchOps[i]);
    if (ARM_INST_TYPE_SIZE == op->inst_type) break;
    AddInstruction(op);
  }
}
