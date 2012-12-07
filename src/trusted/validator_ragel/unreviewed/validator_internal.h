/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file contains common parts of x86-32 and x86-64 internals (inline
 * functions and defines).
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_VALIDATOR_INTERNAL_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_VALIDATOR_INTERNAL_H_

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/decoding.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/validator.h"

/* Maximum set of R-DFA allowable CPUID features.  */
extern const NaClCPUFeaturesX86 kValidatorCPUIDFeatures;

/* Macroses to suppport CPUID handling.  */
#define SET_CPU_FEATURE(F) \
  if (!(F##_Allowed)) { \
    instruction_info_collected |= UNRECOGNIZED_INSTRUCTION; \
  } \
  if (!(F)) { \
    instruction_info_collected |= CPUID_UNSUPPORTED_INSTRUCTION; \
  }
#define CPUFeature_3DNOW    cpu_features->data[NaClCPUFeatureX86_3DNOW]
/*
 * AMD documentation claims it's always available if CPUFeature_LM is present,
 * But Intel documentation does not even mention it!
 * Keep it as 3DNow! instruction.
 */
#define CPUFeature_3DPRFTCH CPUFeature_3DNOW || CPUFeature_PRE
#define CPUFeature_AES      cpu_features->data[NaClCPUFeatureX86_AES]
#define CPUFeature_AESAVX   CPUFeature_AES && CPUFeature_AVX
#define CPUFeature_AVX      cpu_features->data[NaClCPUFeatureX86_AVX]
#define CPUFeature_BMI1     cpu_features->data[NaClCPUFeatureX86_BMI1]
#define CPUFeature_CLFLUSH  cpu_features->data[NaClCPUFeatureX86_CLFLUSH]
#define CPUFeature_CLMUL    cpu_features->data[NaClCPUFeatureX86_CLMUL]
#define CPUFeature_CLMULAVX CPUFeature_CLMUL && CPUFeature_AVX
#define CPUFeature_CMOV     cpu_features->data[NaClCPUFeatureX86_CMOV]
#define CPUFeature_CMOVx87  CPUFeature_CMOV && CPUFeature_x87
#define CPUFeature_CX16     cpu_features->data[NaClCPUFeatureX86_CX16]
#define CPUFeature_CX8      cpu_features->data[NaClCPUFeatureX86_CX8]
#define CPUFeature_E3DNOW   cpu_features->data[NaClCPUFeatureX86_E3DNOW]
#define CPUFeature_EMMX     cpu_features->data[NaClCPUFeatureX86_EMMX]
#define CPUFeature_EMMXSSE  CPUFeature_EMMX || CPUFeature_SSE
#define CPUFeature_F16C     cpu_features->data[NaClCPUFeatureX86_F16C]
#define CPUFeature_FMA      cpu_features->data[NaClCPUFeatureX86_FMA]
#define CPUFeature_FMA4     cpu_features->data[NaClCPUFeatureX86_FMA4]
#define CPUFeature_FXSR     cpu_features->data[NaClCPUFeatureX86_FXSR]
#define CPUFeature_LAHF     cpu_features->data[NaClCPUFeatureX86_LAHF]
#define CPUFeature_LM       cpu_features->data[NaClCPUFeatureX86_LM]
#define CPUFeature_LWP      cpu_features->data[NaClCPUFeatureX86_LWP]
/*
 * We allow lzcnt unconditionally
 * See http://code.google.com/p/nativeclient/issues/detail?id=2869
 */
#define CPUFeature_LZCNT    TRUE
#define CPUFeature_MMX      cpu_features->data[NaClCPUFeatureX86_MMX]
#define CPUFeature_MON      cpu_features->data[NaClCPUFeatureX86_MON]
#define CPUFeature_MOVBE    cpu_features->data[NaClCPUFeatureX86_MOVBE]
#define CPUFeature_OSXSAVE  cpu_features->data[NaClCPUFeatureX86_OSXSAVE]
#define CPUFeature_POPCNT   cpu_features->data[NaClCPUFeatureX86_POPCNT]
#define CPUFeature_PRE      cpu_features->data[NaClCPUFeatureX86_PRE]
#define CPUFeature_SSE      cpu_features->data[NaClCPUFeatureX86_SSE]
#define CPUFeature_SSE2     cpu_features->data[NaClCPUFeatureX86_SSE2]
#define CPUFeature_SSE3     cpu_features->data[NaClCPUFeatureX86_SSE3]
#define CPUFeature_SSE41    cpu_features->data[NaClCPUFeatureX86_SSE41]
#define CPUFeature_SSE42    cpu_features->data[NaClCPUFeatureX86_SSE42]
#define CPUFeature_SSE4A    cpu_features->data[NaClCPUFeatureX86_SSE4A]
#define CPUFeature_SSSE3    cpu_features->data[NaClCPUFeatureX86_SSSE3]
#define CPUFeature_TBM      cpu_features->data[NaClCPUFeatureX86_TBM]
#define CPUFeature_TSC      cpu_features->data[NaClCPUFeatureX86_TSC]
/*
 * We allow tzcnt unconditionally
 * See http://code.google.com/p/nativeclient/issues/detail?id=2869
 */
#define CPUFeature_TZCNT    TRUE
#define CPUFeature_x87      cpu_features->data[NaClCPUFeatureX86_x87]
#define CPUFeature_XOP      cpu_features->data[NaClCPUFeatureX86_XOP]

#define CPUFeature_3DNOW_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_3DNOW]
/*
 * AMD documentation claims it's always available if CPUFeature_LM is present,
 * But Intel documentation does not even mention it!
 * Keep it as 3DNow! instruction.
 */
#define CPUFeature_3DPRFTCH_Allowed \
  CPUFeature_3DNOW_Allowed || CPUFeature_PRE_Allowed
#define CPUFeature_AES_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_AES]
#define CPUFeature_AESAVX_Allowed \
  CPUFeature_AES_Allowed && CPUFeature_AVX_Allowed
#define CPUFeature_AVX_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_AVX]
#define CPUFeature_BMI1_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_BMI1]
#define CPUFeature_CLFLUSH_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_CLFLUSH]
#define CPUFeature_CLMUL_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_CLMUL]
#define CPUFeature_CLMULAVX_Allowed \
  CPUFeature_CLMUL_Allowed && CPUFeature_AVX_Allowed
#define CPUFeature_CMOV_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_CMOV]
#define CPUFeature_CMOVx87_Allowed \
  CPUFeature_CMOV_Allowed && CPUFeature_x87_Allowed
#define CPUFeature_CX16_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_CX16]
#define CPUFeature_CX8_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_CX8]
#define CPUFeature_E3DNOW_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_E3DNOW]
#define CPUFeature_EMMX_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_EMMX]
#define CPUFeature_EMMXSSE_Allowed \
  CPUFeature_EMMX_Allowed || CPUFeature_SSE_Allowed
#define CPUFeature_F16C_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_F16C]
#define CPUFeature_FMA_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_FMA]
#define CPUFeature_FMA4_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_FMA4]
#define CPUFeature_FXSR_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_FXSR]
#define CPUFeature_LAHF_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_LAHF]
#define CPUFeature_LM_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_LM]
#define CPUFeature_LWP_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_LWP]
/*
 * We allow lzcnt unconditionally
 * See http://code.google.com/p/nativeclient/issues/detail?id=2869
 */
#define CPUFeature_LZCNT_Allowed TRUE
#define CPUFeature_MMX_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_MMX]
#define CPUFeature_MON_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_MON]
#define CPUFeature_MOVBE_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_MOVBE]
#define CPUFeature_OSXSAVE_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_OSXSAVE]
#define CPUFeature_POPCNT_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_POPCNT]
#define CPUFeature_PRE_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_PRE]
#define CPUFeature_SSE_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_SSE]
#define CPUFeature_SSE2_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_SSE2]
#define CPUFeature_SSE3_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_SSE3]
#define CPUFeature_SSE41_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_SSE41]
#define CPUFeature_SSE42_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_SSE42]
#define CPUFeature_SSE4A_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_SSE4A]
#define CPUFeature_SSSE3_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_SSSE3]
#define CPUFeature_TBM_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_TBM]
#define CPUFeature_TSC_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_TSC]
/*
 * We allow tzcnt unconditionally
 * See http://code.google.com/p/nativeclient/issues/detail?id=2869
 */
#define CPUFeature_TZCNT_Allowed TRUE
#define CPUFeature_x87_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_x87]
#define CPUFeature_XOP_Allowed \
  kValidatorCPUIDFeatures.data[NaClCPUFeatureX86_XOP]

/* Remember some information about instruction for further processing.  */
#define GET_REX_PREFIX() rex_prefix
#define SET_REX_PREFIX(P) rex_prefix = (P)
#define GET_VEX_PREFIX2() vex_prefix2
#define SET_VEX_PREFIX2(P) vex_prefix2 = (P)
#define GET_VEX_PREFIX3() vex_prefix3
#define SET_VEX_PREFIX3(P) vex_prefix3 = (P)
#define SET_MODRM_BASE(N) base = (N)
#define SET_MODRM_INDEX(N) index = (N)

enum operand_kind {
  OperandSandboxIrrelevant = 0,
  /*
   * Currently we do not distinguish 8bit and 16bit modifications from
   * OperandSandboxUnrestricted to match the behavior of the old validator.
   *
   * 8bit operands must be distinguished from other types because the REX prefix
   * regulates the choice between %ah and %spl, as well as %ch and %bpl.
   */
  OperandSandbox8bit,
  OperandSandboxRestricted,
  OperandSandboxUnrestricted
};

#define SET_OPERAND_NAME(N, S) operand_states |= ((S) << ((N) << 3))
#define SET_OPERAND_TYPE(N, T) SET_OPERAND_TYPE_ ## T(N)
#define SET_OPERAND_TYPE_OPERAND_TYPE_8_BIT(N) \
  operand_states |= OperandSandbox8bit << (5 + ((N) << 3))
#define SET_OPERAND_TYPE_OPERAND_TYPE_16_BIT(N) \
  operand_states |= OperandSandboxUnrestricted << (5 + ((N) << 3))
#define SET_OPERAND_TYPE_OPERAND_TYPE_32_BIT(N) \
  operand_states |= OperandSandboxRestricted << (5 + ((N) << 3))
#define SET_OPERAND_TYPE_OPERAND_TYPE_64_BIT(N) \
  operand_states |= OperandSandboxUnrestricted << (5 + ((N) << 3))
#define CHECK_OPERAND(N, S, T) \
  ((operand_states & (0xff << ((N) << 3))) == ((S | (T << 5)) << ((N) << 3)))

/* Ignore this information for now.  */
#define SET_DATA16_PREFIX(S)
#define SET_REPZ_PREFIX(S)
#define SET_REPNZ_PREFIX(S)
#define SET_MODRM_SCALE(S)
#define SET_DISP_PTR(P)
#define SET_IMM_PTR(P)
#define SET_IMM2_PTR(P)

/*
 * Collect information about anyfields (offsets and immediates).
 * Note: we use += below instead of |=. This means two immediate fields will
 * be treated as one.  It's not important for safety.
 */
#define SET_DISP_TYPE(T) SET_DISP_TYPE_##T
#define SET_DISP_TYPE_DISPNONE
#define SET_DISP_TYPE_DISP8 (instruction_info_collected += DISPLACEMENT_8BIT)
#define SET_DISP_TYPE_DISP32 (instruction_info_collected += DISPLACEMENT_32BIT)
#define SET_IMM_TYPE(T) SET_IMM_TYPE_##T
/* imm2 field is a flag, not accumulator, like with other immediates  */
#define SET_IMM_TYPE_IMM2 (instruction_info_collected |= IMMEDIATE_2BIT)
#define SET_IMM_TYPE_IMM8 (instruction_info_collected += IMMEDIATE_8BIT)
#define SET_IMM_TYPE_IMM16 (instruction_info_collected += IMMEDIATE_16BIT)
#define SET_IMM_TYPE_IMM32 (instruction_info_collected += IMMEDIATE_32BIT)
#define SET_IMM_TYPE_IMM64 (instruction_info_collected += IMMEDIATE_64BIT)
#define SET_IMM2_TYPE(T) SET_IMM2_TYPE_##T
#define SET_IMM2_TYPE_IMM8 \
    (instruction_info_collected += SECOND_IMMEDIATE_8BIT)
#define SET_IMM2_TYPE_IMM16 \
    (instruction_info_collected += SECOND_IMMEDIATE_16BIT)

#define BITMAP_WORD_NAME BITMAP_WORD_NAME1(NACL_HOST_WORDSIZE)
#define BITMAP_WORD_NAME1(size) BITMAP_WORD_NAME2(size)
#define BITMAP_WORD_NAME2(size) uint##size##_t

typedef BITMAP_WORD_NAME bitmap_word;

static INLINE bitmap_word *BitmapAllocate(size_t indexes) {
  bitmap_word *bitmap;
  size_t byte_count = ((indexes + NACL_HOST_WORDSIZE - 1) / NACL_HOST_WORDSIZE)*
                      sizeof *bitmap;
  bitmap = malloc(byte_count);
  if (bitmap != NULL) {
    memset(bitmap, 0, byte_count);
  }
  return bitmap;
}

static FORCEINLINE int BitmapIsBitSet(bitmap_word *bitmap, size_t index) {
  return (bitmap[index / NACL_HOST_WORDSIZE] &
                       (((bitmap_word)1) << (index % NACL_HOST_WORDSIZE))) != 0;
}

static FORCEINLINE void BitmapSetBit(bitmap_word *bitmap, size_t index) {
  bitmap[index / NACL_HOST_WORDSIZE] |=
                               ((bitmap_word)1) << (index % NACL_HOST_WORDSIZE);
}

static FORCEINLINE void BitmapClearBit(bitmap_word *bitmap, size_t index) {
  bitmap[index / NACL_HOST_WORDSIZE] &=
                            ~(((bitmap_word)1) << (index % NACL_HOST_WORDSIZE));
}

/* All the bits must be in a single 32-bit bundle.  */
static FORCEINLINE int BitmapIsAnyBitSet(bitmap_word *bitmap,
                                         size_t index, size_t bits) {
  return (bitmap[index / NACL_HOST_WORDSIZE] &
       (((((bitmap_word)1) << bits) - 1) << (index % NACL_HOST_WORDSIZE))) != 0;
}

/* All the bits must be in a single 32-bit bundle. */
static FORCEINLINE void BitmapSetBits(bitmap_word *bitmap,
                                      size_t index, size_t bits) {
  bitmap[index / NACL_HOST_WORDSIZE] |=
               ((((bitmap_word)1) << bits) - 1) << (index % NACL_HOST_WORDSIZE);
}

/* All the bits must be in a single 32-bit bundle. */
static FORCEINLINE void BitmapClearBits(bitmap_word *bitmap,
                                        size_t index, size_t bits) {
  bitmap[index / NACL_HOST_WORDSIZE] &=
            ~(((((bitmap_word)1) << bits) - 1) << (index % NACL_HOST_WORDSIZE));
}

/* Mark the destination of a jump instruction and make an early validity check:
 * to jump outside given code region, the target address must be aligned.
 *
 * Returns TRUE iff the jump passes the early validity check.
 */
static FORCEINLINE int MarkJumpTarget(size_t jump_dest,
                                      bitmap_word *jump_dests,
                                      size_t size) {
  if ((jump_dest & kBundleMask) == 0) {
    return TRUE;
  }
  if (jump_dest >= size) {
    return FALSE;
  }
  BitmapSetBit(jump_dests, jump_dest);
  return TRUE;
}

/*
 * Mark the given address as valid jump target address.
 */
static FORCEINLINE void MarkValidJumpTarget(size_t address,
                                            bitmap_word *valid_targets) {
  BitmapSetBit(valid_targets, address);
}

/*
 * Mark the given address as invalid jump target address (that is: unmark it).
 */
static FORCEINLINE void UnmarkValidJumpTarget(size_t address,
                                              bitmap_word *valid_targets) {
  BitmapClearBit(valid_targets, address);
}

/*
 * Mark the given addresses as invalid jump target addresses (that is: unmark
 * them).
 */
static FORCEINLINE void UnmarkValidJumpTargets(size_t address,
                                               size_t bytes,
                                               bitmap_word *valid_targets) {
  BitmapClearBits(valid_targets, address, bytes);
}

static INLINE Bool ProcessInvalidJumpTargets(
    const uint8_t *data,
    size_t size,
    bitmap_word *valid_targets,
    bitmap_word *jump_dests,
    ValidationCallbackFunc user_callback,
    void *callback_data) {
  size_t elements = (size + NACL_HOST_WORDSIZE - 1) / NACL_HOST_WORDSIZE;
  size_t i, j;
  Bool result = TRUE;

  for (i = 0; i < elements ; i++) {
    bitmap_word jump_dest_mask = jump_dests[i];
    bitmap_word valid_target_mask = valid_targets[i];
    if ((jump_dest_mask & ~valid_target_mask) != 0) {
      for (j = i * NACL_HOST_WORDSIZE; j < (i + 1) * NACL_HOST_WORDSIZE; j++)
        if (BitmapIsBitSet(jump_dests, j) &&
            !BitmapIsBitSet(valid_targets, j)) {
          result &= user_callback(data + j,
                                  data + j,
                                  BAD_JUMP_TARGET,
                                  callback_data);
        }
    }
  }

  return result;
}


/*
 * Process rel8_operand.  Note: rip points to the beginning of the next
 * instruction here and x86 encoding guarantees rel8 field is the last one
 * in a current instruction.
 */
static FORCEINLINE void rel8_operand(const uint8_t *rip,
                                     const uint8_t* codeblock_start,
                                     bitmap_word *jump_dests,
                                     size_t jumpdests_size,
                                     uint32_t *instruction_info_collected) {
  int8_t offset = (uint8_t) (rip[-1]);
  size_t jump_dest = offset + (rip - codeblock_start);

  if (MarkJumpTarget(jump_dest, jump_dests, jumpdests_size))
    *instruction_info_collected |= RELATIVE_8BIT;
  else
    *instruction_info_collected |= RELATIVE_8BIT | DIRECT_JUMP_OUT_OF_RANGE;
}

/*
 * Process rel32_operand.  Note: rip points to the beginning of the next
 * instruction here and x86 encoding guarantees rel32 field is the last one
 * in a current instruction.
 */
static FORCEINLINE void rel32_operand(const uint8_t *rip,
                                      const uint8_t* codeblock_start,
                                      bitmap_word *jump_dests,
                                      size_t jumpdests_size,
                                      uint32_t *instruction_info_collected) {
  int32_t offset = (rip[-4] + 256U * (rip[-3] + 256U * (
                                       rip[-2] + 256U * ((uint32_t) rip[-1]))));
  size_t jump_dest = offset + (rip - codeblock_start);

  if (MarkJumpTarget(jump_dest, jump_dests, jumpdests_size))
    *instruction_info_collected |= RELATIVE_32BIT;
  else
    *instruction_info_collected |= RELATIVE_32BIT | DIRECT_JUMP_OUT_OF_RANGE;
}

static INLINE void check_access(ptrdiff_t instruction_start,
                                enum OperandName base,
                                enum OperandName index,
                                uint8_t restricted_register,
                                bitmap_word *valid_targets,
                                uint32_t *instruction_info_collected) {
  if ((base == REG_RIP) || (base == REG_R15) ||
      (base == REG_RSP) || (base == REG_RBP)) {
    if ((index == NO_REG) || (index == REG_RIZ))
      { /* do nothing. */ }
    else if (index == restricted_register)
      BitmapClearBit(valid_targets, instruction_start),
      *instruction_info_collected |= RESTRICTED_REGISTER_USED;
    else
      *instruction_info_collected |= UNRESTRICTED_INDEX_REGISTER;
  } else {
    *instruction_info_collected |= FORBIDDEN_BASE_REGISTER;
  }
}


static INLINE void process_0_operands(enum OperandName *restricted_register,
                                      uint32_t *instruction_info_collected) {
  /* Restricted %rsp or %rbp must be processed by appropriate nacl-special
   * instruction, not with regular instruction.  */
  if (*restricted_register == REG_RSP) {
    *instruction_info_collected |= RESTRICTED_RSP_UNPROCESSED;
  } else if (*restricted_register == REG_RBP) {
    *instruction_info_collected |= RESTRICTED_RBP_UNPROCESSED;
  }
  *restricted_register = NO_REG;
}

static INLINE void process_1_operand(enum OperandName *restricted_register,
                                     uint32_t *instruction_info_collected,
                                     uint8_t rex_prefix,
                                     uint32_t operand_states) {
  /* Restricted %rsp or %rbp must be processed by appropriate nacl-special
   * instruction, not with regular instruction.  */
  if (*restricted_register == REG_RSP) {
    *instruction_info_collected |= RESTRICTED_RSP_UNPROCESSED;
  } else if (*restricted_register == REG_RBP) {
    *instruction_info_collected |= RESTRICTED_RBP_UNPROCESSED;
  }
  *restricted_register = NO_REG;
  if (CHECK_OPERAND(0, REG_R15, OperandSandbox8bit) ||
      CHECK_OPERAND(0, REG_R15, OperandSandboxRestricted) ||
      CHECK_OPERAND(0, REG_R15, OperandSandboxUnrestricted)) {
    *instruction_info_collected |= R15_MODIFIED;
  } else if ((CHECK_OPERAND(0, REG_RBP, OperandSandbox8bit) && rex_prefix) ||
              CHECK_OPERAND(0, REG_RBP, OperandSandboxRestricted) ||
              CHECK_OPERAND(0, REG_RBP, OperandSandboxUnrestricted)) {
    *instruction_info_collected |= BPL_MODIFIED;
  } else if ((CHECK_OPERAND(0, REG_RSP, OperandSandbox8bit) && rex_prefix) ||
              CHECK_OPERAND(0, REG_RSP, OperandSandboxRestricted) ||
              CHECK_OPERAND(0, REG_RSP, OperandSandboxUnrestricted)) {
    *instruction_info_collected |= SPL_MODIFIED;
  }
}

static INLINE void process_1_operand_zero_extends(
    enum OperandName *restricted_register,
    uint32_t *instruction_info_collected, uint8_t rex_prefix,
    uint32_t operand_states) {
  /* Restricted %rsp or %rbp must be processed by appropriate nacl-special
   * instruction, not with regular instruction.  */
  if (*restricted_register == REG_RSP) {
    *instruction_info_collected |= RESTRICTED_RSP_UNPROCESSED;
  } else if (*restricted_register == REG_RBP) {
    *instruction_info_collected |= RESTRICTED_RBP_UNPROCESSED;
  }
  *restricted_register = NO_REG;
  if (CHECK_OPERAND(0, REG_R15, OperandSandbox8bit) ||
      CHECK_OPERAND(0, REG_R15, OperandSandboxRestricted) ||
      CHECK_OPERAND(0, REG_R15, OperandSandboxUnrestricted)) {
    *instruction_info_collected |= R15_MODIFIED;
  } else if ((CHECK_OPERAND(0, REG_RBP, OperandSandbox8bit) && rex_prefix) ||
              CHECK_OPERAND(0, REG_RBP, OperandSandboxUnrestricted)) {
    *instruction_info_collected |= BPL_MODIFIED;
  } else if ((CHECK_OPERAND(0, REG_RSP, OperandSandbox8bit) && rex_prefix) ||
              CHECK_OPERAND(0, REG_RSP, OperandSandboxUnrestricted)) {
    *instruction_info_collected |= SPL_MODIFIED;
  /* Take 2 bits of operand type from operand_states as *restricted_register,
   * make sure operand_states denotes a register (4th bit == 0). */
  } else if ((operand_states & 0x70) == (OperandSandboxRestricted << 5)) {
    *restricted_register = operand_states & 0x0f;
  }
}

static INLINE void process_2_operands(enum OperandName *restricted_register,
                                      uint32_t *instruction_info_collected,
                                      uint8_t rex_prefix,
                                      uint32_t operand_states) {
  /* Restricted %rsp or %rbp must be processed by appropriate nacl-special
   * instruction, not with regular instruction.  */
  if (*restricted_register == REG_RSP) {
    *instruction_info_collected |= RESTRICTED_RSP_UNPROCESSED;
  } else if (*restricted_register == REG_RBP) {
    *instruction_info_collected |= RESTRICTED_RBP_UNPROCESSED;
  }
  *restricted_register = NO_REG;
  if (CHECK_OPERAND(0, REG_R15, OperandSandbox8bit) ||
      CHECK_OPERAND(0, REG_R15, OperandSandboxRestricted) ||
      CHECK_OPERAND(0, REG_R15, OperandSandboxUnrestricted) ||
      CHECK_OPERAND(1, REG_R15, OperandSandbox8bit) ||
      CHECK_OPERAND(1, REG_R15, OperandSandboxRestricted) ||
      CHECK_OPERAND(1, REG_R15, OperandSandboxUnrestricted)) {
    *instruction_info_collected |= R15_MODIFIED;
  } else if ((CHECK_OPERAND(0, REG_RBP, OperandSandbox8bit) && rex_prefix) ||
              CHECK_OPERAND(0, REG_RBP, OperandSandboxRestricted) ||
              CHECK_OPERAND(0, REG_RBP, OperandSandboxUnrestricted) ||
             (CHECK_OPERAND(1, REG_RBP, OperandSandbox8bit) && rex_prefix) ||
              CHECK_OPERAND(1, REG_RBP, OperandSandboxRestricted) ||
              CHECK_OPERAND(1, REG_RBP, OperandSandboxUnrestricted)) {
    *instruction_info_collected |= BPL_MODIFIED;
  } else if ((CHECK_OPERAND(0, REG_RSP, OperandSandbox8bit) && rex_prefix) ||
              CHECK_OPERAND(0, REG_RSP, OperandSandboxRestricted) ||
              CHECK_OPERAND(0, REG_RSP, OperandSandboxUnrestricted) ||
             (CHECK_OPERAND(1, REG_RSP, OperandSandbox8bit) && rex_prefix) ||
              CHECK_OPERAND(1, REG_RSP, OperandSandboxRestricted) ||
              CHECK_OPERAND(1, REG_RSP, OperandSandboxUnrestricted)) {
    *instruction_info_collected |= SPL_MODIFIED;
  }
}

static INLINE void process_2_operands_zero_extends(
     enum OperandName *restricted_register,
     uint32_t *instruction_info_collected,
     uint8_t rex_prefix, uint32_t operand_states) {
  /* Restricted %rsp or %rbp must be processed by appropriate nacl-special
   * instruction, not with regular instruction.  */
  if (*restricted_register == REG_RSP) {
    *instruction_info_collected |= RESTRICTED_RSP_UNPROCESSED;
  } else if (*restricted_register == REG_RBP) {
    *instruction_info_collected |= RESTRICTED_RBP_UNPROCESSED;
  }
  *restricted_register = NO_REG;
  if (CHECK_OPERAND(0, REG_R15, OperandSandbox8bit) ||
      CHECK_OPERAND(0, REG_R15, OperandSandboxRestricted) ||
      CHECK_OPERAND(0, REG_R15, OperandSandboxUnrestricted) ||
      CHECK_OPERAND(1, REG_R15, OperandSandbox8bit) ||
      CHECK_OPERAND(1, REG_R15, OperandSandboxRestricted) ||
      CHECK_OPERAND(1, REG_R15, OperandSandboxUnrestricted)) {
    *instruction_info_collected |= R15_MODIFIED;
  } else if ((CHECK_OPERAND(0, REG_RBP, OperandSandbox8bit) && rex_prefix) ||
              CHECK_OPERAND(0, REG_RBP, OperandSandboxUnrestricted) ||
             (CHECK_OPERAND(1, REG_RBP, OperandSandbox8bit) && rex_prefix) ||
              CHECK_OPERAND(1, REG_RBP, OperandSandboxUnrestricted)) {
    *instruction_info_collected |= BPL_MODIFIED;
  } else if ((CHECK_OPERAND(0, REG_RSP, OperandSandbox8bit) && rex_prefix) ||
              CHECK_OPERAND(0, REG_RSP, OperandSandboxUnrestricted) ||
             (CHECK_OPERAND(1, REG_RSP, OperandSandbox8bit) && rex_prefix) ||
              CHECK_OPERAND(1, REG_RSP, OperandSandboxUnrestricted)) {
    *instruction_info_collected |= SPL_MODIFIED;
  /* Take 2 bits of operand type from operand_states as *restricted_register,
   * make sure operand_states denotes a register (4th bit == 0).  */
  } else if ((operand_states & 0x70) == (OperandSandboxRestricted << 5)) {
    *restricted_register = operand_states & 0x0f;
    if (CHECK_OPERAND(1, REG_RSP, OperandSandboxRestricted)) {
      *instruction_info_collected |= RESTRICTED_RSP_UNPROCESSED;
    } else if (CHECK_OPERAND(1, REG_RBP, OperandSandboxRestricted)) {
      *instruction_info_collected |= RESTRICTED_RBP_UNPROCESSED;
    }
  /* Take 2 bits of operand type from operand_states as *restricted_register,
   * make sure operand_states denotes a register (12th bit == 0).  */
  } else if ((operand_states & 0x7000) == (OperandSandboxRestricted << 13)) {
    *restricted_register = (operand_states & 0x0f00) >> 8;
  }
}

/*
 * This function merges “dangerous” instruction with sandboxing instructions to
 * get a “superinstruction” and unmarks in-between jump targets.
 */
static INLINE void ExpandSuperinstructionBySandboxingBytes(
    size_t sandbox_instructions_size, const uint8_t **instruction_start,
    const uint8_t *data, bitmap_word *valid_targets) {
  *instruction_start -= sandbox_instructions_size;
  /*
   * We need to unmark start of the “dangerous” instruction itself, too, but we
   * don't need to mark the beginning of the whole “superinstruction” - that's
   * why we move start by one byte and don't change the length.
   */
  UnmarkValidJumpTargets((*instruction_start + 1 - data),
                         sandbox_instructions_size,
                         valid_targets);
}

/*
 * Return TRUE if naclcall or nacljmp uses the same register in all three
 * instructions.
 *
 * This version is for the case where “add %src_register, %dst_register” with
 * dst in RM field and src in REG field of ModR/M byte is used.
 *
 * There are five possible forms:
 *
 *              0: 83 eX e0    and    $~0x1f,E86
 *              3: 4? 01 fX    add    RBASE,R86
 *              6: ff eX       jmpq   *R86
 *                 ↑  ↑
 * instruction_start  current_position
 *
 *              0: 4? 83 eX e0 and    $~0x1f,E86
 *              4: 4? 01 fX    add    RBASE,R86
 *              7: ff eX       jmpq   *R86
 *                 ↑  ↑
 * instruction_start  current_position
 *
 *              0: 83 eX e0    and    $~0x1f,E86
 *              3: 4? 01 fX    add    RBASE,R86
 *              6: 4? ff eX    jmpq   *R86
 *                 ↑     ↑
 * instruction_start     current_position
 *
 *              0: 4? 83 eX e0 and    $~0x1f,E86
 *              4: 4? 01 fX    add    RBASE,R86
 *              7: 4? ff eX       jmpq   *R86
 *                 ↑     ↑
 * instruction_start     current_position
 *
 *              0: 4? 83 eX e0 and    $~0x1f,E64
 *              4: 4? 01 fX    add    RBASE,R64
 *              7: 4? ff eX    jmpq   *R64
 *                 ↑     ↑
 * instruction_start     current_position
 *
 * We don't care about “?” (they are checked by DFA).
 */
static INLINE Bool VerifyNaclCallOrJmpAddToRM(
   const uint8_t *instruction_start, const uint8_t *current_position) {
  return
    RMFromModRM(instruction_start[-5]) == RMFromModRM(instruction_start[-1]) &&
    RMFromModRM(instruction_start[-5]) == RMFromModRM(current_position[0]);
}

/*
 * Return TRUE if naclcall or nacljmp uses the same register in all three
 * instructions.
 *
 * This version is for the case where “add %src_register, %dst_register” with
 * dst in REG field and src in RM field of ModR/M byte is used.
 *
 * There are five possible forms:
 *
 *              0: 83 eX e0    and    $~0x1f,E86
 *              3: 4? 03 Xf    add    RBASE,R86
 *              6: ff eX       jmpq   *R86
 *                 ↑  ↑
 * instruction_start  current_position
 *
 *              0: 4? 83 eX e0 and    $~0x1f,E86
 *              4: 4? 03 Xf    add    RBASE,R86
 *              7: ff eX       jmpq   *R86
 *                 ↑  ↑
 * instruction_start  current_position
 *
 *              0: 83 eX e0    and    $~0x1f,E86
 *              3: 4? 03 Xf    add    RBASE,R86
 *              6: 4? ff eX       jmpq   *R86
 *                 ↑     ↑
 * instruction_start     current_position
 *
 *              0: 4? 83 eX e0 and    $~0x1f,E86
 *              4: 4? 03 Xf    add    RBASE,R86
 *              7: 4? ff eX       jmpq   *R86
 *                 ↑     ↑
 * instruction_start     current_position
 *
 *              0: 4? 83 eX e0 and    $~0x1f,E64
 *              4: 4? 03 Xf    add    RBASE,R64
 *              7: 4? ff eX    jmpq   *R64
 *                 ↑     ↑
 * instruction_start     current_position
 *
 * We don't care about “?” (they are checked by DFA).
 */
static INLINE Bool VerifyNaclCallOrJmpAddToReg(
   const uint8_t *instruction_start, const uint8_t *current_position) {
  return
    RMFromModRM(instruction_start[-5]) == RegFromModRM(instruction_start[-1]) &&
    RMFromModRM(instruction_start[-5]) == RMFromModRM(current_position[0]);
}

/*
 * This function checks that naclcall or nacljmp are correct (that is: three
 * component instructions match) and if that is true then it merges call or jmp
 * with a sandboxing to get a “superinstruction” and removes in-between jump
 * targets.  If it's not true then it triggers “unrecognized instruction” error
 * condition.
 *
 * This version is for the case where “add with dst register in RM field”
 * (opcode 0x01) and “add without REX prefix” is used.
 *
 * There are two possibile forms:
 *
 *              0: 83 eX e0    and    $~0x1f,E86
 *              3: 4? 01 fX    add    RBASE,R86
 *              6: ff eX       jmpq   *R86
 *                 ↑  ↑
 * instruction_start  current_position
 *
 *              0: 83 eX e0    and    $~0x1f,E86
 *              3: 4? 01 fX    add    RBASE,R86
 *              6: 4? ff eX    jmpq   *R86
 *                 ↑     ↑
 * instruction_start     current_position
 */
static INLINE void ProcessNaclCallOrJmpAddToRMNoRex(
    uint32_t *instruction_info_collected, const uint8_t **instruction_start,
    const uint8_t *current_position, const uint8_t *data,
    bitmap_word *valid_targets) {
  if (VerifyNaclCallOrJmpAddToRM(*instruction_start, current_position))
    ExpandSuperinstructionBySandboxingBytes(
      3 /* and */ + 3 /* add */, instruction_start, data, valid_targets);
  else
    *instruction_info_collected |= UNRECOGNIZED_INSTRUCTION;
}

/*
 * This function checks that naclcall or nacljmp are correct (that is: three
 * component instructions match) and if that is true then it merges call or jmp
 * with a sandboxing to get a “superinstruction” and removes in-between jump
 * targets.  If it's not true then it triggers “unrecognized instruction” error
 * condition.
 *
 * This version is for the case where “add with dst register in REG field”
 * (opcode 0x03) and “add without REX prefix” is used.
 *
 * There are two possibile forms:
 *
 *              0: 83 eX e0    and    $~0x1f,E86
 *              3: 4? 03 Xf    add    RBASE,R86
 *              6: ff eX       jmpq   *R86
 *                 ↑  ↑
 * instruction_start  current_position
 *
 *              0: 83 eX e0    and    $~0x1f,E86
 *              3: 4? 03 Xf    add    RBASE,R86
 *              6: 4? ff eX    jmpq   *R86
 *                 ↑     ↑
 * instruction_start     current_position
 */
static INLINE void ProcessNaclCallOrJmpAddToRegNoRex(
    uint32_t *instruction_info_collected, const uint8_t **instruction_start,
    const uint8_t *current_position, const uint8_t *data,
    bitmap_word *valid_targets) {
  if (VerifyNaclCallOrJmpAddToReg(*instruction_start, current_position))
    ExpandSuperinstructionBySandboxingBytes(
      3 /* and */ + 3 /* add */, instruction_start, data, valid_targets);
  else
    *instruction_info_collected |= UNRECOGNIZED_INSTRUCTION;
}

/*
 * This function checks that naclcall or nacljmp are correct (that is: three
 * component instructions match) and if that is true then it merges call or jmp
 * with a sandboxing to get a “superinstruction” and removes in-between jump
 * targets.  If it's not true then it triggers “unrecognized instruction” error
 * condition.
 *
 * This version is for the case where “add with dst register in RM field”
 * (opcode 0x01) and “add without REX prefix” is used.
 *
 * There are three possibile forms:
 *
 *              0: 4? 83 eX e0 and    $~0x1f,E86
 *              4: 4? 01 fX    add    RBASE,R86
 *              7: ff eX    jmpq   *R86
 *                 ↑  ↑
 * instruction_start  current_position
 *
 *              0: 4? 83 eX e0 and    $~0x1f,E86
 *              4: 4? 01 fX    add    RBASE,R86
 *              7: 4? ff eX    jmpq   *R86
 *                 ↑     ↑
 * instruction_start     current_position
 *
 *              0: 4? 83 eX e0 and    $~0x1f,E64
 *              4: 4? 01 fX    add    RBASE,R64
 *              7: 4? ff eX    jmpq   *R64
 *                 ↑     ↑
 * instruction_start     current_position
 */
static INLINE void ProcessNaclCallOrJmpAddToRMWithRex(
    uint32_t *instruction_info_collected, const uint8_t **instruction_start,
    const uint8_t *current_position, const uint8_t *data,
    bitmap_word *valid_targets) {
  if (VerifyNaclCallOrJmpAddToRM(*instruction_start, current_position))
    ExpandSuperinstructionBySandboxingBytes(
      4 /* and */ + 3 /* add */, instruction_start, data, valid_targets);
  else
    *instruction_info_collected |= UNRECOGNIZED_INSTRUCTION;
}

/*
 * This function checks that naclcall or nacljmp are correct (that is: three
 * component instructions match) and if that is true then it merges call or jmp
 * with a sandboxing to get a “superinstruction” and removes in-between jump
 * targets.  If it's not true then it triggers “unrecognized instruction” error
 * condition.
 *
 * This version is for the case where “add with dst register in REG field”
 * (opcode 0x03) and “add without REX prefix” is used.
 *
 * There are three possibile forms:
 *
 *              0: 4? 83 eX e0 and    $~0x1f,E86
 *              4: 4? 03 Xf    add    RBASE,R86
 *              7: ff eX    jmpq   *R86
 *                 ↑  ↑
 * instruction_start  current_position
 *
 *              0: 4? 83 eX e0 and    $~0x1f,E86
 *              4: 4? 03 Xf    add    RBASE,R86
 *              7: 4? ff eX    jmpq   *R86
 *                 ↑     ↑
 * instruction_start     current_position
 *
 *              0: 4? 83 eX e0 and    $~0x1f,E64
 *              4: 4? 03 Xf    add    RBASE,R64
 *              7: 4? ff eX    jmpq   *R64
 *                 ↑     ↑
 * instruction_start     current_position
 */
static INLINE void ProcessNaclCallOrJmpAddToRegWithRex(
    uint32_t *instruction_info_collected, const uint8_t **instruction_start,
    const uint8_t *current_position, const uint8_t *data,
    bitmap_word *valid_targets) {
  if (VerifyNaclCallOrJmpAddToReg(*instruction_start, current_position))
    ExpandSuperinstructionBySandboxingBytes(
      4 /* and */ + 3 /* add */, instruction_start, data, valid_targets);
  else
    *instruction_info_collected |= UNRECOGNIZED_INSTRUCTION;
}

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_VALIDATOR_INTERNAL_H_ */
