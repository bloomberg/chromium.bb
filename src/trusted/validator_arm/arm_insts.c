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
 * Defines the API to modeled ARM instructions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "native_client/src/trusted/validator_arm/arm_insts.h"
#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/shared/utils/formatting.h"

/*
 * Command line option specifying how to print out
 * conditional opcodes. The default is: "(eq) op"
 * (i.e. the condition followed by the opcode)
 * (which happens when this flag is false). When this
 * flag is true, it prints "opeq" (i.e. the opcode followed
 * by the condition).
 */
Bool FLAGS_name_cond = FALSE;

const char* GetArmInstTypeName(ArmInstType type) {
  if (type >= ARM_INST_TYPE_SIZE) {
    return "?UNKNOWN_ARM_TYPE_SIZE?";
  } else {
    static const char* kArmInstTypeString[ARM_INST_TYPE_SIZE] = {
      "ARM_UNDEFINED",
      "ARM_ILLEGAL",
      "ARM_INVALID",
      "ARM_DP_IS",
      "ARM_MISC_SAT",
      "ARM_CPS",
      "ARM_DP_RS",
      "ARM_DP_RE",
      "ARM_DP_I",
      "ARM_LS_IO",
      "ARM_LS_RO",
      "ARM_LS_MULT",
      "ARM_BRANCH",
      "ARM_BRANCH_RS",
      "ARM_CP_F1",
      "ARM_CP_F2",
      "ARM_CP_F3",
      "ARM_CP_F4"
    };
    return kArmInstTypeString[type];
  }
}

const char* GetArmInstKindName(ArmInstKind kind) {
  if (kind >= ARM_INST_KIND_SIZE) {
    return "?UNKNOWN_ARM_KIND_SIZE";
  } else {
    static const char* kArmInstKindString[ARM_INST_KIND_SIZE] = {
      "ARM_UNKNOWN_INST",
      "ARM_BRANCH_INST",
      "ARM_BRANCH_AND_LINK",
      "ARM_BRANCH_WITH_LINK_AND_EXCHANGE_1",
      "ARM_BRANCH_WITH_LINK_AND_EXCHANGE_2",
      "ARM_BRANCH_AND_EXCHANGE",
      "ARM_BRANCH_AND_EXCHANGE_TO_JAZELLE",
      "ARM_AND",
      "ARM_EOR",
      "ARM_SUB",
      "ARM_RSB",
      "ARM_ADD",
      "ARM_ADC",
      "ARM_SBC",
      "ARM_RSC",
      "ARM_TST",
      "ARM_TEQ",
      "ARM_CMP",
      "ARM_CMN",
      "ARM_ORR",
      "ARM_MOV",
      "ARM_BIC",
      "ARM_MVN",
      "ARM_MLA",
      "ARM_MUL",
      "ARM_SMLA_XY",
      "ARM_SMUAD",
      "ARM_SMUADX",
      "ARM_SMLAD",
      "ARM_SMLADX",
      "ARM_SMLAL",
      "ARM_SMLAL_XY",
      "ARM_SMLALD",
      "ARM_SMLALDX",
      "ARM_SMLAWB",
      "ARM_SMLAWT",
      "ARM_SMUSD",
      "ARM_SMUSDX",
      "ARM_SMLSD",
      "ARM_SMLSDX",
      "ARM_SMLSLD",
      "ARM_SMLSLDX",
      "ARM_SMMUL",
      "ARM_SMMULR",
      "ARM_SMMLA",
      "ARM_SMMLAR",
      "ARM_SMMLS",
      "ARM_SMMLSR",
      "ARM_SMUL_XY",
      "ARM_SMULL",
      "ARM_SMULWB",
      "ARM_SMULWT",
      "ARM_UMAAL",
      "ARM_UMLAL",
      "ARM_UMULL",
      "ARM_QADD",
      "ARM_QADD16",
      "ARM_QADD8",
      "ARM_QADDSUBX",
      "ARM_QDADD",
      "ARM_QSUB",
      "ARM_QSUB16",
      "ARM_QSUB8",
      "ARM_QSUBADDX",
      "ARM_QDSUB",
      "ARM_SADD16",
      "ARM_SADD8",
      "ARM_SADDSUBX",
      "ARM_SSUB16",
      "ARM_SSUB8",
      "ARM_SSUBADDX",
      "ARM_SHADD16",
      "ARM_SHADD8",
      "ARM_SHADDSUBX",
      "ARM_SHSUB16",
      "ARM_SHSUB8",
      "ARM_SHSUBADDX",
      "ARM_UADD16",
      "ARM_UADD8",
      "ARM_UADDSUBX",
      "ARM_USUB16",
      "ARM_USUB8",
      "ARM_USUBADDX",
      "ARM_UHADD16",
      "ARM_UHADD8",
      "ARM_UHADDSUBX",
      "ARM_UHSUB16",
      "ARM_UHSUB8",
      "ARM_UHSUBADDX",
      "ARM_UQADD16",
      "ARM_UQADD8",
      "ARM_UQADDSUBX",
      "ARM_UQSUB16",
      "ARM_UQSUB8",
      "ARM_UQSUBADDX",
      "ARM_SXTAB16",
      "ARM_SXTAB",
      "ARM_SXTAH",
      "ARM_SXTB16",
      "ARM_SXTB",
      "ARM_SXTH",
      "ARM_UXTAB16",
      "ARM_UXTAB",
      "ARM_UXTAH",
      "ARM_UXTB16",
      "ARM_UXTB",
      "ARM_UXTH",
      "ARM_CLZ",
      "ARM_USAD8",
      "ARM_USADA8",
      "ARM_PKHBT_1",
      "ARM_PKHBT_1_LSL",
      "ARM_PKHBT_2",
      "ARM_PKHBT_2_ASR",
      "ARM_REV",
      "ARM_REV16",
      "ARM_REVSH",
      "ARM_SEL",
      "ARM_SSAT",
      "ARM_SSAT_LSL",
      "ARM_SSAT_ASR",
      "ARM_SSAT16",
      "ARM_USAT",
      "ARM_USAT_LSL",
      "ARM_USAT_ASR",
      "ARM_USAT16",
      "ARM_MRS_CPSR",
      "ARM_MRS_SPSR",
      "ARM_MSR_CPSR_IMMEDIATE",
      "ARM_MSR_CPSR_REGISTER",
      "ARM_MSR_SPSR_IMMEDIATE",
      "ARM_MSR_SPSR_REGISTER",
      "ARM_CPS_FLAGS",
      "ARM_CPS_FLAGS_MODE",
      "ARM_CPS_MODE",
      "ARM_SETEND_BE",
      "ARM_SETEND_LE",
      "ARM_LDR",
      "ARM_LDREX",
      "ARM_STR",
      "ARM_STREX",
      "ARM_STR_HALFWORD",
      "ARM_LDR_DOUBLEWORD",
      "ARM_STR_DOUBLEWORD",
      "ARM_LDR_UNSIGNED_HALFWORD",
      "ARM_LDR_SIGNED_BYTE",
      "ARM_LDR_SIGNED_HALFWORD",
      "ARM_LDM_1",
      "ARM_LDM_1_MODIFY",
      "ARM_LDM_2",
      "ARM_LDM_3",
      "ARM_STM_1",
      "ARM_STM_1_MODIFY",
      "ARM_STM_2",
      "ARM_SWP",
      "ARM_SWPB",
      "ARM_BKPT",
      "ARM_SWI",
      "ARM_CDP",
      "ARM_MCR",
      "ARM_MCRR",
      "ARM_MRC",
      "ARM_MRRC",
      "ARM_LDC",
      "ARM_STC"
    };
    return kArmInstKindString[kind];
  }
}

const char* GetArmAddressingModeName(ArmAddressingMode mode) {
  if (mode >= ARM_ADDRESSING_SIZE) {
    return "?UNKNOWN_ARM_ADDRESSING_MODE?";
  } else {
    static const char* kArmAddressingModeString[ARM_ADDRESSING_SIZE] = {
      "ARM_ADDRESSING_UNDEFINED",
      "ARM_DATA_IMMEDIATE",
      "ARM_DATA_REGISTER",
      "ARM_DATA_RRX",
      "ARM_LS_IMMEDIATE",
      "ARM_LS_REGISTER",
      "ARM_LS_SCALED_REGISTER",
      "ARM_MLS_IMMEDIATE",
      "ARM_MLS_REGISTER",
      "ARM_LSC_IMMEDIATE",
      "ARM_LSC_UNINDEXED"
    };
    return kArmAddressingModeString[mode];
  }
}

const char* GetArmSubaddressingModeName(ArmSubaddressingMode mode) {
  if (mode >= ARM_SUBADDRESSING_SIZE) {
    return "?UNKNOWN_ARM_SUBADDRESSING_MODE?";
  } else {
    static const char* kArmSubaddressingModeString[ARM_SUBADDRESSING_SIZE] = {
      "ARM_SUBADDRESSING_UNDEFINED",
      "ARM_SUBADDRESSING_LSL",
      "ARM_SUBADDRESSING_LSR",
      "ARM_SUBADDRESSING_ASR",
      "ARM_SUBADDRESSING_ROR",
      "ARM_SUBADDRESSING_RRX",
    };
    return kArmSubaddressingModeString[mode];
  }
}

const char* GetArmIndexingModeName(ArmIndexingMode mode) {
  if (mode >= ARM_INDEXING_SIZE) {
    return "?UNKNOWN_ARM_ADDRESSING_MODE?";
  } else {
    static const char* kArmIndexingModeString[ARM_INDEXING_SIZE] = {
      "ARM_INDEXING_UNDEFINED",
      "ARM_INDEXING_OFFSET",
      "ARM_INDEXING_PRE",
      "ARM_INDEXING_POST",
    };
    return kArmIndexingModeString[mode];
  }
}

const ArmAccessMode kArmUndefinedAccessMode ={
  ARM_ADDRESSING_UNDEFINED,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_UNDEFINED
};

const ArmAccessMode kArmDataImmediate = {
  ARM_DATA_IMMEDIATE,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_UNDEFINED
};

const ArmAccessMode kArmDataLslImmediate = {
  ARM_DATA_IMMEDIATE,
  ARM_SUBADDRESSING_LSL,
  ARM_INDEXING_UNDEFINED
};

const ArmAccessMode kArmDataLsrImmediate = {
  ARM_DATA_IMMEDIATE,
  ARM_SUBADDRESSING_LSR,
  ARM_INDEXING_UNDEFINED
};

const ArmAccessMode kArmDataAsrImmediate = {
  ARM_DATA_IMMEDIATE,
  ARM_SUBADDRESSING_ASR,
  ARM_INDEXING_UNDEFINED
};

const ArmAccessMode kArmDataRorImmediate = {
  ARM_DATA_IMMEDIATE,
  ARM_SUBADDRESSING_ROR,
  ARM_INDEXING_UNDEFINED
};

const ArmAccessMode kArmDataRrx = {
  ARM_DATA_RRX,
  ARM_SUBADDRESSING_RRX,
  ARM_INDEXING_UNDEFINED
};

const ArmAccessMode kArmDataRegister = {
  ARM_DATA_REGISTER,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_UNDEFINED
};

const ArmAccessMode kArmDataLslRegister = {
  ARM_DATA_REGISTER,
  ARM_SUBADDRESSING_LSL,
  ARM_INDEXING_UNDEFINED
};

const ArmAccessMode kArmDataLsrRegister = {
  ARM_DATA_REGISTER,
  ARM_SUBADDRESSING_LSR,
  ARM_INDEXING_UNDEFINED
};

const ArmAccessMode kArmDataAsrRegister = {
  ARM_DATA_REGISTER,
  ARM_SUBADDRESSING_ASR,
  ARM_INDEXING_UNDEFINED
};

const ArmAccessMode kArmDataRorRegister = {
  ARM_DATA_REGISTER,
  ARM_SUBADDRESSING_ROR,
  ARM_INDEXING_UNDEFINED
};

const ArmAccessMode kArmLsImmediateOffset = {
  ARM_LS_IMMEDIATE,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_OFFSET
};

const ArmAccessMode kArmLsRegisterOffset = {
  ARM_LS_REGISTER,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_OFFSET
};

const ArmAccessMode kArmLsScaledRegisterLslOffset = {
  ARM_LS_SCALED_REGISTER,
  ARM_SUBADDRESSING_LSL,
  ARM_INDEXING_OFFSET
};

const ArmAccessMode kArmLsScaledRegisterLsrOffset = {
  ARM_LS_SCALED_REGISTER,
  ARM_SUBADDRESSING_LSR,
  ARM_INDEXING_OFFSET
};

const ArmAccessMode kArmLsScaledRegisterAsrOffset = {
  ARM_LS_SCALED_REGISTER,
  ARM_SUBADDRESSING_ASR,
  ARM_INDEXING_OFFSET
};

const ArmAccessMode kArmLsScaledRegisterRorOffset = {
  ARM_LS_SCALED_REGISTER,
  ARM_SUBADDRESSING_ROR,
  ARM_INDEXING_OFFSET
};

const ArmAccessMode kArmLsScaledRegisterRrxOffset = {
  ARM_LS_SCALED_REGISTER,
  ARM_SUBADDRESSING_RRX,
  ARM_INDEXING_OFFSET
};

const ArmAccessMode kArmLsImmediatePreIndexed = {
  ARM_LS_IMMEDIATE,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_PRE
};

const ArmAccessMode kArmLsRegisterPreIndexed = {
  ARM_LS_REGISTER,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_PRE
};

const ArmAccessMode kArmLsScaledRegisterLslPreIndexed = {
  ARM_LS_SCALED_REGISTER,
  ARM_SUBADDRESSING_LSL,
  ARM_INDEXING_PRE
};

const ArmAccessMode kArmLsScaledRegisterLsrPreIndexed = {
  ARM_LS_SCALED_REGISTER,
  ARM_SUBADDRESSING_LSR,
  ARM_INDEXING_PRE
};

const ArmAccessMode kArmLsScaledRegisterAsrPreIndexed = {
  ARM_LS_SCALED_REGISTER,
  ARM_SUBADDRESSING_ASR,
  ARM_INDEXING_PRE
};

const ArmAccessMode kArmLsScaledRegisterRorPreIndexed = {
  ARM_LS_SCALED_REGISTER,
  ARM_SUBADDRESSING_ROR,
  ARM_INDEXING_PRE
};

const ArmAccessMode kArmLsScaledRegisterRrxPreIndexed = {
  ARM_LS_SCALED_REGISTER,
  ARM_SUBADDRESSING_RRX,
  ARM_INDEXING_PRE
};

const ArmAccessMode kArmLsImmediatePostIndexed = {
  ARM_LS_IMMEDIATE,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_POST
};

const ArmAccessMode kArmLsRegisterPostIndexed = {
  ARM_LS_REGISTER,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_POST
};

const ArmAccessMode kArmLsScaledRegisterLslPostIndexed = {
  ARM_LS_SCALED_REGISTER,
  ARM_SUBADDRESSING_LSL,
  ARM_INDEXING_POST
};

const ArmAccessMode kArmLsScaledRegisterLsrPostIndexed = {
  ARM_LS_SCALED_REGISTER,
  ARM_SUBADDRESSING_LSR,
  ARM_INDEXING_POST
};

const ArmAccessMode kArmLsScaledRegisterAsrPostIndexed = {
  ARM_LS_SCALED_REGISTER,
  ARM_SUBADDRESSING_ASR,
  ARM_INDEXING_POST
};

const ArmAccessMode kArmLsScaledRegisterRorPostIndexed = {
  ARM_LS_SCALED_REGISTER,
  ARM_SUBADDRESSING_ROR,
  ARM_INDEXING_POST
};

const ArmAccessMode kArmLsScaledRegisterRrxPostIndexed = {
  ARM_LS_SCALED_REGISTER,
  ARM_SUBADDRESSING_RRX,
  ARM_INDEXING_POST
};

const ArmAccessMode kArmMlsImmediateOffset = {
  ARM_MLS_IMMEDIATE,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_OFFSET
};

const ArmAccessMode kArmMlsRegisterOffset = {
  ARM_MLS_REGISTER,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_OFFSET
};

const ArmAccessMode kArmMlsImmediatePreIndexed = {
  ARM_MLS_IMMEDIATE,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_PRE
};

const ArmAccessMode kArmMlsRegisterPreIndexed = {
  ARM_MLS_REGISTER,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_PRE
};

const ArmAccessMode kArmMlsImmediatePostIndexed = {
  ARM_MLS_IMMEDIATE,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_POST
};

const ArmAccessMode kArmMlsRegisterPostIndexed = {
  ARM_MLS_REGISTER,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_POST
};

const ArmAccessMode kArmLscImmediateOffset = {
  ARM_LSC_IMMEDIATE,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_OFFSET
};

const ArmAccessMode kArmLscImmediatePreIndexed = {
  ARM_LSC_IMMEDIATE,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_PRE
};

const ArmAccessMode kArmLscImmediatePostIndexed = {
  ARM_LSC_IMMEDIATE,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_POST
};

const ArmAccessMode kArmLscUnindexed = {
  ARM_LSC_UNINDEXED,
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_INDEXING_UNDEFINED
};

const InstValues kArmDontCareMask = ARM_DONT_CARE_MASK;

const InstValues* GetArmInstMasks(ArmInstType type) {
  if (type >= ARM_INST_TYPE_SIZE) {
    return &kArmDontCareMask;
  } else {
    static const InstValues kArmInstMasks[ARM_INST_TYPE_SIZE] = {
      /* ARM_UNDEFINED */
      ARM_DONT_CARE_MASK,
      /* ARM_ILLEGAL */
      ARM_DONT_CARE_MASK,
      /* ARM_INVALID */
      ARM_DONT_CARE_MASK,
      /* ARM_DP_IS */
      { 0xF0000000, 0x0E000000, 0x01F00000,
        0x000F0000, 0x0000F000, 0x00000F80,
        0x0000000F, 0x00000060, 0x00000010 },
      /* ARM_MISC_SAT */
      { 0xF0000000, 0x0E000000, 0x01E00000,
        0x001F0000, 0x0000F000, 0x00000F80,
        0x0000000F, 0x00000070, 0x00000000 },
      /* ARM_CPS */
      { 0xF0000000, 0x0E000000, 0x01F00000,
        0x000C0000, 0x00020000, 0x000001C0,
        0x0000001F, 0x00000020, 0x0001FE00 },
      /* ARM_DP_RS */
      { 0xF0000000, 0x0E000000, 0x01F00000,
        0x000F0000, 0x0000F000, 0x00000F00,
        0x0000000F, 0x00000060, 0x00000090 },
      /* ARM_DP_RE */
      { 0xF0000000, 0x0E000000, 0x01F00000,
        0x000F0000, 0x0000F000, 0x00000C00,
        0x0000000F, 0x00000300, 0x000000F0 },
      /* ARM_DP_I */
      { 0xF0000000, 0x0E000000, 0x01F00000,
        0x000F0000, 0x0000F000, 0x00000000,
        0x00000000, 0x00000F00, 0x000000FF },
      /* ARM_LS_IO */
      { 0xF0000000, 0x0E000000, 0x01F00000,
        0x000F0000, 0x0000F000, 0x00000000,
        0x00000000, 0x00000000, 0x00000FFF },
      /* ARM_LS_RO */
      { 0xF0000000, 0x0E000000, 0x01F00000,
        0x000F0000, 0x0000F000, 0x00000F80,
        0x0000000F, 0x00000070, 0x00000000 },
      /* ARM_LS_MULT */
      { 0xF0000000, 0x0E000000, 0x01F00000,
        0x000F0000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x0000FFFF },
      /* ARM_BRANCH */
      { 0xF0000000, 0x0E000000, 0x01000000,
        0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00FFFFFF },
      /* ARM_BRANCH_RS */
      { 0xF0000000, 0x0E000000, 0x01F00000,
        0x000F0000, 0x0000F000, 0x00000F00,
        0x0000000F, 0x000000F0, 0x00000000 },
      /* ARM_CP_F1 */
      { 0xF0000000, 0x0F000000, 0x00F00000,
        0x000F0000, 0x0000F000, 0x00000F00,
        0x0000000F, 0x00000010, 0x000000E0 },
      /* ARM_CP_F2 */
      { 0xF0000000, 0x0F000000, 0x00F00000,
        0x000F0000, 0x0000F000, 0x00000F00,
        0x00000000, 0x00000000, 0x000000FF },
      /* ARM_CP_F3 */
      { 0xF0000000, 0x0F000000, 0x00F00000,
        0x000F0000, 0x0000F000, 0x00000F00,
        0x0000000F, 0x00000000, 0x000000F0 },
      /* ARM_CP_F4 */
      { 0xF0000000, 0x0F000000, 0x00E00000,
        0x000F0000, 0x0000F000, 0x00000F00,
        0x0000000F, 0x00100010, 0x000000E0 },
    };
    return &(kArmInstMasks[type]);
  }
}

/*
 * *******************************************************
 * This section defines some useful routines that are needed during
 * both generation time and runtime.
 * *******************************************************
 */

/*
 * Append a hex value into the buffer at the given cursor position.
 * When the buffer fills, no additional text is added to the buffer,
 * even though the cursor is incremented accordingly.
 */
static void HexAppend(char* buffer,
                      size_t buffer_size,
                      uint32_t value,
                      size_t* cursor) {
  char value_string[24];
  snprintf(value_string, sizeof(value_string), "0x%x", value);
  FormatAppend(buffer, buffer_size, value_string, cursor);
}

/* Append a hex value that is one word wide, into the buffer at
 * the given cursor position. When the buffer fills, no additional
 * text is added to the buffer, even though the cursor is incremented
 * accordingly.
 */
static void HexWordAppend(char* buffer,
                          size_t buffer_size,
                          uint32_t value,
                          size_t* cursor) {
  char value_string[24];
  snprintf(value_string, sizeof(value_string), "0x%08x", value);
  FormatAppend(buffer, buffer_size, value_string, cursor);
}

/*
 * Appends the hex bit pattern defined by value into the buffer
 * at the cursor position. When the buffer fills, no additional text
 * is added to the buffer, even though the cursor is incremented
 * accordingly.
 */
static void HexPatternAppend(char* buffer,
                             size_t buffer_size,
                             int32_t value,
                             size_t* cursor) {
  switch (value) {
    case NOT_USED:
      FormatAppend(buffer, buffer_size, "NOT_USED", cursor);
      break;
    case ANY:
      FormatAppend(buffer, buffer_size, "ANY", cursor);
      break;
    case NON_ZERO:
      FormatAppend(buffer, buffer_size, "NON_ZERO", cursor);
      break;
    case CONTAINS_ZERO:
      FormatAppend(buffer, buffer_size, "CONTAINS_ZERO", cursor);
      break;
    default:
      HexAppend(buffer, buffer_size, value, cursor);
      break;
  }
}

/* Appends an instruction value pattern into the buffer at the cursor position.
 * When the buffer fills, no additional text is added to the buffer, even
 * though the cursor is incremented accordingly.
 *
 * arguments:
 *    buffer - The buffer to append the pattern to.
 *    buffer_size - The size of the buffer.
 *    name - The prefix (i.e. name) of the instruction value being printed.
 *    value - The expected value for the instruction value.
 *    mask - The mask to get the instruction value from an instruction.
 *    cursor - The index into the buffer, where the next character
 *         should be added, if there is sufficient room in the buffer.
 */
static void InstValuePatternAppend(char* buffer,
                                   size_t buffer_size,
                                   char* name,
                                   int32_t value,
                                   int32_t mask,
                                   size_t* cursor) {
  FormatAppend(buffer, buffer_size, name, cursor);
  FormatAppend(buffer, buffer_size, "=", cursor);
  HexPatternAppend(buffer, buffer_size, value, cursor);
  FormatAppend(buffer, buffer_size, " [", cursor);
  HexWordAppend(buffer, buffer_size, mask, cursor);
  FormatAppend(buffer, buffer_size, "]", cursor);
}

typedef struct {
  const InstValues* mask;
  const InstValues* values;
} InstMaskAndValues;

/*
 * Prints out the specified field of the given mask/values InstValues pair,
 * into the buffer at the cursor position.
 *
 * %c - cond field.
 * %p - prefix field.
 * %o - opcode field
 * %1 - arg1 field
 * %2 - arg2 field
 * %3 - arg3 field
 * %4 - arg4 field
 * %s - shift field
 * %i - immediate field
 */
static Bool InstMaskAndValuesDirective(char directive,
                                       char* buffer,
                                       size_t buffer_size,
                                       void* data,
                                       size_t* cursor) {
  InstMaskAndValues* value = (InstMaskAndValues*) data;
  switch (directive) {
    case 'c':
      InstValuePatternAppend(buffer,
                             buffer_size,
                             "cond",
                             value->values->cond,
                             value->mask->cond,
                             cursor);
      return TRUE;
    case 'p':
      InstValuePatternAppend(buffer,
                             buffer_size,
                             "prefix",
                             value->values->prefix,
                             value->mask->prefix,
                             cursor);
      return TRUE;
    case 'o':
      InstValuePatternAppend(buffer,
                             buffer_size,
                             "opcode",
                             value->values->opcode,
                             value->mask->opcode,
                             cursor);
      return TRUE;
    case '1':
      InstValuePatternAppend(buffer,
                             buffer_size,
                             "arg1",
                             value->values->arg1,
                             value->mask->arg1,
                             cursor);
      return TRUE;
    case '2':
      InstValuePatternAppend(buffer,
                             buffer_size,
                             "arg2",
                             value->values->arg2,
                             value->mask->arg2,
                             cursor);
      return TRUE;
    case '3':
      InstValuePatternAppend(buffer,
                             buffer_size,
                             "arg3",
                             value->values->arg3,
                             value->mask->arg3,
                             cursor);
      return TRUE;
    case '4':
      InstValuePatternAppend(buffer,
                             buffer_size,
                             "arg4",
                             value->values->arg4,
                             value->mask->arg4,
                             cursor);
      return TRUE;
    case 's':
      InstValuePatternAppend(buffer,
                             buffer_size,
                             "shift",
                             value->values->shift,
                             value->mask->shift,
                             cursor);
    case 'i':
      InstValuePatternAppend(buffer,
                             buffer_size,
                             "immediate",
                             value->values->immediate,
                             value->mask->immediate,
                             cursor);
      return TRUE;
    default:
      return FALSE;
  }
}

void InstValuesAppend(char* buffer,
                      size_t buffer_size,
                      const InstValues* values,
                      ArmInstType inst_type,
                      size_t* cursor) {
  InstMaskAndValues value;
  value.mask = GetArmInstMasks(inst_type);
  value.values = values;
  FormatDataAppend(buffer,
                   buffer_size,
                   "{ %c, %p, %o,\n\t%1, %2, %3, %4,\n\t%s, %i}",
                   (void*)(&value),
                   InstMaskAndValuesDirective,
                   cursor);
}

Bool DescribeInstValues(char* buffer,
                        size_t buffer_size,
                        const InstValues* values,
                        ArmInstType inst_type) {
  size_t cursor = 0;
  InstValuesAppend(buffer, buffer_size, values, inst_type, &cursor);
  return cursor < buffer_size;
}

/*
 * Append the given value into the buffer at the given cursor position.
 * When the buffer fills, no additional text is added to the buffer,
 * even though the cursor is incremented accordingly.
 */
void ValueAppend(char* buffer,
                 size_t buffer_size,
                 int32_t value,
                 size_t* cursor) {
  char value_string[24];
  snprintf(value_string, sizeof(value_string), "%d", value);
  FormatAppend(buffer, buffer_size, value_string, cursor);
}

uint32_t GetBitMask(uint32_t index) {
  if (index < 32) {
    return (((uint32_t) 1) << index);
  } else {
    /* This should not happen!! */
    fprintf(stderr,
            "GetBitMask(%u) not defined, assuming zero.\n",
            index);
    return 0;
  }
}

Bool GetBit(uint32_t value, uint32_t index) {
  return 0 != (value & GetBitMask(index));
}

void SetBit(uint32_t index, Bool bit_value, uint32_t* value) {
  uint32_t mask = GetBitMask(index);
  if (bit_value) {
    *value |= mask;
  } else {
    *value &= ~mask;
  }
}

int PosOfLowestBitSet(uint32_t mask) {
  int shift = 0;
  if (0 == mask) return 0;
  while (0 == (mask & 1)) {
    mask >>= 1;
    ++shift;
  }
  return shift;
}

const char* GetBoolName(Bool b) {
  return b ? "TRUE" : "FALSE";
}
