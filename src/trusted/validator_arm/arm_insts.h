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
 * Defines the API to modeled ARM instructions, as well as support routines
 * needed during both generation and runtime.
 */

#ifndef NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_INSTS_H__
#define NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_INSTS_H__

#include "native_client/src/shared/utils/types.h"

EXTERN_C_BEGIN

#include <stdio.h>

/* Forward declarations. */
struct NcDecodedInstruction;

/* Defines the format to print out conditions. */
extern Bool FLAGS_name_cond;

/* Maximum character size to use to generate instruction formats etc. */
#define INST_BUFFER_SIZE 1024

/* Enumerated type for kinds of ARM instructions */
typedef enum {
  ARM_UNDEFINED,        /* uninitialized space; should never happen */
  ARM_ILLEGAL,          /* not allowed in NaCL */
  ARM_INVALID,          /* not valid in any known ARM instruction */
  /* For categorizations, see ARM 14128.pdf */
  ARM_DP_IS,            /* ARM data processing, immediate shift */
  ARM_MISC_SAT,         /* ARM miscellaneous saturating instructions */
  ARM_CPS,              /* ARM cps instruction. */
  ARM_DP_RS,            /* ARM data processing, register shift */
  ARM_DP_RE,            /* ARM data processing, register extend */
  ARM_DP_I,             /* ARM data processing, immediate */
  ARM_LS_IO,            /* ARM load/store immediate offset */
  ARM_LS_RO,            /* ARM load/store register offset */
  ARM_LS_MULT,          /* ARM load/store multiple */
  ARM_BRANCH,           /* ARM branch and branch with link */
  ARM_BRANCH_RS,        /* ARM branch to register shift. */
  ARM_CP_F1,            /* ARM coprocessor, form 1 */
  ARM_CP_F2,            /* ARM coprocessor, form 2. */
  ARM_CP_F3,            /* ARM coprocessor, form 3. */
  ARM_CP_F4,            /* ARM coprocessor, form 4. */
  ARM_INST_TYPE_SIZE    /* Special end of list marker. */
} ArmInstType;

/*
 * Returns a print name for the given type.
 */
const char* GetArmInstTypeName(ArmInstType type);

/* Enumerated type for each ARM instruction. */
typedef enum {
  ARM_UNKNOWN_INST,         /* special start marker. */
  ARM_BRANCH_INST,
  ARM_BRANCH_AND_LINK,
  ARM_BRANCH_WITH_LINK_AND_EXCHANGE_1,
  ARM_BRANCH_WITH_LINK_AND_EXCHANGE_2,
  ARM_BRANCH_AND_EXCHANGE,
  ARM_BRANCH_AND_EXCHANGE_TO_JAZELLE,
  ARM_AND,
  ARM_EOR,
  ARM_SUB,
  ARM_RSB,
  ARM_ADD,
  ARM_ADC,
  ARM_SBC,
  ARM_RSC,
  ARM_TST,
  ARM_TEQ,
  ARM_CMP,
  ARM_CMN,
  ARM_ORR,
  ARM_MOV,
  ARM_BIC,
  ARM_MVN,
  ARM_MLA,
  ARM_MUL,
  ARM_SMLA_XY,
  ARM_SMUAD,
  ARM_SMUADX,
  ARM_SMLAD,
  ARM_SMLADX,
  ARM_SMLAL,
  ARM_SMLAL_XY,
  ARM_SMLALD,
  ARM_SMLALDX,
  ARM_SMLAWB,
  ARM_SMLAWT,
  ARM_SMUSD,
  ARM_SMUSDX,
  ARM_SMLSD,
  ARM_SMLSDX,
  ARM_SMLSLD,
  ARM_SMLSLDX,
  ARM_SMMUL,
  ARM_SMMULR,
  ARM_SMMLA,
  ARM_SMMLAR,
  ARM_SMMLS,
  ARM_SMMLSR,
  ARM_SMUL_XY,
  ARM_SMULL,
  ARM_SMULWB,
  ARM_SMULWT,
  ARM_UMAAL,
  ARM_UMLAL,
  ARM_UMULL,
  ARM_QADD,
  ARM_QADD16,
  ARM_QADD8,
  ARM_QADDSUBX,
  ARM_QDADD,
  ARM_QSUB,
  ARM_QSUB16,
  ARM_QSUB8,
  ARM_QSUBADDX,
  ARM_QDSUB,
  ARM_SADD16,
  ARM_SADD8,
  ARM_SADDSUBX,
  ARM_SSUB16,
  ARM_SSUB8,
  ARM_SSUBADDX,
  ARM_SHADD16,
  ARM_SHADD8,
  ARM_SHADDSUBX,
  ARM_SHSUB16,
  ARM_SHSUB8,
  ARM_SHSUBADDX,
  ARM_UADD16,
  ARM_UADD8,
  ARM_UADDSUBX,
  ARM_USUB16,
  ARM_USUB8,
  ARM_USUBADDX,
  ARM_UHADD16,
  ARM_UHADD8,
  ARM_UHADDSUBX,
  ARM_UHSUB16,
  ARM_UHSUB8,
  ARM_UHSUBADDX,
  ARM_UQADD16,
  ARM_UQADD8,
  ARM_UQADDSUBX,
  ARM_UQSUB16,
  ARM_UQSUB8,
  ARM_UQSUBADDX,
  ARM_SXTAB16,
  ARM_SXTAB,
  ARM_SXTAH,
  ARM_SXTB16,
  ARM_SXTB,
  ARM_SXTH,
  ARM_UXTAB16,
  ARM_UXTAB,
  ARM_UXTAH,
  ARM_UXTB16,
  ARM_UXTB,
  ARM_UXTH,
  ARM_CLZ,
  ARM_USAD8,
  ARM_USADA8,
  ARM_PKHBT_1,
  ARM_PKHBT_1_LSL,
  ARM_PKHBT_2,
  ARM_PKHBT_2_ASR,
  ARM_REV,
  ARM_REV16,
  ARM_REVSH,
  ARM_SEL,
  ARM_SSAT,
  ARM_SSAT_LSL,
  ARM_SSAT_ASR,
  ARM_SSAT16,
  ARM_USAT,
  ARM_USAT_LSL,
  ARM_USAT_ASR,
  ARM_USAT16,
  ARM_MRS_CPSR,
  ARM_MRS_SPSR,
  ARM_MSR_CPSR_IMMEDIATE,
  ARM_MSR_CPSR_REGISTER,
  ARM_MSR_SPSR_IMMEDIATE,
  ARM_MSR_SPSR_REGISTER,
  ARM_CPS_FLAGS,
  ARM_CPS_FLAGS_MODE,
  ARM_CPS_MODE,
  ARM_SETEND_BE,
  ARM_SETEND_LE,
  ARM_LDR,
  ARM_LDREX,
  ARM_STR,
  ARM_STREX,
  ARM_STR_HALFWORD,
  ARM_LDR_DOUBLEWORD,
  ARM_STR_DOUBLEWORD,
  ARM_LDR_UNSIGNED_HALFWORD,
  ARM_LDR_SIGNED_BYTE,
  ARM_LDR_SIGNED_HALFWORD,
  ARM_LDM_1,
  ARM_LDM_1_MODIFY,
  ARM_LDM_2,
  ARM_LDM_3,
  ARM_STM_1,
  ARM_STM_1_MODIFY,
  ARM_STM_2,
  ARM_SWP,
  ARM_SWPB,
  ARM_BKPT,
  ARM_SWI,
  ARM_CDP,
  ARM_MCR,
  ARM_MCRR,
  ARM_MRC,
  ARM_MRRC,
  ARM_LDC,
  ARM_STC,
  ARM_INST_KIND_SIZE    /* Special end of list marker. */
} ArmInstKind;

/* Returns a print name for the given type. */
const char* GetArmInstKindName(ArmInstKind kind);

/* Define the types of common addressing modes in instructions. */
typedef enum {
  ARM_ADDRESSING_UNDEFINED,
  /* Data processing modes*/
  ARM_DATA_IMMEDIATE,
  ARM_DATA_REGISTER,
  ARM_DATA_RRX,
  /* Load store modes. */
  ARM_LS_IMMEDIATE,
  ARM_LS_REGISTER,
  ARM_LS_SCALED_REGISTER,
  /* Miscellaneous load store modes. */
  ARM_MLS_IMMEDIATE,
  ARM_MLS_REGISTER,
  /* Load store coprocessor modes. */
  ARM_LSC_IMMEDIATE,
  ARM_LSC_UNINDEXED,
  ARM_ADDRESSING_SIZE  /* Special end of list marker. */
} ArmAddressingMode;

/* Returns a print name for the given type. */
const char* GetArmAddressingModeName(ArmAddressingMode mode);

/* Define the types of common subaddressing modes used in instructions. */
typedef enum {
  ARM_SUBADDRESSING_UNDEFINED,
  ARM_SUBADDRESSING_LSL,
  ARM_SUBADDRESSING_LSR,
  ARM_SUBADDRESSING_ASR,
  ARM_SUBADDRESSING_ROR,
  ARM_SUBADDRESSING_RRX,
  ARM_SUBADDRESSING_SIZE  /* Special end of list marker. */
} ArmSubaddressingMode;

/* Returns a print name for the given type. */
const char* GetArmSubaddressingModeName(ArmSubaddressingMode mode);

/* Define the types of common (resgister) indexing used in instructions. */
typedef enum {
  ARM_INDEXING_UNDEFINED,
  ARM_INDEXING_OFFSET,
  ARM_INDEXING_PRE,
  ARM_INDEXING_POST,
  ARM_INDEXING_SIZE  /* Special end of list marker. */
} ArmIndexingMode;

const char* GetArmIndexingModeName(ArmIndexingMode mode);

/*
 * Define a combination of access modes. One (unique)
 * constant will be defined for each possible access
 * mode, so that addresss compare can be used to
 * compare access modes. On the other hand, multiple
 * values are associated with each access mode so that
 * routines can base their answer based on the values
 * individual fields.
 */
typedef struct {
  ArmAddressingMode address_mode;
  ArmSubaddressingMode address_submode;
  ArmIndexingMode index_mode;
} ArmAccessMode;

/* NOTE: The names of each access mode must be addded
 * to header file arm_inst_modeling.h.
 */

/* { ARM_ADDRESSING_UNDEFINED,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_UNDEFINED }
*/
extern const ArmAccessMode kArmUndefinedAccessMode;

/* { ARM_DATA_IMMEDIATE,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_UNDEFINED }
*/
extern const ArmAccessMode kArmDataImmediate;

/* { ARM_DATA_IMMEDIATE,
     ARM_SUBADDRESSING_LSL,
     ARM_INDEXING_UNDEFINED }
*/
extern const ArmAccessMode kArmDataLslImmediate;

/* { ARM_DATA_IMMEDIATE,
     ARM_SUBADDRESSING_LSR,
     ARM_INDEXING_UNDEFINED }
*/
extern const ArmAccessMode kArmDataLsrImmediate;

/* { ARM_DATA_IMMEDIATE,
     ARM_SUBADDRESSING_ASR,
     ARM_INDEXING_UNDEFINED }
*/
extern const ArmAccessMode kArmDataAsrImmediate;

/* { ARM_DATA_IMMEDIATE,
     ARM_SUBADDRESSING_ROR,
     ARM_INDEXING_UNDEFINED }
*/
extern const ArmAccessMode kArmDataRorImmediate;

/* { ARM_DATA_RRX,
     ARM_SUBADDRESSING_RRX,
     ARM_INDEXING_UNDEFINED }
*/
extern const ArmAccessMode kArmDataRrx;

/* { ARM_DATA_REGISTER,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_UNDEFINED }
*/
extern const ArmAccessMode kArmDataRegister;

/* { ARM_DATA_REGISTER,
     ARM_SUBADDRESSING_LSL,
     ARM_INDEXING_UNDEFINED }
*/
extern const ArmAccessMode kArmDataLslRegister;

/* { ARM_DATA_REGISTER,
     ARM_SUBADDRESSING_LSR,
     ARM_INDEXING_UNDEFINED }
*/
extern const ArmAccessMode kArmDataLsrRegister;

/* { ARM_DATA_REGISTER,
     ARM_SUBADDRESSING_ASR,
     ARM_INDEXING_UNDEFINED }
*/
extern const ArmAccessMode kArmDataAsrRegister;

/* { ARM_DATA_REGISTER,
     ARM_SUBADDRESSING_ROR,
     ARM_INDEXING_UNDEFINED }
*/
extern const ArmAccessMode kArmDataRorRegister;

/* { ARM_LS_IMMEDIATE,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_OFFSET }
*/
extern const ArmAccessMode kArmLsImmediateOffset;

/* { ARM_LS_REGISTER,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_OFFSET }
*/
extern const ArmAccessMode kArmLsRegisterOffset;

/* { ARM_LS_SCALED_REGISTER,
     ARM_SUBADDRESSING_LSL,
     ARM_INDEXING_OFFSET }
*/
extern const ArmAccessMode kArmLsScaledRegisterLslOffset;

/* { ARM_LS_SCALED_REGISTER,
     ARM_SUBADDRESSING_LSR,
     ARM_INDEXING_OFFSET }
*/
extern const ArmAccessMode kArmLsScaledRegisterLsrOffset;

/* { ARM_LS_SCALED_REGISTER,
     ARM_SUBADDRESSING_ASR,
     ARM_INDEXING_OFFSET }
*/
extern const ArmAccessMode kArmLsScaledRegisterAsrOffset;

/* { ARM_LS_SCALED_REGISTER,
     ARM_SUBADDRESSING_ROR,
     ARM_INDEXING_OFFSET }
*/
extern const ArmAccessMode kArmLsScaledRegisterRorOffset;

/* { ARM_LS_SCALED_REGISTER,
     ARM_SUBADDRESSING_RRX,
     ARM_INDEXING_OFFSET }
*/
extern const ArmAccessMode kArmLsScaledRegisterRrxOffset;

/* { ARM_LS_IMMEDIATE,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_PRE }
*/
extern const ArmAccessMode kArmLsImmediatePreIndexed;

/* { ARM_LS_REGISTER,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_PRE }
*/
extern const ArmAccessMode kArmLsRegisterPreIndexed;

/* { ARM_LS_SCALED_REGISTER,
     ARM_SUBADDRESSING_LSL,
     ARM_INDEXING_PRE }
*/
extern const ArmAccessMode kArmLsScaledRegisterLslPreIndexed;

/* { ARM_LS_SCALED_REGISTER,
     ARM_SUBADDRESSING_LSR,
     ARM_INDEXING_PRE }
*/
extern const ArmAccessMode kArmLsScaledRegisterLsrPreIndexed;

/* { ARM_LS_SCALED_REGISTER,
     ARM_SUBADDRESSING_ASR,
     ARM_INDEXING_PRE }
*/
extern const ArmAccessMode kArmLsScaledRegisterAsrPreIndexed;

/* { ARM_LS_SCALED_REGISTER,
     ARM_SUBADDRESSING_ROR,
     ARM_INDEXING_PRE }
*/
extern const ArmAccessMode kArmLsScaledRegisterRorPreIndexed;

/* { ARM_LS_SCALED_REGISTER,
     ARM_SUBADDRESSING_RRX,
     ARM_INDEXING_PRE }
*/
extern const ArmAccessMode kArmLsScaledRegisterRrxPreIndexed;

/* { ARM_LS_IMMEDIATE,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_POST }
*/
extern const ArmAccessMode kArmLsImmediatePostIndexed;

/* { ARM_LS_REGISTER,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_POST }
*/
extern const ArmAccessMode kArmLsRegisterPostIndexed;

/* { ARM_LS_SCALED_REGISTER,
     ARM_SUBADDRESSING_LSL,
     ARM_INDEXING_POST }
*/
extern const ArmAccessMode kArmLsScaledRegisterLslPostIndexed;

/* { ARM_LS_SCALED_REGISTER,
     ARM_SUBADDRESSING_LSR,
     ARM_INDEXING_POST }
*/
extern const ArmAccessMode kArmLsScaledRegisterLsrPostIndexed;

/* { ARM_LS_SCALED_REGISTER,
     ARM_SUBADDRESSING_ASR,
     ARM_INDEXING_POST }
*/
extern const ArmAccessMode kArmLsScaledRegisterAsrPostIndexed;

/* { ARM_LS_SCALED_REGISTER,
     ARM_SUBADDRESSING_ROR,
     ARM_INDEXING_POST }
*/
extern const ArmAccessMode kArmLsScaledRegisterRorPostIndexed;

/* { ARM_LS_SCALED_REGISTER,
     ARM_SUBADDRESSING_RRX,
     ARM_INDEXED_POST }
*/
extern const ArmAccessMode kArmLsScaledRegisterRrxPostIndexed;

/* { ARM_MLS_IMMEDIATE,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_OFFSET }
*/
extern const ArmAccessMode kArmMlsImmediateOffset;

/* { ARM_MLS_REGISTER,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_OFFSET }
*/
extern const ArmAccessMode kArmMlsRegisterOffset;

/* { ARM_MLS_IMMEDIATE,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_PRE }
*/
extern const ArmAccessMode kArmMlsImmediatePreIndexed;

/* { ARM_MLS_REGISTER,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_PRE }
*/
extern const ArmAccessMode kArmMlsRegisterPreIndexed;

/* { ARM_MLS_IMMEDIATE,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_POST }
*/
extern const ArmAccessMode kArmMlsImmediatePostIndexed;

/* { ARM_MLS_REGISTER,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_POST }
*/
extern const ArmAccessMode kArmMlsRegisterPostIndexed;

/* { ARM_LSC_IMMEDIATE,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_OFFSET  }
*/
extern const ArmAccessMode kArmLscImmediateOffset;

/* { ARM_LSC_IMMEDIATE,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_PRE }
*/
extern const ArmAccessMode kArmLscImmediatePreIndexed;

/* { ARM_LSC_IMMEDIATE,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_POST }
*/
extern const ArmAccessMode kArmLscImmediatePostIndexed;

/* { ARM_LSC_UNINDEXED,
     ARM_SUBADDRESSING_UNDEFINED,
     ARM_INDEXING_UNDEFINED }
*/
extern const ArmAccessMode kArmLscUnindexed;

/*
 * The following define standard regsiter indices, and some
 * useful bit indices used by arm.
 */

/* Models the index for the link register. */
#define LINK_INDEX 14

/* Models the index for the PC. */
#define PC_INDEX 15

/* Models the index for the SP. */
#define SP_INDEX 13

/* Models the (standard) location for the S bit (used
 * in many (but not all) instructions to define if the
 * CPSR is modified by the instruction.
 */
#define ARM_S_BIT 20

/* Models the (standard) location for the W bit (used
 * in many (but not all) instructions that a write update
 * happens on the base register.
 */
#define ARM_W_BIT 21

/*
 * The following define possible multiple value bit patterns.
 */

/*
 * Defines pattern for instruction value that isn't used.
 */
#define NOT_USED (-1)

/*
 * Defines pattern for instruction value that can be any valid
 * (masked) value.
 */
#define ANY (-2)

/*
 * Defines pattern for instruction value that must be non-zero.
 */
#define NON_ZERO (-3)

/*
 * Defines pattern for instruction value that must not be all ones.
 */
#define CONTAINS_ZERO (-4)

/*
 * Defines values (or masks) to extract out data fields within an instruction.
 */
typedef struct InstValues {
  int32_t cond;           /* value to extract condition codes */
                          /* (typically bits 28-31) */
  int32_t prefix;         /* value to extract prefix of opcode */
                          /* (typically 25-27 or 24-27) */
  int32_t opcode;         /* value to extract opcode. (typically bits 12-24).*/
  int32_t arg1;           /* value to get arg1 (Rn) (typically bits 16-19) */
  int32_t arg2;           /* value to get arg2 (Rd) (typically bits 12-15) */
  int32_t arg3;           /* value to get arg3 (Rs) (typically bits 8-11) */
  int32_t arg4;           /* value to get arg4 (Rm) (typically bits 0-3) */
  int32_t shift;          /* value for shift value (typically 7-11 or 5-6) */
  int32_t immediate;      /* value for immediate value (typically 0-7 or 0-11)*/
                          /* Also used for register list or 24-bit offxosets. */
                          /* Also used to check required bit patterns within */
                          /* instruction. */
} InstValues;

/*
 * Defines a special mask for instruction values where we don't care
 * about any of the  extracted values.
 */
#define ARM_DONT_CARE_MASK { 0, 0, 0, 0, 0, 0, 0, 0, 0 }

/*
 * Define function to check (other) properties of decoding the given
 * instruction, returning true iff they are met.
 */
typedef Bool (*MatchCheckFcn)(struct NcDecodedInstruction* instruction);

/*
 * Define information used to model instructions
 */
typedef struct OpInfo {
  const char* name;            /* Printable name for the operation. */
  ArmInstKind inst_kind;       /* The kind of instruction. */
  const ArmAccessMode* inst_access;  /* Access pattern of instruction. */
  ArmInstType inst_type;       /* Type of instruction */
  Bool arm_safe;               /* true iff allowed in native client */
  const char *describe_format; /* Print format for disassembled text. */
  int num_bytes;               /* number of bytes int the instruction. */
  MatchCheckFcn check_fcn;     /* When non-null, function to call to check
                                * additional properties.
                                */
  InstValues expected_values;  /* Extracted values based on corresponding masks
                                * for the type of instruction.
                                */
} OpInfo;

/*
 * Define the expanded version of an instruction (from the corresponding
 * compressed instruction word).
 */
typedef struct NcDecodedInstruction {
  uint32_t vpc;               /* Virtual value of program counter. */
  /* Instruction stored at value of vpc. Note: if the instruction is
   * less than 4 bytes, the instruction is right padded with zeroes.
   */
  uint32_t inst;
  const OpInfo* matched_inst; /* Matched instruction information for
                               * instruction
                               */
  InstValues values;          /* expanded values for instruction. */
} NcDecodedInstruction;

/* Defines the length of a word on ARM. */
#define ARM_WORD_LENGTH 4

/*
 * Define maximum number of instruction forms that can be modeled.
 */
#define MAX_ARM_INSTRUCTION_SET_SIZE 1024

/*
 * Models a list of known instructions.
 */
typedef struct ArmInstructionSet {
  /* The number of known instructions */
  int size;
  /* The information for the i-th instruction in the instruction set. */
  const OpInfo* instructions[MAX_ARM_INSTRUCTION_SET_SIZE];
} ArmInstructionSet;

/*
 * Define the instruction set. Not built until BuildArmInstructionSet called.
 */
extern ArmInstructionSet arm_instruction_set;

/*
 * *******************************************************
 * This section defines some useful routines that are needed during
 * both generation time and runtime.
 * *******************************************************
 */

/*
 * Append the given value into the buffer at the given cursor position.
 * When the buffer fills, no additional text is added to the buffer,
 * even though the cursor is incremented accordingly.
 */
void ValueAppend(char* buffer,
                 size_t buffer_size,
                 int32_t value,
                 size_t* cursor);

/*
 * Appends the given instruction values (assuming the corresponding
 * instruction type) to the given buffer, at the given cursor position.
 * When the buffer fills, no additional text is added to the buffer,
 * even though the cursor is incremented accordingly.
 *
 * Note: Buffer overflow occurs iff the cursor is greater than or
 * equal to the buffer size.
 */
void InstValuesAppend(char* buffer,
                      size_t buffer_size,
                      const InstValues* values,
                      ArmInstType inst_type,
                      size_t* cursor);

/*
 * Prints a description of the given instruction values (assuming the
 * corresponding instruction type) to the given buffer. Returns
 * true if the buffer doesn't overflow.
 */
Bool DescribeInstValues(char* buffer,
                        size_t buffer_size,
                        const InstValues* values,
                        ArmInstType inst_type);

/*
 * Gets the corresponding set of masks associated with the given
 * instruction type.
 */
const InstValues* GetArmInstMasks(ArmInstType type);

/*
 * Returns a mask to get the bit in the corresponding index
 * of a (uint32_t) word.
 */
uint32_t GetBitMask(uint32_t index);

/*
 * Returns the indexed bit of the given value.
 */
Bool GetBit(uint32_t value, uint32_t index);

/*
 * Sets the indexed bit of the given index, to the given bit value.
 */
void SetBit(uint32_t index, Bool bit_value, uint32_t* value);

/*
 * Returns the number of bits to shift the mask value so that the
 * rightmost 1 bit is all the way to the right.
 */
int PosOfLowestBitSet(uint32_t mask);

/* Return a printable name for a boolean value. */
const char* GetBoolName(Bool b);

EXTERN_C_END

#endif /* NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARMINSTS_H__ */
