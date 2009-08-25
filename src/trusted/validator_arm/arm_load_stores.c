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
 * Defines ARM load and store operations.
 */

#include <string.h>

#include "native_client/src/trusted/validator_arm/arm_load_stores.h"
#include "native_client/src/trusted/validator_arm/arm_insts.h"
#include "native_client/src/trusted/validator_arm/arm_inst_modeling.h"
#include "native_client/src/shared/utils/formatting.h"

/* Categorizes types of load/stores. */
typedef enum LoadStoreCategory {
  SimpleLoadStore,
  TranslationLoadStore,
  MiscellaneousLoadStore,
  ExclusiveLoadStore
} LoadStoreCategory;

/*
 * Model load and store operations' specific data, in addition to the
 * kLoadStorePattern's.
 */
typedef struct MetaLoadStoreOp {
  Bool is_load;             /* true if the operation should model a store. */
  ArmInstKind kind;         /* Corresponding load/store instruction kind,
                             * based on immediate offset form.
                             */
  int32_t LSHCode;          /* The LSH bits of the code. */
} MetaLoadStoreOp;

/*
 * Extend the meta load/store data with additional options followed
 * while building load/store operations.
 */
typedef struct MetaLoadStoreExtendedOp {
  MetaLoadStoreOp* meta_op;   /* The load/store specific data to use. */
  Bool is_negative;           /* True if the negative offset version
                               * of the load/store should be generated.
                               */
  Bool is_byte_access;        /* True if the byte load/store version
                               * of the operation should be generated.
                              */
  LoadStoreCategory category; /* Category of load/store. */
} MetaLoadStoreExtendedOp;

/* Model Simple load/store (and not translation) register operations.
 * Note: The names for the patterns are not specified, as they will be
 * added by the driver code.
 */
static const ModeledOpInfo kSimpleLoadStorePatterns[] = {
  { "",
    ARM_LDR,
    kArmLsImmediateOffsetName,
    ARM_LS_IO,
    TRUE,
    "%C%b\t%x, [%r, #%-%i]",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x2, 0x10, ANY, ANY, NOT_USED, NOT_USED, NOT_USED, ANY }},
  { "",
    ARM_LDR,
    kArmLsRegisterOffsetName,
    ARM_LS_RO, TRUE,
    "%C%b\t%x, [%r, %-%z]",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x3, 0x10, ANY, ANY, 0x0, CONTAINS_ZERO, 0x0, NOT_USED }},
  { "",
    ARM_LDR,
    kArmLsScaledRegisterLslOffsetName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r, %-%z, lsl #%3]",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x3, 0x10, ANY, ANY, NON_ZERO,
      CONTAINS_ZERO, 0x0, NOT_USED } },
  { "",
    ARM_LDR,
    kArmLsScaledRegisterLsrOffsetName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r, %-%z, lsr #%#]",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x3, 0x10, ANY, ANY, ANY,
      CONTAINS_ZERO, 0x2, NOT_USED } },
  { "",
    ARM_LDR,
    kArmLsScaledRegisterAsrOffsetName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r, %-%z, asr #%#]",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x3, 0x10, ANY, ANY, ANY,
      CONTAINS_ZERO, 0x4, NOT_USED } },
  { "",
    ARM_LDR,
    kArmLsScaledRegisterRorOffsetName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r, %-%z, ror #%3]",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x3, 0x10, ANY, ANY, NON_ZERO,
      CONTAINS_ZERO, 0x6, NOT_USED } },
  { "",
    ARM_LDR,
    kArmLsScaledRegisterRrxOffsetName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r, %-%z, rrx]",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x3, 0x10, ANY, ANY, 0x0,
      CONTAINS_ZERO, 0x6, NOT_USED } },
  { "",
    ARM_LDR,
    kArmLsImmediatePreIndexedName,
    ARM_LS_IO,
    TRUE,
    "%C%b\t%x, [%r, #%-%i]!",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x2, 0x12, ANY, ANY, NOT_USED,
      NOT_USED, NOT_USED, ANY }},
  { "",
    ARM_LDR,
    kArmLsRegisterPreIndexedName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r, %-%z]!",
    ARM_WORD_LENGTH,
    Args_1_4_NotEqualName,
    { CONTAINS_ZERO, 0x3, 0x12, ANY, ANY, 0x0,
      CONTAINS_ZERO, 0x0, NOT_USED }},
  { "",
    ARM_LDR,
    kArmLsScaledRegisterLslPreIndexedName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r, %-%z, lsl #%3]!",
    ARM_WORD_LENGTH,
    Args_1_4_NotEqualName,
    { CONTAINS_ZERO, 0x3, 0x12, ANY, ANY, NON_ZERO,
      CONTAINS_ZERO, 0x0, NOT_USED }},
  { "",
    ARM_LDR,
    kArmLsScaledRegisterLsrPreIndexedName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r, %-%z, lsr #%#]!",
    ARM_WORD_LENGTH,
    Args_1_4_NotEqualName,
    { CONTAINS_ZERO, 0x3, 0x12, ANY, ANY, ANY,
      CONTAINS_ZERO, 0x2, NOT_USED }},
  { "",
    ARM_LDR,
    kArmLsScaledRegisterAsrPreIndexedName,
    ARM_LS_RO, TRUE,
    "%C%b\t%x, [%r, %-%z, asr #%#]!",
    ARM_WORD_LENGTH,
    Args_1_4_NotEqualName,
    { CONTAINS_ZERO, 0x3, 0x12, ANY, ANY, ANY,
      CONTAINS_ZERO, 0x4, NOT_USED }},
  { "",
    ARM_LDR,
    kArmLsScaledRegisterRorPreIndexedName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r, %-%z, ror #%3]!",
    ARM_WORD_LENGTH,
    Args_1_4_NotEqualName,
    { CONTAINS_ZERO, 0x3, 0x12, ANY, ANY, NON_ZERO,
      CONTAINS_ZERO, 0x6, NOT_USED }},
  { "",
    ARM_LDR,
    kArmLsScaledRegisterRrxPreIndexedName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r, %-%z, rrx]!",
    ARM_WORD_LENGTH,
    Args_1_4_NotEqualName,
    { CONTAINS_ZERO, 0x3, 0x12, ANY, ANY, 0x0,
      CONTAINS_ZERO, 0x6, NOT_USED }},
  END_OPINFO_LIST
};

/* Model the load/store specific values to plug into the
 * load/store patterns to generate the set of possible
 * load/store instructions.
 */
static const MetaLoadStoreOp kMetaSimpleLoadStoreOps[] = {
  {  TRUE , ARM_LDR, 0x4},
  { FALSE , ARM_STR, 0x0},
  /* Dummy field to end list. */
  { FALSE , ARM_UNKNOWN_INST, 0xFF },
};

/*
 * Model multiple load/stores.
 */
static const ModeledOpInfo kMultipleLoadStorePatterns[] = {
  { "ldm",
    ARM_LDM_1,
    kArmUndefinedAccessModeName,
    ARM_LS_MULT,
    TRUE,
    "%C%M\t%r, %S",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x4, 0x1, CONTAINS_ZERO, NOT_USED, NOT_USED,
      NOT_USED, NOT_USED, ANY }},
  { "ldm",
    ARM_LDM_1_MODIFY,
    kArmUndefinedAccessModeName,
    ARM_LS_MULT,
    TRUE,
    "%C%M\t%r!, %S",
    ARM_WORD_LENGTH,
    Arg1NotInLoadStoreRegistersName,
    { CONTAINS_ZERO, 0x4, 0x3, CONTAINS_ZERO, NOT_USED, NOT_USED,
      NOT_USED, NOT_USED, ANY }},
  { "ldm",
    ARM_LDM_2,
    kArmUndefinedAccessModeName,
    ARM_LS_MULT,
    TRUE,
    "%C%M\t%r, %S^",
    ARM_WORD_LENGTH,
    NULLName,            /* placeholder for LDM(3) when r15 in registers. */
    { CONTAINS_ZERO, 0x4, 0x5, CONTAINS_ZERO, NOT_USED, NOT_USED,
      NOT_USED, NOT_USED, ANY }},
  { "ldm",
    ARM_LDM_3,
    kArmUndefinedAccessModeName,
    ARM_LS_MULT,
    TRUE,
    "%C%M\t%r!, %S^",
    ARM_WORD_LENGTH,
    R15InLoadStoreRegistersName,
    { CONTAINS_ZERO, 0x4, 0x7, CONTAINS_ZERO, NOT_USED, NOT_USED,
      NOT_USED, NOT_USED, ANY }},
  { "stm",
    ARM_STM_1,
    kArmUndefinedAccessModeName,
    ARM_LS_MULT,
    TRUE,
    "%C%M\t%r, %S",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x4, 0x0, CONTAINS_ZERO, NOT_USED, NOT_USED,
      NOT_USED, NOT_USED, ANY }},
  { "stm",
    ARM_STM_1_MODIFY,
    kArmUndefinedAccessModeName,
    ARM_LS_MULT,
    TRUE,
    "%C%M\t%r!, %S", ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x4, 0x2, CONTAINS_ZERO, NOT_USED, NOT_USED,
      NOT_USED, NOT_USED, ANY }},
  { "stm",
    ARM_STM_2,
    kArmUndefinedAccessModeName,
    ARM_LS_MULT,
    TRUE,
    "%C%M\t%r, %S^",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x4, 0x4, CONTAINS_ZERO, NOT_USED, NOT_USED,
      NOT_USED, NOT_USED, ANY }},
  END_OPINFO_LIST
};

/*
 * Model exclusive load/stores for shared memory.
 */
static const ModeledOpInfo kExclusiveLoadStorePatterns[] = {
  { "ldr",
    ARM_LDREX,
    kArmUndefinedAccessModeName,
    ARM_DP_RS,
    TRUE,
    "%Cex\t%x, [%r]",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x0, 0x18, CONTAINS_ZERO, CONTAINS_ZERO,
      0x0, 0x0, 0x0, 0x9 }},
  END_OPINFO_LIST
};

/* Model Simple and translation load/store register operations.
 * Note: The names for the patterns are not specified, as they will be
 * added by the driver code.
 */
static const ModeledOpInfo kTranslationLoadStorePatterns[] = {
  { "",
    ARM_LDR,
    kArmLsImmediatePostIndexedName,
    ARM_LS_IO,
    TRUE,
    "%C%b\t%x, [%r], #%-%i",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x2, 0x0, ANY, ANY, NOT_USED,
      NOT_USED, NOT_USED, ANY }},
  { "",
    ARM_LDR,
    kArmLsRegisterPostIndexedName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r], %-%z",
    ARM_WORD_LENGTH,
    Args_1_4_NotEqualName,
    { CONTAINS_ZERO, 0x3, 0x0, ANY, ANY, 0x0,
      CONTAINS_ZERO, 0x0, NOT_USED }},
  { "",
    ARM_LDR,
    kArmLsScaledRegisterLslPostIndexedName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r], %-%z, lsl #%3",
    ARM_WORD_LENGTH,
    Args_1_4_NotEqualName,
    { CONTAINS_ZERO, 0x3, 0x0, ANY, ANY, NON_ZERO,
      CONTAINS_ZERO, 0x0, NOT_USED }},
  { "",
    ARM_LDR,
    kArmLsScaledRegisterLsrPostIndexedName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r], %-%z, lsr #%#",
    ARM_WORD_LENGTH,
    Args_1_4_NotEqualName,
    { CONTAINS_ZERO, 0x3, 0x0, ANY, ANY, ANY,
      CONTAINS_ZERO, 0x2, NOT_USED }},
  { "",
    ARM_LDR,
    kArmLsScaledRegisterAsrPostIndexedName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r], %-%z, asr #%#",
    ARM_WORD_LENGTH,
    Args_1_4_NotEqualName,
    { CONTAINS_ZERO, 0x3, 0x0, ANY, ANY, ANY,
      CONTAINS_ZERO, 0x4, NOT_USED }},
  { "",
    ARM_LDR,
    kArmLsScaledRegisterRorPostIndexedName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r], %-%z, ror #%3",
    ARM_WORD_LENGTH,
    Args_1_4_NotEqualName,
    { CONTAINS_ZERO, 0x3, 0x0, ANY, ANY, NON_ZERO,
      CONTAINS_ZERO, 0x6, NOT_USED }},
  { "",
    ARM_LDR,
    kArmLsScaledRegisterRrxPostIndexedName,
    ARM_LS_RO,
    TRUE,
    "%C%b\t%x, [%r], %-%z, rrx",
    ARM_WORD_LENGTH,
    Args_1_4_NotEqualName,
    { CONTAINS_ZERO, 0x3, 0x0, ANY, ANY, 0x0,
      CONTAINS_ZERO, 0x6, NOT_USED }},
  END_OPINFO_LIST
};

/* Model irregular load/store specific values to plug into the
 * load/store patterns to generate the set of possible
 * load/store instructions.
 *
 * Note: for these instructions, the load/store model isn't
 * regular like the other loads/stores. The encodings are based
 * on the following:
 *
 * Bits:
 *
 * U = bit[23] => If zero, then address differencing is being applied.
 *
 * L = bit[20],
 * SH = bit[5:6]
 *
 * L=0, SH=01 store halfword.
 * L=0, SH=10 load doubleword.
 * L=0, SH=11 store doubleword.
 * L=1, SH=01 load unsigned halfword.
 * L=1, SH=10 load signed byte.
 * L=1, SH=11 load signed halfword.
 *
 * Note: Signed bytes and halfwords can be stored with ordinary STRB and
 * STRH instructions, and hence, they are not in this encoding.
 */
static const MetaLoadStoreOp kMetaMiscellaneousLoadStoreOps[] = {
  { FALSE , ARM_STR_HALFWORD,          0x1 },
  { TRUE,   ARM_LDR_DOUBLEWORD,        0x2 },
  { FALSE,  ARM_STR_DOUBLEWORD,        0x3 },
  { TRUE,   ARM_LDR_UNSIGNED_HALFWORD, 0x5 },
  { TRUE,   ARM_LDR_SIGNED_BYTE,       0x6 },
  { TRUE,   ARM_LDR_SIGNED_HALFWORD,   0x7},
  /* Dummy field to end list. */
  { FALSE , ARM_UNKNOWN_INST,          0xFF },
};

/* Model other irregular load/store register operations.
 * For more information on the irregular pattern, see
 * comments for kMetaMiscellaneousLoadStoreOps below.
 * Note: The names for the patterns are not specified, as they will be
 * added by the driver code.
 */
static const ModeledOpInfo kMiscellaneousLoadStorePatterns[] = {
  { "",
    ARM_STR_HALFWORD,
    kArmMlsImmediateOffsetName,
    ARM_DP_RS,
    TRUE,
    "%C%L\t%x, [%r%h]",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x0, 0x14, ANY, ANY, ANY, ANY, 0x0, 0x9 }},
  { "",
    ARM_STR_HALFWORD,
    kArmMlsRegisterOffsetName,
    ARM_DP_RS,
    TRUE,
    "%C%L\t%x, [%r %-%z]",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x0, 0x10, ANY, ANY, 0x0, CONTAINS_ZERO, 0x0, 0x9 }},

  { "",
    ARM_STR_HALFWORD,
    kArmMlsImmediatePreIndexedName,
    ARM_DP_RS,
    TRUE,
    "%C%L\t%x, %r, #%-%H]!",
    ARM_WORD_LENGTH,
    Args_3_4_NotBothZeroName,
    { CONTAINS_ZERO, 0x0, 0x16, CONTAINS_ZERO, ANY, ANY, ANY, 0x0, 0x9 }},

  { "",
    ARM_STR_HALFWORD,
    kArmMlsRegisterPreIndexedName,
    ARM_DP_RS,
    TRUE,
    "%C%L\t%x, [%r, %-%z]!",
    ARM_WORD_LENGTH,
    Args_1_4_NotEqualName,
    { CONTAINS_ZERO, 0x0, 0x12, CONTAINS_ZERO, ANY, 0x0,
      CONTAINS_ZERO, 0x0, 0x9 }},

  { "",
    ARM_STR_HALFWORD,
    kArmMlsImmediatePostIndexedName,
    ARM_DP_RS,
    TRUE,
    "%C%L\t%x, [%r], #%-%H",
    ARM_WORD_LENGTH,
    Args_3_4_NotBothZeroName,
    { CONTAINS_ZERO, 0x0, 0x4, CONTAINS_ZERO, ANY, ANY, ANY, 0x0, 0x9 }},

  { "",
    ARM_STR_HALFWORD,
    kArmMlsRegisterPreIndexedName,
    ARM_DP_RS, TRUE,
    "%C%L\t%x, [%r], %-%z",
    ARM_WORD_LENGTH,
    NULLName,
    { CONTAINS_ZERO, 0x0, 0x0, CONTAINS_ZERO, CONTAINS_ZERO, ANY,
      ANY, 0x0, 0x9 }},
  END_OPINFO_LIST
};

/*
 * Return name suffix of Miscellaneous load/store instruction based on LSH bit
 * sequence.
 */
static const char* GetMiscellaneousLoadStoreSuffix(int32_t lsh_value) {
  switch (lsh_value) {
    case 0x1:
    case 0x5:
      return "h";
    case 0x2:
    case 0x3:
      return "d";
    case 0x6:
      return "sb";
    case 0x7:
      return "sh";
    default:
      /* This shouldn't happen */
      fprintf(stderr, "Unrecognized load/store lsh value: %03x\n", lsh_value);
      return "?";
  }
}

/*
 * Processes format directives for the load/store operations.
 *
 * %b - If the load/store loads bytes (instead of words), replace with the
 *      letter b (and nothing otherwise). If the load/store is a translation
 *      operation, replace with the letter t. If both, replace with the
 *      letters bt.
 * %L - Insert miscellaneous load/store suffix, based on LSH value.
 * %- - If the load/store operation modifies by a negative offset, replace
 *      with a - (minus sign), and nothing otherwise.
 */
static Bool FixMetaLoadStoreDirective(char directive,
                                      char* buffer,
                                      size_t buffer_size,
                                      void* data,
                                      size_t* cursor) {
  MetaLoadStoreExtendedOp* op = (MetaLoadStoreExtendedOp*) data;
  switch (directive) {
    case 'b':
      switch (op->category) {
        case SimpleLoadStore:
          if (op->is_byte_access) {
            FormatAppend(buffer, buffer_size, "b", cursor);
          }
          return TRUE;
        case TranslationLoadStore:
          if (op->is_byte_access) {
            FormatAppend(buffer, buffer_size, "bt", cursor);
          } else {
            FormatAppend(buffer, buffer_size, "t", cursor);
          }
          return TRUE;
        default:
          return FALSE;
      }
    case 'L':
      if (op->category == MiscellaneousLoadStore) {
        FormatAppend(buffer,
                     buffer_size,
                     GetMiscellaneousLoadStoreSuffix(op->meta_op->LSHCode),
                     cursor);
        return TRUE;
      }
      return FALSE;
    case '-':
      if (op->is_negative) {
        FormatAppend(buffer, buffer_size, "-", cursor);
      }
      return TRUE;
    default:
      return FALSE;
  }
}

static Bool FixMetaLoadStoreFormat(char* buffer,
                            uint32_t buffer_size,
                            const char* format,
                            const MetaLoadStoreExtendedOp* op) {
  return FormatData(buffer, buffer_size,
                    format, (void*) op, FixMetaLoadStoreDirective);
}

/*
 * Fix multiple load store values.
 *
 * %M - Insert multiple load/store suffix, based on PU (bits[23:24]).
 */
static Bool FixMultipleLoadStoreDirective(char directive,
                                          char* buffer,
                                          size_t buffer_size,
                                          void* data,
                                          size_t* cursor) {
  OpInfo* op = (OpInfo*) data;
  switch (directive) {
    case 'M':
      {
        int32_t bits = (op->expected_values.opcode & 0x18) >> 3;
        switch (bits) {
          case 0x0:
            FormatAppend(buffer, buffer_size, "da", cursor);
            return TRUE;
          case 0x1:
            FormatAppend(buffer, buffer_size, "ia", cursor);
            return TRUE;
          case 0x2:
            FormatAppend(buffer, buffer_size, "db", cursor);
            return TRUE;
          case 0x3:
            FormatAppend(buffer, buffer_size, "ib", cursor);
            return TRUE;
          default:
            /* This should not happen, but be safe. */
            return FALSE;
        }
      }
    default:
      return FALSE;
  }
}

static Bool FixMultipleLoadStoreFormat(ModeledOpInfo* op) {
  char buffer[INST_BUFFER_SIZE];
  Bool results = FormatData(buffer,
                            sizeof(buffer),
                            op->describe_format,
                            (void*) op,
                            FixMultipleLoadStoreDirective);
  op->describe_format = strdup(buffer);
  return results;
}

/*
 * Given the data in the extended meta op (for load/stores),
 * and the instruction pattern, whether the U bit is negative,
 * and the category defining the type of load/store, build
 * the corresponding load/store instruction.
 */
static void BuildLoadStorePattern(
    MetaLoadStoreExtendedOp* extended_meta_op,
    const ModeledOpInfo* pattern,
    Bool is_negative,
    LoadStoreCategory category) {
  char buffer[INST_BUFFER_SIZE];
  int access;
  const MetaLoadStoreOp* meta_op = extended_meta_op->meta_op;
  int access_count;
  switch (category) {
    case MiscellaneousLoadStore:
    case ExclusiveLoadStore:
      access_count = 1;
      break;
    default:
      access_count = 2;
      break;
  }
  for (access = 0; access < access_count; ++access) {
    ModeledOpInfo* op = CopyOfOpInfo(pattern);
    extended_meta_op->is_negative = is_negative;
    extended_meta_op->is_byte_access = (access == 1);
    extended_meta_op->category = category;
    op->name = (meta_op->is_load ? "ldr" : "str");
    if (meta_op->LSHCode & 0x4) {
      op->expected_values.opcode |= 0x1;
    }
    if (MiscellaneousLoadStore == category) {
      op->inst_kind = meta_op->kind;
      op->expected_values.shift = (meta_op->LSHCode & 0x3);
    } else {
      op->inst_kind = meta_op->kind;
      if (extended_meta_op->is_byte_access) {
        op->expected_values.opcode |= 0x4;
      }
      if (TranslationLoadStore == category) {
        /* Set the translation bit[21] to 1 */
        op->expected_values.opcode |= 0x2;
      }
    }
    if (ExclusiveLoadStore != category && !is_negative) {
      op->expected_values.opcode |= 0x8;
    }
    FixMetaLoadStoreFormat(buffer,
                           sizeof(buffer),
                           pattern->describe_format,
                           extended_meta_op);
    op->describe_format = strdup(buffer);
    AddInstruction(op);
  }
}

/*
 * Build the set of exclusive (i.e. locking) load/store
 * instructions from the corresponding patterns.
 */
static void BuildExclusiveLoadStorePatterns() {
  int i;
  for (i = 0; ; ++i) {
    MetaLoadStoreExtendedOp extended_meta_op;
    int j;
    const MetaLoadStoreOp* meta_op = &(kMetaSimpleLoadStoreOps[i]);
    if (0xFF == meta_op->LSHCode) break;
    extended_meta_op.meta_op = (MetaLoadStoreOp*) meta_op;
    for (j = 0; ; ++j) {
      const ModeledOpInfo* pattern = &(kExclusiveLoadStorePatterns[j]);
      if (ARM_INST_TYPE_SIZE == pattern->inst_type) break;
      BuildLoadStorePattern(&extended_meta_op,
                            pattern,
                            FALSE,
                            ExclusiveLoadStore);
    }
  }
}

/*
 * Build the set of multiple load/store instructions
 * (which load/store a set of registers).
 */
static void BuildMultipleLoadStorePatterns() {
  int i = 0;
  int j;
  for (i = 0; ; ++i) {
    const ModeledOpInfo* pattern = &(kMultipleLoadStorePatterns[i]);
    if (ARM_INST_TYPE_SIZE == pattern->inst_type) break;
    for (j = 0; j < 4; ++j) {
      ModeledOpInfo* op = CopyOfOpInfo(pattern);
      op->expected_values.opcode |= (j << 3);
      FixMultipleLoadStoreFormat(op);
      AddInstruction(op);
    }
  }
}

/*
 * Build all load/store instructions for the given
 * meta operations, the given patterns, and the
 * type (i.e. category) of load/store.
 */
static void BuildLoadStorePatterns(
    const MetaLoadStoreOp meta_ops[],
    const ModeledOpInfo patterns[],
    LoadStoreCategory category) {
  int i;
  for (i = 0; ; ++i) {
    MetaLoadStoreExtendedOp extended_meta_op;
    int j;
    const MetaLoadStoreOp* meta_op = &(meta_ops[i]);
    if (0xFF == meta_op->LSHCode) break;
    extended_meta_op.meta_op = (MetaLoadStoreOp*) meta_op;
    for (j = 0; ; ++j) {
      int sign;
      const ModeledOpInfo* pattern = &(patterns[j]);
      if (ARM_INST_TYPE_SIZE == pattern->inst_type) break;
      for (sign = 0; sign < 2; ++sign) {
        BuildLoadStorePattern(&extended_meta_op,
                              pattern,
                              sign == 0,
                              category);
      }
    }
  }
}

/*
 * Build all load/store instructions/
 */
void BuildLoadStores() {
  BuildLoadStorePatterns(kMetaSimpleLoadStoreOps,
                         kSimpleLoadStorePatterns,
                         SimpleLoadStore);
  BuildLoadStorePatterns(kMetaSimpleLoadStoreOps,
                         kTranslationLoadStorePatterns,
                         SimpleLoadStore);
  BuildLoadStorePatterns(kMetaSimpleLoadStoreOps,
                         kTranslationLoadStorePatterns,
                         TranslationLoadStore);
  BuildLoadStorePatterns(kMetaMiscellaneousLoadStoreOps,
                         kMiscellaneousLoadStorePatterns,
                         MiscellaneousLoadStore);
  BuildExclusiveLoadStorePatterns();
  BuildMultipleLoadStorePatterns();
}
