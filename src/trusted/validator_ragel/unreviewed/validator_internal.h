/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file contains common parts of x86-32 and x86-64 insternals (inline
 * functions and defines).
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_VALIDATOR_INTERNAL_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_VALIDATOR_INTERNAL_H_

#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/validator.h"

#if NACL_WINDOWS
# define FORCEINLINE __forceinline
#else
# define FORCEINLINE __inline __attribute__ ((always_inline))
#endif

/* Macroses to suppport CPUID handling.  */
#define SET_CPU_FEATURE(F) \
  if (!(F)) { \
    errors_detected |= CPUID_UNSUPPORTED_INSTRUCTION; \
  }
#define CPUFeature_3DNOW    cpu_features->data[NaClCPUFeature_3DNOW]
#define CPUFeature_3DPRFTCH CPUFeature_3DNOW || CPUFeature_PRE || CPUFeature_LM
#define CPUFeature_AES      cpu_features->data[NaClCPUFeature_AES]
#define CPUFeature_AESAVX   CPUFeature_AES && CPUFeature_AVX
#define CPUFeature_AVX      cpu_features->data[NaClCPUFeature_AVX]
#define CPUFeature_BMI1     cpu_features->data[NaClCPUFeature_BMI1]
#define CPUFeature_CLFLUSH  cpu_features->data[NaClCPUFeature_CLFLUSH]
#define CPUFeature_CLMUL    cpu_features->data[NaClCPUFeature_CLMUL]
#define CPUFeature_CLMULAVX CPUFeature_CLMUL && CPUFeature_AVX
#define CPUFeature_CMOV     cpu_features->data[NaClCPUFeature_CMOV]
#define CPUFeature_CMOVx87  CPUFeature_CMOV && CPUFeature_x87
#define CPUFeature_CX16     cpu_features->data[NaClCPUFeature_CX16]
#define CPUFeature_CX8      cpu_features->data[NaClCPUFeature_CX8]
#define CPUFeature_E3DNOW   cpu_features->data[NaClCPUFeature_E3DNOW]
#define CPUFeature_EMMX     cpu_features->data[NaClCPUFeature_EMMX]
#define CPUFeature_EMMXSSE  CPUFeature_EMMX || CPUFeature_SSE
#define CPUFeature_F16C     cpu_features->data[NaClCPUFeature_F16C]
#define CPUFeature_FMA      cpu_features->data[NaClCPUFeature_FMA]
#define CPUFeature_FMA4     cpu_features->data[NaClCPUFeature_FMA4]
#define CPUFeature_FXSR     cpu_features->data[NaClCPUFeature_FXSR]
#define CPUFeature_LAHF     cpu_features->data[NaClCPUFeature_LAHF]
#define CPUFeature_LM       cpu_features->data[NaClCPUFeature_LM]
#define CPUFeature_LWP      cpu_features->data[NaClCPUFeature_LWP]
/*
 * We allow lzcnt unconditionally
 * See http://code.google.com/p/nativeclient/issues/detail?id=2869
 */
#define CPUFeature_LZCNT    TRUE
#define CPUFeature_MMX      cpu_features->data[NaClCPUFeature_MMX]
#define CPUFeature_MON      cpu_features->data[NaClCPUFeature_MON]
#define CPUFeature_MOVBE    cpu_features->data[NaClCPUFeature_MOVBE]
#define CPUFeature_OSXSAVE  cpu_features->data[NaClCPUFeature_OSXSAVE]
#define CPUFeature_POPCNT   cpu_features->data[NaClCPUFeature_POPCNT]
#define CPUFeature_PRE      cpu_features->data[NaClCPUFeature_PRE]
#define CPUFeature_SSE      cpu_features->data[NaClCPUFeature_SSE]
#define CPUFeature_SSE2     cpu_features->data[NaClCPUFeature_SSE2]
#define CPUFeature_SSE3     cpu_features->data[NaClCPUFeature_SSE3]
#define CPUFeature_SSE41    cpu_features->data[NaClCPUFeature_SSE41]
#define CPUFeature_SSE42    cpu_features->data[NaClCPUFeature_SSE42]
#define CPUFeature_SSE4A    cpu_features->data[NaClCPUFeature_SSE4A]
#define CPUFeature_SSSE3    cpu_features->data[NaClCPUFeature_SSSE3]
#define CPUFeature_TBM      cpu_features->data[NaClCPUFeature_TBM]
#define CPUFeature_TSC      cpu_features->data[NaClCPUFeature_TSC]
/*
 * We allow tzcnt unconditionally
 * See http://code.google.com/p/nativeclient/issues/detail?id=2869
 */
#define CPUFeature_TZCNT    TRUE
#define CPUFeature_x87      cpu_features->data[NaClCPUFeature_x87]
#define CPUFeature_XOP      cpu_features->data[NaClCPUFeature_XOP]

/* Remember some information about instruction for further processing.  */
#define GET_REX_PREFIX() rex_prefix
#define SET_REX_PREFIX(P) rex_prefix = (P)
#define GET_VEX_PREFIX2() vex_prefix2
#define SET_VEX_PREFIX2(P) vex_prefix2 = (P)
#define GET_VEX_PREFIX3() vex_prefix3
#define SET_VEX_PREFIX3(P) vex_prefix3 = (P)
#define SET_MODRM_BASE(N) base = (N)
#define SET_MODRM_INDEX(N) index = (N)

enum {
  REX_B = 1,
  REX_X = 2,
  REX_R = 4,
  REX_W = 8
};

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
#define SET_OPERAND_TYPE_OperandSize8bit(N) \
  operand_states |= OperandSandbox8bit << (5 + ((N) << 3))
#define SET_OPERAND_TYPE_OperandSize16bit(N) \
  operand_states |= OperandSandboxUnrestricted << (5 + ((N) << 3))
#define SET_OPERAND_TYPE_OperandSize32bit(N) \
  operand_states |= OperandSandboxRestricted << (5 + ((N) << 3))
#define SET_OPERAND_TYPE_OperandSize64bit(N) \
  operand_states |= OperandSandboxUnrestricted << (5 + ((N) << 3))
#define CHECK_OPERAND(N, S, T) \
  ((operand_states & (0xff << ((N) << 3))) == ((S | (T << 5)) << ((N) << 3)))

/* Ignore this information for now.  */
#define SET_DATA16_PREFIX(S)
#define SET_LOCK_PREFIX(S)
#define SET_REPZ_PREFIX(S)
#define SET_REPNZ_PREFIX(S)
#define SET_BRANCH_TAKEN(S)
#define SET_BRANCH_NOT_TAKEN(S)
#define SET_MODRM_SCALE(S)
#define SET_DISP_TYPE(T)
#define SET_DISP_PTR(P)
#define SET_IMM_TYPE(T)
#define SET_IMM_PTR(P)
#define SET_IMM2_TYPE(T)
#define SET_IMM2_PTR(P)

static const int kBitsPerByte = 8;

static INLINE uint8_t *BitmapAllocate(size_t indexes) {
  size_t byte_count = (indexes + kBitsPerByte - 1) / kBitsPerByte;
  uint8_t *bitmap = malloc(byte_count);
  if (bitmap != NULL) {
    memset(bitmap, 0, byte_count);
  }
  return bitmap;
}

static FORCEINLINE int BitmapIsBitSet(uint8_t *bitmap, size_t index) {
  return (bitmap[index / kBitsPerByte] & (1 << (index % kBitsPerByte))) != 0;
}

static FORCEINLINE void BitmapSetBit(uint8_t *bitmap, size_t index) {
  bitmap[index / kBitsPerByte] |= 1 << (index % kBitsPerByte);
}

static FORCEINLINE void BitmapClearBit(uint8_t *bitmap, size_t index) {
  bitmap[index / kBitsPerByte] &= ~(1 << (index % kBitsPerByte));
}

/* Mark the destination of a jump instruction and make an early validity check:
 * to jump outside given code region, the target address must be aligned.
 *
 * Returns TRUE iff the jump passes the early validity check.
 */
static FORCEINLINE int MarkJumpTarget(size_t jump_dest,
                                      uint8_t *jump_dests,
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
 * Process rel8_operand.  Note: rip points to the beginning of the next
 * instruction here and x86 encoding guarantees rel8 field is the last one
 * in a current instruction.
 */
static FORCEINLINE void rel8_operand(const uint8_t *rip,
                                     const uint8_t* codeblock_start,
                                     uint8_t *jump_dests, size_t jumpdests_size,
                                     uint32_t *errors_detected) {
  int8_t offset = (uint8_t) (rip[-1]);
  size_t jump_dest = offset + (rip - codeblock_start);

  if (!MarkJumpTarget(jump_dest, jump_dests, jumpdests_size)) {
    *errors_detected |= DIRECT_JUMP_OUT_OF_RANGE;
  }
}

/*
 * Process rel32_operand.  Note: rip points to the beginning of the next
 * instruction here and x86 encoding guarantees rel32 field is the last one
 * in a current instruction.
 */
static FORCEINLINE void rel32_operand(const uint8_t *rip,
                                      const uint8_t* codeblock_start,
                                      uint8_t *jump_dests,
                                      size_t jumpdests_size,
                                      uint32_t *errors_detected) {
  int32_t offset = (rip[-4] + 256U * (rip[-3] + 256U * (
                                       rip[-2] + 256U * ((uint32_t) rip[-1]))));
  size_t jump_dest = offset + (rip - codeblock_start);

  if (!MarkJumpTarget(jump_dest, jump_dests, jumpdests_size)) {
    *errors_detected |= DIRECT_JUMP_OUT_OF_RANGE;
  }
}

enum {
  kNoRestrictedReg = 32,
  kSandboxedRsi,
  kSandboxedRdi,
  kSandboxedRsiRestrictedRdi,
  kSandboxedRsiSandboxedRdi
};

static INLINE void check_access(ptrdiff_t instruction_start,
                                enum register_name base,
                                enum register_name index,
                                uint8_t restricted_register,
                                uint8_t *valid_targets,
                                uint32_t *errors_detected) {
  if ((base == REG_RIP) || (base == REG_R15) ||
      ((base == REG_RSP) && (restricted_register != REG_RSP)) ||
      ((base == REG_RBP) && (restricted_register != REG_RBP))) {
    if ((index == restricted_register) ||
        ((index == REG_RDI) &&
         (restricted_register == kSandboxedRsiRestrictedRdi))) {
      BitmapClearBit(valid_targets, instruction_start);
    } else if ((index != NO_REG) && (index != REG_RIZ)) {
      *errors_detected |= UNRESTRICTED_INDEX_REGISTER;
    }
  } else {
    *errors_detected |= FORBIDDEN_BASE_REGISTER;
  }
}


static INLINE void process_0_operands(uint8_t *restricted_register,
                                      uint32_t *errors_detected) {
  /* Restricted %rsp or %rbp must be processed by appropriate nacl-special
   * instruction, not with regular instruction.  */
  if (*restricted_register == REG_RSP) {
    *errors_detected |= RESTRICTED_RSP_UNPROCESSED;
  } else if (*restricted_register == REG_RBP) {
    *errors_detected |= RESTRICTED_RBP_UNPROCESSED;
  }
  *restricted_register = kNoRestrictedReg;
}

static INLINE void process_1_operands(uint8_t *restricted_register,
                                      uint32_t *errors_detected,
                                      uint8_t rex_prefix,
                                      uint32_t operand_states,
                                      const uint8_t *instruction_start,
                                      const uint8_t
                                               **sandboxed_rsi_restricted_rdi) {
  /* Restricted %rsp or %rbp must be processed by appropriate nacl-special
   * instruction, not with regular instruction.  */
  if (*restricted_register == REG_RSP) {
    *errors_detected |= RESTRICTED_RSP_UNPROCESSED;
  } else if (*restricted_register == REG_RBP) {
    *errors_detected |= RESTRICTED_RBP_UNPROCESSED;
  }
  /* If Sandboxed Rsi is destroyed then we must detect that.  */
  if (*restricted_register == kSandboxedRsi) {
    if (CHECK_OPERAND(0, REG_RSI, OperandSandboxRestricted) ||
        CHECK_OPERAND(0, REG_RSI, OperandSandboxUnrestricted)) {
      *restricted_register = kNoRestrictedReg;
    }
  }
  if (*restricted_register == kSandboxedRsi) {
    if (CHECK_OPERAND(0, REG_RDI, OperandSandboxRestricted)) {
      *sandboxed_rsi_restricted_rdi = instruction_start;
      *restricted_register = kSandboxedRsiRestrictedRdi;
    }
  }
  if (*restricted_register != kSandboxedRsiRestrictedRdi) {
    *restricted_register = kNoRestrictedReg;
    if (CHECK_OPERAND(0, REG_R15, OperandSandbox8bit) ||
        CHECK_OPERAND(0, REG_R15, OperandSandboxRestricted) ||
        CHECK_OPERAND(0, REG_R15, OperandSandboxUnrestricted)) {
      *errors_detected |= R15_MODIFIED;
    } else if ((CHECK_OPERAND(0, REG_RBP, OperandSandbox8bit) && rex_prefix) ||
               CHECK_OPERAND(0, REG_RBP, OperandSandboxUnrestricted)) {
      *errors_detected |= BPL_MODIFIED;
    } else if ((CHECK_OPERAND(0, REG_RSP, OperandSandbox8bit) && rex_prefix) ||
               CHECK_OPERAND(0, REG_RSP, OperandSandboxUnrestricted)) {
      *errors_detected |= SPL_MODIFIED;
    /* Take 2 bits of operand type from operand_states as *restricted_register,
     * make sure operand_states denotes a register (4th bit == 0). */
    } else if ((operand_states & 0x70) == (OperandSandboxRestricted << 5)) {
      *restricted_register = operand_states & 0x0f;
    }
  }
}

static INLINE void process_2_operands(uint8_t *restricted_register,
                                      uint32_t *errors_detected,
                                      uint8_t rex_prefix,
                                      uint32_t operand_states,
                                      const uint8_t *instruction_start,
                                      const uint8_t
                                               **sandboxed_rsi_restricted_rdi) {
  /* Restricted %rsp or %rbp must be processed by appropriate nacl-special
   * instruction, not with regular instruction.  */
  if (*restricted_register == REG_RSP) {
    *errors_detected |= RESTRICTED_RSP_UNPROCESSED;
  } else if (*restricted_register == REG_RBP) {
    *errors_detected |= RESTRICTED_RBP_UNPROCESSED;
  }
  /* If Sandboxed Rsi is destroyed then we must detect that.  */
  if (*restricted_register == kSandboxedRsi) {
    if (CHECK_OPERAND(0, REG_RSI, OperandSandboxRestricted) ||
        CHECK_OPERAND(0, REG_RSI, OperandSandboxUnrestricted) ||
        CHECK_OPERAND(1, REG_RSI, OperandSandboxRestricted) ||
        CHECK_OPERAND(1, REG_RSI, OperandSandboxUnrestricted)) {
      *restricted_register = kNoRestrictedReg;
    }
  }
  if (*restricted_register == kSandboxedRsi) {
    if (CHECK_OPERAND(0, REG_RDI, OperandSandboxRestricted) ||
        CHECK_OPERAND(1, REG_RDI, OperandSandboxRestricted)) {
      *sandboxed_rsi_restricted_rdi = instruction_start;
      *restricted_register = kSandboxedRsiRestrictedRdi;
    }
  }
  if (*restricted_register != kSandboxedRsiRestrictedRdi) {
    *restricted_register = kNoRestrictedReg;
    if (CHECK_OPERAND(0, REG_R15, OperandSandbox8bit) ||
        CHECK_OPERAND(0, REG_R15, OperandSandboxRestricted) ||
        CHECK_OPERAND(0, REG_R15, OperandSandboxUnrestricted) ||
        CHECK_OPERAND(1, REG_R15, OperandSandbox8bit) ||
        CHECK_OPERAND(1, REG_R15, OperandSandboxRestricted) ||
        CHECK_OPERAND(1, REG_R15, OperandSandboxUnrestricted)) {
      *errors_detected |= R15_MODIFIED;
    } else if ((CHECK_OPERAND(0, REG_RBP, OperandSandbox8bit) && rex_prefix) ||
               CHECK_OPERAND(0, REG_RBP, OperandSandboxUnrestricted) ||
               (CHECK_OPERAND(1, REG_RBP, OperandSandbox8bit) && rex_prefix) ||
               CHECK_OPERAND(1, REG_RBP, OperandSandboxUnrestricted)) {
      *errors_detected |= BPL_MODIFIED;
    } else if ((CHECK_OPERAND(0, REG_RSP, OperandSandbox8bit) && rex_prefix) ||
               CHECK_OPERAND(0, REG_RSP, OperandSandboxUnrestricted) ||
               (CHECK_OPERAND(1, REG_RSP, OperandSandbox8bit) && rex_prefix) ||
               CHECK_OPERAND(1, REG_RSP, OperandSandboxUnrestricted)) {
      *errors_detected |= SPL_MODIFIED;
    /* Take 2 bits of operand type from operand_states as *restricted_register,
     * make sure operand_states denotes a register (4th bit == 0).  */
    } else if ((operand_states & 0x70) == (OperandSandboxRestricted << 5)) {
      *restricted_register = operand_states & 0x0f;
    /* Take 2 bits of operand type from operand_states as *restricted_register,
     * make sure operand_states denotes a register (12th bit == 0).  */
    } else if ((operand_states & 0x7000) ==
        (OperandSandboxRestricted << (5 + 8))) {
      *restricted_register = (operand_states & 0x0f00) >> 8;
    }
  }
}

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_VALIDATOR_INTERNAL_H_ */
