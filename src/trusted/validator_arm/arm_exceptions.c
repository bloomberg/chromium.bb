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
