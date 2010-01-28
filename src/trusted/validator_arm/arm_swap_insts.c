/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Define ARM swap instructions.
 */

#include "native_client/src/trusted/validator_arm/arm_swap_insts.h"
#include "native_client/src/trusted/validator_arm/arm_insts.h"
#include "native_client/src/trusted/validator_arm/arm_inst_modeling.h"

/*
 * Model deprecated swap instructions (use ldrex or strex).
 */
static const ModeledOpInfo kSwapInstructions[] = {
  { "swp",
    ARM_SWP,
    kArmUndefinedAccessModeName,
    ARM_DP_RS,
    TRUE,
    "%C\t%x, %z, [%r]",
    ARM_WORD_LENGTH, NULLName,
    { CONTAINS_ZERO, 0x0, 0x10, ANY, ANY, 0x0, ANY, 0x0, 0x9 }},
  { "swp",
    ARM_SWPB,
    kArmUndefinedAccessModeName,
    ARM_DP_RS,
    TRUE,
    "%Cb\t%x, %z, [%r]",
    ARM_WORD_LENGTH, NULLName,
    { CONTAINS_ZERO, 0x0, 0x14, ANY, ANY, 0x0, ANY, 0x0, 0x9 }},
  END_OPINFO_LIST
};

void BuildSwapInstructions() {
  int i;
  for (i = 0; ; ++i) {
    const ModeledOpInfo* op = &(kSwapInstructions[i]);
    if (ARM_INST_TYPE_SIZE == op->inst_type) break;
    AddInstruction(op);
  }
}
