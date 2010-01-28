/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Defines ARM data operation instructions.
 */

#include <string.h>

#include "native_client/src/trusted/validator_arm/arm_data_ops.h"
#include "native_client/src/trusted/validator_arm/arm_insts.h"
#include "native_client/src/trusted/validator_arm/arm_inst_modeling.h"
#include "native_client/src/shared/utils/formatting.h"

/* Defines if we should keep arg1 and arg2 of the instruction. */
typedef enum MetaDataArgs {
  KEEP_DATA_ALL,
  KEEP_DATA_ARG1,
  KEEP_DATA_ARG2
} MetaDataArgs;

/*
 * Model data operation specific data that needs to decide
 * how the kDataOpPatterns apply.
 */
typedef struct MetaDataOp {
  const char* name;         /* The name of the data operation. */
  ArmInstKind kind;         /* The immediate form of the data op kind. */
  int32_t opcode;           /* The opcode (minus the S bit) of the operation.*/
  int32_t s_bit;            /* either explicit encoding of S bit, or ANY. */
  MetaDataArgs keep_args;   /* Defines how arg1 and arg2 are handled. */
} MetaDataOp;

/*
 * Model a meta op where specific values for the s bit have been set.
 */
typedef struct MetaDataOpData {
  const MetaDataOp* meta_op;
  int32_t s_value;
} MetaDataOpData;

/*
 * Model Data Operations parameters.
 */
static const MetaDataOp kMetaDataOps[] = {
  { "and", ARM_AND, 0x0, ANY, KEEP_DATA_ALL },
  { "eor", ARM_EOR, 0x1, ANY, KEEP_DATA_ALL },
  { "sub", ARM_SUB, 0x2, ANY, KEEP_DATA_ALL },
  { "rsb", ARM_RSB, 0x3, ANY, KEEP_DATA_ALL },
  { "add", ARM_ADD, 0x4, ANY, KEEP_DATA_ALL },
  { "adc", ARM_ADC, 0x5, ANY, KEEP_DATA_ALL },
  { "sbc", ARM_SBC, 0x6, ANY, KEEP_DATA_ALL },
  { "rsc", ARM_RSC, 0x7, ANY, KEEP_DATA_ALL },
  { "tst", ARM_TST, 0x8, 0x1, KEEP_DATA_ARG1 },
  { "teq", ARM_TEQ, 0x9, 0x1, KEEP_DATA_ARG1 },
  { "cmp", ARM_CMP, 0xA, 0x1, KEEP_DATA_ARG1 },
  { "cmn", ARM_CMN, 0xB, 0x1, KEEP_DATA_ARG1 },
  { "orr", ARM_ORR, 0xC, ANY, KEEP_DATA_ALL },
  { "mov", ARM_MOV, 0xD, ANY, KEEP_DATA_ARG2 },
  { "bic", ARM_BIC, 0xE, ANY, KEEP_DATA_ALL },
  { "mvn", ARM_MVN, 0xF, ANY, KEEP_DATA_ARG2 },
  /* Dummy field to end list. */
  { NULL , ARM_UNKNOWN_INST, -1, ANY, KEEP_DATA_ALL }
};

/*
 * Defines patterns for each possible type of data operation.
 */
static const ModeledOpInfo kDataOpPatterns[] = {
  /* NOTE: opcode field should be based on the
   * pattern for the AND operation (i.e. 0x0).
   */
  { "",
    ARM_AND,
    kArmDataImmediateName,
    ARM_DP_I,
    TRUE,
    "%C%d\t%K, #%I",
    ARM_WORD_LENGTH,
    NULLName,                                             /* 1 */
    { CONTAINS_ZERO, 0x1, 0x0, ANY, ANY, NOT_USED, NOT_USED, ANY, ANY }},
  { "",
    ARM_AND,
    kArmDataRegisterName,
    ARM_DP_IS,
    TRUE,
    "%C%d\t%K, %z",
    ARM_WORD_LENGTH,
    NULLName,                                             /* 2 */
    { CONTAINS_ZERO, 0x0, 0x0, ANY, ANY, 0x0, ANY, 0x0, 0x0 }},
  { "",
    ARM_AND,
    kArmDataLslImmediateName,
    ARM_DP_IS,
    TRUE,
    "%C%d\t%K, %z, lsl #%3",
    ARM_WORD_LENGTH,
    NULLName,                                             /* 3 */
    { CONTAINS_ZERO, 0x0, 0x0, ANY, ANY, NON_ZERO, ANY, 0x0, 0x0 }},
  { "",
    ARM_AND,
    kArmDataLslRegisterName,
    ARM_DP_RS,
    TRUE,
    "%C%d\t%K, %z, lsl %y",
    ARM_WORD_LENGTH,
    NULLName,                                             /* 4 */
    { CONTAINS_ZERO, 0x0, 0x0, CONTAINS_ZERO, CONTAINS_ZERO, CONTAINS_ZERO,
      CONTAINS_ZERO, 0x0, 0x1 }},
  { "",
    ARM_AND,
    kArmDataLsrImmediateName,
    ARM_DP_IS,
    TRUE,
    "%C%d\t%K, %z, lsr #%3",
    ARM_WORD_LENGTH,
    NULLName,                                             /* 5 */
    { CONTAINS_ZERO, 0x0, 0x0, ANY, ANY, ANY, ANY, 0x1, 0x0 }},
  { "",
    ARM_AND,
    kArmDataLsrRegisterName,
    ARM_DP_RS,
    TRUE,
    "%C%d\t%K, %z, lsr %y",
    ARM_WORD_LENGTH,
    NULLName,                                             /* 6 */
    { CONTAINS_ZERO, 0x0, 0x0, CONTAINS_ZERO, CONTAINS_ZERO, CONTAINS_ZERO,
      CONTAINS_ZERO, 0x1, 0x1 }},
  { "",
    ARM_AND,
    kArmDataAsrImmediateName,
    ARM_DP_IS,
    TRUE,
    "%C%d\t%K, %z, asr #%3",
    ARM_WORD_LENGTH,
    NULLName,                                             /* 7 */
    { CONTAINS_ZERO, 0x0, 0x0, ANY, ANY, ANY, ANY, 0x2, 0x0 }},
  { "",
    ARM_AND,
    kArmDataAsrRegisterName,
    ARM_DP_RS,
    TRUE,
    "%C%d\t%K, %z, asr %y",
    ARM_WORD_LENGTH,
    NULLName,                                           /* 8 */
    { CONTAINS_ZERO, 0x0, 0x0, CONTAINS_ZERO, CONTAINS_ZERO, CONTAINS_ZERO,
      CONTAINS_ZERO, 0x2, 0x1 }},
  { "",
    ARM_AND,
    kArmDataRorImmediateName,
    ARM_DP_IS,
    TRUE,
    "%C%d\t%K, %z, ror #%3",
    ARM_WORD_LENGTH,
    NULLName,                                             /* 9 */
    { CONTAINS_ZERO, 0x0, 0x0, ANY, ANY, NON_ZERO, ANY, 0x3, 0x0 }},
  { "",
    ARM_AND,
    kArmDataRorRegisterName,
    ARM_DP_RS, TRUE,
    "%C%d\t%K, %z, ror %y",
    ARM_WORD_LENGTH,
    NULLName,                                            /* 10 */
    { CONTAINS_ZERO, 0x0, 0x0, CONTAINS_ZERO, CONTAINS_ZERO, CONTAINS_ZERO,
      CONTAINS_ZERO, 0x3, 0x1 }},
  { "",
    ARM_AND,
    kArmDataRrxName,
    ARM_DP_RS,
    TRUE,
    "%C%d\t%K, %z, rrx",
    ARM_WORD_LENGTH,
    NULLName,                                            /* 11 */
    { CONTAINS_ZERO, 0x0, 0x0, ANY, ANY, 0x0, ANY, 0x3, 0x0 }},
  END_OPINFO_LIST
};

/*
 * Fixes the format directive of data operations, by updating the
 * %K directive with the corresponding correct format pattern. That is,
 * %K => '%x, %r' if keep arg1, and '%r' otherwise.
 * %d => '%d' if S bit is can be ANY (vs a specific value, which is assume
 *        to imply that the S bit is predefined by the instruction, rather
 *        than optionally set by the s instruction suffix).
 */
static Bool FixMetaDataOpDirective(char directive,
                                   char* buffer,
                                   size_t buffer_size,
                                   void* data,
                                   size_t* cursor) {
  char* subst_text;
  MetaDataOpData* op = (MetaDataOpData*)data;
  switch (directive) {
    case 'K':
      switch (op->meta_op->keep_args) {
        default:
          subst_text = "%x, %r";
          break;
        case KEEP_DATA_ARG1:
          subst_text = "%r";
          break;
        case KEEP_DATA_ARG2:
          subst_text = "%x";
          break;
      }
      FormatAppend(buffer, buffer_size, subst_text, cursor);
      return TRUE;
    case 'd':
      if (ANY == op->meta_op->s_bit) {
        FormatAppend(buffer, buffer_size,"%d", cursor);
      }
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
 *
 * That is, process the following directives:
 *
 * %K => '%x, %r' if keep arg1, and '%r' otherwise.
 * %d => '%d' if S bit is can be ANY (vs a specific value, which is assume
 *        to imply that the S bit is predefined by the instruction, rather
 *        than optionally set by the s instruction suffix).
 */
static Bool FixMetaDataOpFormat(char* buffer,
                                size_t buffer_size,
                                const char* format,
                                const MetaDataOpData* op) {
  return FormatData(buffer,
                    buffer_size,
                    format,
                    (void*) op,
                    FixMetaDataOpDirective);
}

/* After building a  prototype for the data instruction
 * (and having set the S bit as appropriate), finish
 * installing the data operation.
 */
static void FinishBuildingArmDataInstruction(
    ModeledOpInfo* op,
    const ModeledOpInfo* pattern,
    const MetaDataOpData* op_data) {
  char buffer[INST_BUFFER_SIZE];
  const MetaDataOp* meta_op = op_data->meta_op;

  /* Fix opcode by adding in S bit of pattern. */
  op->expected_values.opcode =
      (meta_op->opcode << 1) | op->expected_values.opcode;

  /* Fix format directive. */
  FixMetaDataOpFormat(buffer, sizeof(buffer),
                      pattern->describe_format, op_data);
  op->describe_format = strdup(buffer);
  switch (meta_op->keep_args) {
    case KEEP_DATA_ARG1:
      op->expected_values.arg2 = 0x0;
      break;
    case KEEP_DATA_ARG2:
      op->expected_values.arg1 = 0x0;
      break;
    default:
      break;
  }
  AddInstruction(op);
}

/* Add data operations to the instruction set. */
void BuildArmDataInstructions() {
  int i;
  MetaDataOpData op_data;
  for (i = 0; ; ++i) {
    int j;
    const MetaDataOp* meta_op = &(kMetaDataOps[i]);
    if (NULL == meta_op->name) break;
    op_data.meta_op = meta_op;
    for (j = 0; ; ++j) {
      ModeledOpInfo* op;
      const ModeledOpInfo* pattern = &(kDataOpPatterns[j]);
      if (ARM_INST_TYPE_SIZE == pattern->inst_type) break;
      op = CopyOfOpInfo(pattern);
      op->name = meta_op->name;
      op->inst_kind = meta_op->kind;

      /* Don't include if S bit not right in pattern */
      if (ANY == meta_op->s_bit) {
        ModeledOpInfo* op_copy = CopyOfOpInfo(op);
        op_data.s_value = 0x0;
        op->expected_values.opcode = 0x0;
        FinishBuildingArmDataInstruction(op, pattern, &op_data);
        op_data.s_value = 0x1;
        op_copy->expected_values.opcode = 0x1;
        FinishBuildingArmDataInstruction(op_copy, pattern, &op_data);
      } else {
        op_data.s_value = 0x0;
        op->expected_values.opcode = meta_op->s_bit;
        FinishBuildingArmDataInstruction(op, pattern, &op_data);

      }
    }
  }
}
