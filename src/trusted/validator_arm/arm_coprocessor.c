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
 * Define ARM coprocessor instructions.
 */

#include <string.h>

#include "native_client/src/trusted/validator_arm/arm_coprocessor.h"
#include "native_client/src/trusted/validator_arm/arm_insts.h"
#include "native_client/src/trusted/validator_arm/arm_inst_modeling.h"

/*
 * Model coprocessor instructions.
 */
static const ModeledOpInfo kCoprocessorOps[] = {
  { "cdp",
    ARM_CDP,
    kArmUndefinedAccessModeName,
    ARM_CP_F1,
    FALSE,
    "%C\t%3, %o, %X, %R, %Z, %i",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0xE, ANY, ANY, ANY, ANY, ANY, 0x0, ANY }},
  { "mcr",
    ARM_MCR,
    kArmUndefinedAccessModeName,
    ARM_CP_F4,
    FALSE,
    "%C\t%3, %o, %X, %r, %Z",
    ARM_WORD_LENGTH, NULLName,
    { CONTAINS_ZERO, 0xE, NON_ZERO, ANY, ANY, ANY, ANY, 0x1, 0x0 }},
  { "mcr",
    ARM_MCR,
    kArmUndefinedAccessModeName,
    ARM_CP_F4,
    FALSE,
    "%C\t%3, %o, %X, %r, %Z, %i",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0xE, NON_ZERO, ANY, ANY, ANY, ANY, 0x1, NON_ZERO }},
  { "mcrr",
    ARM_MCRR,
    kArmUndefinedAccessModeName,
    ARM_CP_F3,
    FALSE,
    "%C\t%3, %i, %x, %r, %Z",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0xC, 0x4, ANY, ANY, ANY, ANY, NOT_USED, ANY }},
  { "mrc",
    ARM_MRC,
    kArmUndefinedAccessModeName,
    ARM_CP_F4,
    FALSE,
    "%C\t%3, %x, %R, %Z",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0xE, ANY, ANY, ANY, ANY, ANY, 0x10001, 0x0 }},
  { "mrc",
    ARM_MRC,
    kArmUndefinedAccessModeName,
    ARM_CP_F4,
    FALSE,
    "%C\t%3, %x, %R, %Z, %i",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0xE, ANY, ANY, ANY, ANY, ANY, 0x10001, NON_ZERO }},
  { "mrrc",
    ARM_MRRC,
    kArmUndefinedAccessModeName,
    ARM_CP_F3,
    FALSE,
    "%C\t%3, %i, %x, %r, %Z",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0xC, 0x5, ANY, ANY, ANY, ANY, NOT_USED, ANY }},
  END_OPINFO_LIST
};

/*
 * Model parameters to Coprocessor load/store operations.
 */
typedef struct MetaCoprocessorLoadStoreOp {
  ModeledOpInfo pattern;          /* The pattern to use. */
  int32_t u_values;               /* Valid U bit values (0, 1, ANY). */
  int32_t n_values;               /* Valid N bit values (0, 1, ANY). */
} MetaCoprocessorLoadStoreOp;

static const MetaCoprocessorLoadStoreOp kCoprocessorLoadStores[] = {
  { { "",
      ARM_LDC,
      kArmLscImmediateOffsetName,
      ARM_CP_F2,
      FALSE,
      "%C%L\tp%3, %X, [%r, #%-%i*4]",
      ARM_WORD_LENGTH,
      NULLName,
      { CONTAINS_ZERO, 0xD, 0x0, ANY, ANY, ANY, NOT_USED, NOT_USED, ANY }},
    ANY, ANY },
  { { "",
      ARM_LDC,
      kArmLscImmediatePreIndexedName,
      ARM_CP_F2,
      FALSE,
      "%C%L\tp%3, %X, [%r, #%-%i*4]!",
      ARM_WORD_LENGTH,
      NULLName,
      { CONTAINS_ZERO, 0XD, 0x2, ANY, ANY, ANY, NOT_USED, NOT_USED, ANY }},
    ANY, ANY },
  { { "",
      ARM_LDC,
      kArmLscImmediatePostIndexedName,
      ARM_CP_F2,
      FALSE,
      "%C%L\tp%3, %X, [%r], #%-%i*4",
      ARM_WORD_LENGTH,
      NULLName,
      { CONTAINS_ZERO, 0xC, 0x2, ANY, ANY, ANY, NOT_USED, NOT_USED, ANY }},
    ANY, ANY },
  { { "",
      ARM_LDC,
      kArmLscUnindexedName,
      ARM_CP_F2,
      FALSE,
      "%C%L\tp%3, %X, [%r], {%i}",
      ARM_WORD_LENGTH,
      NULLName,
      { CONTAINS_ZERO, 0xC, 0x0, ANY, ANY, ANY, NOT_USED, NOT_USED, ANY }},
    1, ANY },
  /* Dummy end of list marker */
  { END_OPINFO_LIST , 0x0, 0x0 }

};

/*
 * Create instructions of the given coprocessor operation, based on the
 * possible N bit values (of the opcode), as specified by
 * the given pattern.
 */
static void AddNToCoprocessorOp(const MetaCoprocessorLoadStoreOp* pattern,
                                ModeledOpInfo* op) {
  switch (pattern->n_values) {
    case 0x0:
      AddInstruction(op);
      break;
    case 0x1:
      op->expected_values.opcode |= 0x4;
      AddInstruction(op);
      break;
    default:
      {
        ModeledOpInfo* op1 = CopyOfOpInfo(op);
        AddInstruction(op);
        op1->expected_values.opcode |= 0x4;
        AddInstruction(op1);
      }
      break;
  }
}

/* Create instructions of the given coprocessor operation, based on the
 * possible U and B bit values (of the opcode), as specified in the
 * given pattern.
 */
static void AddUNToCoprocessorOp(const MetaCoprocessorLoadStoreOp* pattern,
                                 ModeledOpInfo* op) {
  switch (pattern->u_values) {
    case 0x0:
      AddNToCoprocessorOp(pattern, op);
      break;
    case 0x1:
      op->expected_values.opcode |= 0x8;
      AddNToCoprocessorOp(pattern, op);
      break;
    default:
      {
        ModeledOpInfo* op1 = CopyOfOpInfo(op);
        AddNToCoprocessorOp(pattern, op);
        op1->expected_values.opcode |= 0x8;
        AddNToCoprocessorOp(pattern, op1);
      }
      break;
  }
}

/*
 * Create a copy of the given coprocessor instruction, setting the name
 * and opcode, based on the flag as_load.
 */
static ModeledOpInfo* CoprocessorOpLoadStoreCopy(const ModeledOpInfo* op,
                                                 Bool as_load) {
  ModeledOpInfo* copy_op = CopyOfOpInfo(op);
  if (as_load) {
    copy_op->name = "ldc";
    copy_op->inst_kind = ARM_LDC;
    copy_op->expected_values.opcode |= 0x1;
  } else {
    copy_op->name = "stc";
    copy_op->inst_kind = ARM_STC;
  }
  return copy_op;
}

/*
 * Given the pattern for coprocessor operations, and the flag defining
 * if the patterns correspond to a load or store, generate the corresponding
 * coprocessor instructions.
 */
static void AddCoprocessorOpKind(const MetaCoprocessorLoadStoreOp* pattern,
                                 Bool as_load) {
  char buffer[24];
  ModeledOpInfo* op = CoprocessorOpLoadStoreCopy(&(pattern->pattern), as_load);
  AddUNToCoprocessorOp(pattern, op);
  op = CoprocessorOpLoadStoreCopy(&(pattern->pattern), as_load);
  snprintf(buffer, sizeof(buffer), "%s2", op->name);
  op->name = strdup(buffer);
  op->expected_values.cond = 0xF;
  AddUNToCoprocessorOp(pattern, op);
}

/*
 * Build the set of coprocessor ARM instructions.
 */
void BuildCoprocessorOps() {
  int i;
  char buffer[24];
  for (i = 0; ; ++i) {
    ModeledOpInfo* op;
    const ModeledOpInfo* pattern = &(kCoprocessorOps[i]);
    if (ARM_INST_TYPE_SIZE == pattern->inst_type) break;
    op = CopyOfOpInfo(pattern);
    AddInstruction(op);
    op = CopyOfOpInfo(pattern);
    snprintf(buffer, sizeof(buffer), "%s2", op->name);
    op->name = strdup(buffer);
    op->expected_values.cond = 0xF;
    AddInstruction(op);
  }

  for (i = 0; ; ++i) {
    const MetaCoprocessorLoadStoreOp* pattern = &(kCoprocessorLoadStores[i]);
    if (ARM_INST_TYPE_SIZE == pattern->pattern.inst_type) break;
    AddCoprocessorOpKind(pattern, TRUE);
    AddCoprocessorOpKind(pattern, FALSE);
  }
}
