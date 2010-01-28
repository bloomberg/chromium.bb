/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Define ARM exception generating instructions.
 */

#include "native_client/src/trusted/validator_arm/arm_exceptions.h"
#include "native_client/src/trusted/validator_arm/arm_insts.h"
#include "native_client/src/trusted/validator_arm/arm_inst_modeling.h"

/*
 * Model exception-generating instructions
 */
static const ModeledOpInfo kExceptionGenerating[] = {
  { "bkpt",
    ARM_BKPT,
    kArmUndefinedAccessModeName,
    ARM_DP_RS,
    FALSE,
    "%n\t%b",
    ARM_WORD_LENGTH,
    NULLName,
    { 0xE, 0x0, 0x12, ANY, ANY, ANY, ANY, 0x3, 0x1 }},
  { "swi",
    ARM_SWI,
    kArmUndefinedAccessModeName,
    ARM_BRANCH,
    FALSE,
    "%C\t%i",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x7, 0x1, NOT_USED, NOT_USED, NOT_USED,
      NOT_USED, NOT_USED, ANY }},
  END_OPINFO_LIST
};

void BuildExceptionGenerating() {
  int i;
  for (i = 0; ; ++i) {
    const ModeledOpInfo* op = &(kExceptionGenerating[i]);
    if (ARM_INST_TYPE_SIZE == op->inst_type) break;
    AddInstruction(op);
  }
}
