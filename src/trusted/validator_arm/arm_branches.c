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
    FALSE,
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
