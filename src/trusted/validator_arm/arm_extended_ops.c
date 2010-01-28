/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Define ARM extended operations.
 */

#include <string.h>

#include "native_client/src/trusted/validator_arm/arm_parallel.h"
#include "native_client/src/trusted/validator_arm/arm_insts.h"
#include "native_client/src/trusted/validator_arm/arm_inst_modeling.h"
#include "native_client/src/shared/utils/formatting.h"

/*
 * Model the extend operation specific data that needs to be provided,
 * in addition to the kExtendPattern default choices.
 */
typedef struct MetaExtendOp {
  const char* name;         /* The name of the extend operation. */
  ArmInstKind kind;         /* The kind of extend instruction. */
  int32_t opcode;           /* The opcode (with the S bit) of the operation.*/
  Bool keep_arg1;           /* Uses arg1 in the operation. */
} MetaExtendOp;

/*
 * Model Extend operation parameters.
 */
static const MetaExtendOp kMetaExtendOps[] = {
  { "sxtab16" , ARM_SXTAB16, 0x8 , TRUE },
  { "sxtab" ,   ARM_SXTAB,   0xA , TRUE },
  { "sxtah" ,   ARM_SXTAH,   0xB , TRUE },
  { "sxtb16",   ARM_SXTB16,  0x8 , FALSE },
  { "sxtb",     ARM_SXTB,    0xA , FALSE },
  { "sxth",     ARM_SXTH,    0xB , FALSE },
  { "uxtab16",  ARM_UXTAB16, 0xC , TRUE },
  { "uxtab",    ARM_UXTAB,   0xE , TRUE },
  { "uxtah",    ARM_UXTAH,   0xF , TRUE },
  { "uxtb16",   ARM_UXTB16,  0xC , FALSE },
  { "uxtb",     ARM_UXTB,    0xE , FALSE },
  { "uxth",     ARM_UXTH,    0xF , FALSE },
  /* Dummy field to end list. */
  { NULL , ARM_UNKNOWN_INST, 0x0 , FALSE }
};

/* Define the patterns to add for each meta extend operation. */
static const ModeledOpInfo kExtendPattern[] = {
  { "",
    ARM_UNKNOWN_INST,
    kArmUndefinedAccessModeName,
    ARM_DP_RE, TRUE,
    "%C\t%K, %z",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x3, 0xA, CONTAINS_ZERO, CONTAINS_ZERO,
      0x0, CONTAINS_ZERO, 0x0, 0x7 }},
  { "",
    ARM_UNKNOWN_INST,
    kArmUndefinedAccessModeName,
    ARM_DP_RE,
    TRUE,
    "%C\t%K, %z, ror #8",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x3, 0xA, CONTAINS_ZERO, CONTAINS_ZERO, 0x1,
      CONTAINS_ZERO, 0x0, 0x7 }},
  { "",
    ARM_UNKNOWN_INST,
    kArmUndefinedAccessModeName,
    ARM_DP_RE,
    TRUE,
    "%C\t%K, %z, ror #16",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x3, 0xA, CONTAINS_ZERO, CONTAINS_ZERO, 0x2,
      CONTAINS_ZERO, 0x0, 0x7 }},
  { "",
    ARM_UNKNOWN_INST,
    kArmUndefinedAccessModeName,
    ARM_DP_RE,
    TRUE,
    "%C\t%K, %z, ror #24",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x3, 0xA, CONTAINS_ZERO, CONTAINS_ZERO, 0x3,
      CONTAINS_ZERO, 0x0, 0x7 }},
  END_OPINFO_LIST
};

/*
 * Fixes the format direction of extend operations, by updating the
 * %K directive with the corresponding correct format pattern. That is,
 * %K => '%x, %r' if keep arg1, and '%x' otherwise.
 */
static Bool FixMetaExtendOpDirective(char directive,
                                     char* buffer,
                                     size_t buffer_size,
                                     void* data,
                                     size_t* cursor) {
  MetaExtendOp* op = (MetaExtendOp*)data;
  switch (directive) {
    case 'K':
      FormatAppend(buffer,
                   buffer_size,
                   (op->keep_arg1 ? "%x, %r" : "%x"),
                   cursor);
      return TRUE;
    default:
      return FALSE;
  }
}

/*
 * Updates the (data operation) format by updating the %K directive (based on
 * the value of the given (meta) data operation parameters), and puts the
 * result into the specified buffer. Returns true if buffer overflow
 * doesn't occur.
 */
static Bool FixMetaExtendOpFormat(char* buffer,
                                  size_t buffer_size,
                                  const char* format,
                                  const MetaExtendOp* op) {
  return FormatData(buffer,
                    buffer_size,
                    format,
                    (void*) op,
                    FixMetaExtendOpDirective);
}

void BuildExtendedOps() {
  int i;
  for (i = 0; ; ++i) {
    int j;
    char buffer[INST_BUFFER_SIZE];
    const MetaExtendOp* meta_op = &(kMetaExtendOps[i]);
    if (NULL == meta_op->name) break;
    for (j = 0; ; ++j) {
      ModeledOpInfo* op;
      const ModeledOpInfo* pattern = &(kExtendPattern[j]);
      if (ARM_INST_TYPE_SIZE == pattern->inst_type) break;
      op = CopyOfOpInfo(pattern);
      op->name = meta_op->name;
      op->inst_kind = meta_op->kind;
      op->expected_values.opcode = meta_op->opcode;
      FixMetaExtendOpFormat(buffer, sizeof(buffer),
                            pattern->describe_format, meta_op);
      op->describe_format = strdup(buffer);
      if (!meta_op->keep_arg1) {
        op->expected_values.arg1 = 0xF;
      }
      AddInstruction(op);
    }
  }
}
