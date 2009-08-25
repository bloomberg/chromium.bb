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
