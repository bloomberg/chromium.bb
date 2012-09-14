/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DECODER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DECODER_H_

#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator/x86/nacl_cpuid.h"

EXTERN_C_BEGIN

enum operand_type {

  /*
   * These are for general-purpose registers, memory access and immediates.
   * They are not used for XMM, MMX etc.
  */
  OPERAND_SIZE_2_BIT, /* See VPERMIL2Px instruction for description. */
  OPERAND_SIZE_8_BIT,
  OPERAND_SIZE_16_BIT,
  OPERAND_SIZE_32_BIT,
  OPERAND_SIZE_64_BIT,
  OPERAND_SIZE_128_BIT,
  OPERAND_SIZE_256_BIT,

  /* OPERAND_FLOAT_SIZE_*_BIT are used for in-memory operands. */
  OPERAND_FLOAT_SIZE_16_BIT,
  OPERAND_FLOAT_SIZE_32_BIT,
  OPERAND_FLOAT_SIZE_64_BIT,
  OPERAND_FLOAT_SIZE_80_BIT,

  /* OPERAND_X87_SIZE_64_BIT are signed integers in memory.*/
  OPERAND_X87_SIZE_16_BIT,
  OPERAND_X87_SIZE_32_BIT,
  OPERAND_X87_SIZE_64_BIT,


  OPERAND_X87_BCD,          /* 10-byte packed BCD value in memory.           */
  OPERAND_X87_ENV,          /* A 14-byte or 28-byte x87 environment.         */
  OPERAND_X87_STATE,        /* A 94-byte or 108-byte x87 state.              */
  OPERAND_X87_MMX_MM_STATE, /* A 512-byte extended x87/MMX/XMM state.        */
  OPERAND_SELECTOR,         /* Operand is 6/10 bytes selector in memory.     */
  OPERAND_FAR_PTR,          /* Operand is 6/10 bytes far pointer in memory.  */

  OPERAND_ST,               /* Any X87 register.                             */
  OPERAND_SEGMENT_REGISTER, /* Operand is segment register: %{e,c,s,d,f,g}s. */
  OPERAND_CONTROL_REGISTER, /* Operand is control register: %crX.            */
  OPERAND_DEBUG_REGISTER,   /* Operand is debug register: %drX.              */
  OPERAND_MMX,
  OPERAND_XMM,
  OPERAND_YMM
};

enum register_name {
  /* First 16 registers are compatible with encoding of registers in x86 ABI. */
  REG_RAX,
  REG_RCX,
  REG_RDX,
  REG_RBX,
  REG_RSP,
  REG_RBP,
  REG_RSI,
  REG_RDI,
  REG_R8,
  REG_R9,
  REG_R10,
  REG_R11,
  REG_R12,
  REG_R13,
  REG_R14,
  REG_R15,
  /* These are pseudo-registers used in special cases.                        */
  REG_RM,       /* Address in memory via rm field.                            */
  REG_RIP,      /* RIP - used as base in x86-64 mode.                         */
  REG_RIZ,      /* EIZ/RIZ - used as "always zero index" register.            */
  REG_IMM,      /* Fixed value in imm field.                                  */
  REG_IMM2,     /* Fixed value in second imm field.                           */
  REG_DS_RBX,   /* Fox xlat: %ds(%rbx).                                       */
  REG_ES_RDI,   /* For string instructions: %es:(%rsi).                       */
  REG_DS_RSI,   /* For string instructions: %ds:(%rdi).                       */
  REG_PORT_DX,  /* 16-bit DX: for in/out instructions.                        */
  NO_REG,       /* For modrm: both index and base can be absent.              */
  REG_ST,       /* For x87 instructions: implicit %st.                        */
  JMP_TO        /* Operand is jump target address: usually %rip+offset.       */
};

/*
 * This enum extends NaClCPUFeatureID to cover instructions not recognized in
 *
 * /
enum DecoderCPUFeatures {
};
*/

enum disp_mode {
  DISPNONE,
  DISP8,
  DISP16,
  DISP32,
  DISP64,
};

struct instruction {
  const char *name;
  unsigned char operands_count;
  struct {
    unsigned char rex;        /* Mostly to distingush cases like %ah vs %spl. */
#ifdef _MSC_VER
    Bool data16:1;            /* "Normal", non-rex prefixes. */
    Bool lock:1;
    Bool repnz:1;
    Bool repz:1;
    Bool branch_not_taken:1;
    Bool branch_taken:1;
#else
    _Bool data16:1;           /* "Normal", non-rex prefixes. */
    _Bool lock:1;
    _Bool repnz:1;
    _Bool repz:1;
    _Bool branch_not_taken:1;
    _Bool branch_taken:1;
#endif
  } prefix;
  struct {
    enum register_name name;
    enum operand_type type;
  } operands[5];
  struct {
    enum register_name base;
    enum register_name index;
    int scale;
    int64_t offset;
    enum disp_mode disp_type;
  } rm;
  uint64_t imm[2];
};

typedef void (*process_instruction_func) (const uint8_t *begin,
                                          const uint8_t *end,
                                          struct instruction *instruction,
                                          void *userdata);

typedef void (*process_decoding_error_func) (const uint8_t *ptr,
                                             void *userdata);

/* All possible CPUID features enabled.  */
extern const NaClCPUFeaturesX86 full_cpuid_features;

int DecodeChunkAMD64(const uint8_t *data, size_t size,
                     process_instruction_func process_instruction,
                     process_decoding_error_func process_error, void *userdata);

int DecodeChunkIA32(const uint8_t *data, size_t size,
                    process_instruction_func process_instruction,
                    process_decoding_error_func process_error, void *userdata);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DECODER_H_ */
