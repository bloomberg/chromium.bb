/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Data structures for decoding instructions.  Includes definitions which are
 * by both decoders (full-blown standalone one and reduced one in validator).
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DECODER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DECODER_H_

#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator/x86/nacl_cpuid.h"

EXTERN_C_BEGIN

enum OperandType {
  /*
   * These are for general-purpose registers, memory access and immediates.
   * They are not used for XMM, MMX etc.
   */
  OPERAND_TYPE_8_BIT,
  OPERAND_TYPE_16_BIT,
  OPERAND_TYPE_32_BIT,
  OPERAND_TYPE_64_BIT,

  /* Non-GP registers.  */
  OPERAND_TYPE_ST,               /* Any X87 register.                         */
  OPERAND_TYPE_MMX,              /* MMX registers: %mmX.                      */
  OPERAND_TYPE_XMM,              /* XMM register: %xmmX.                      */
  OPERAND_TYPE_YMM,              /* YMM registers: %ymmX.                     */
  OPERAND_TYPE_SEGMENT_REGISTER, /* Operand is segment register: %es … %gs.   */
  OPERAND_TYPE_CONTROL_REGISTER, /* Operand is control register: %crX.        */
  OPERAND_TYPE_DEBUG_REGISTER,   /* Operand is debug register: %drX.          */
  OPERAND_TYPE_TEST_REGISTER,    /* Operand is test register: %trX.           */

  /*
   * All other operand types are not used as register arguments.  These are
   * immediates or memory.
   */
  OPERAND_TYPES_REGISTER_MAX = OPERAND_TYPE_TEST_REGISTER + 1,

  /* See VPERMIL2Px instruction for description of 2-bit operand type. */
  OPERAND_TYPE_2_BIT = OPERAND_TYPES_REGISTER_MAX,
  /*
   * SIMD memory access operands.  Note: 3D Now! and MMX instructions use
   * OPERAND_TYPE_64_BIT operands which are used for GP registers, too.
   * This overloading is not good and it may be good idea to separate them
   * but sadly AMD/Intel manuals conflate them and it was deemed that it's
   * too much work to separate them.
   */
  OPERAND_TYPE_128_BIT,
  OPERAND_TYPE_256_BIT,

  /* OPERAND_X87_SIZE_*_BIT are signed integers in memory.*/
  OPERAND_TYPE_X87_16_BIT,
  OPERAND_TYPE_X87_32_BIT,
  OPERAND_TYPE_X87_64_BIT,

  /* OPERAND_FLOAT_SIZE_*_BIT are used for in-memory operands. */
  OPERAND_TYPE_FLOAT_32_BIT,
  OPERAND_TYPE_FLOAT_64_BIT,
  OPERAND_TYPE_FLOAT_80_BIT,

  /* Miscellaneous structures in memory.  */
  OPERAND_TYPE_X87_BCD,           /* 10-byte packed BCD value.                */
  OPERAND_TYPE_X87_ENV,           /* A 14-byte or 28-byte x87 environment.    */
  OPERAND_TYPE_X87_STATE,         /* A 94-byte or 108-byte x87 state.         */
  OPERAND_TYPE_X87_MMX_XMM_STATE, /* A 512-byte extended x87/MMX/XMM state.   */
  OPERAND_TYPE_SELECTOR,          /* Operand is 6/10 bytes selector.          */
  OPERAND_TYPE_FAR_PTR            /* Operand is 6/10 bytes far pointer.       */
};

enum OperandName {
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
  /* These are different kinds of operands used in special cases.             */
  REG_RM,           /* Address in memory via rm field.                        */
  REG_RIP,          /* RIP - used as base in x86-64 mode.                     */
  REG_RIZ,          /* EIZ/RIZ - used as "always zero index" register.        */
  REG_IMM,          /* Fixed value in imm field.                              */
  REG_IMM2,         /* Fixed value in second imm field.                       */
  REG_DS_RBX,       /* Fox xlat: %ds(%rbx).                                   */
  REG_ES_RDI,       /* For string instructions: %es:(%rsi).                   */
  REG_DS_RSI,       /* For string instructions: %ds:(%rdi).                   */
  REG_PORT_DX,      /* 16-bit DX: for in/out instructions.                    */
  NO_REG,           /* For modrm: both index and base can be absent.          */
  REG_ST,           /* For x87 instructions: implicit %st.                    */
  JMP_TO            /* Operand is jump target address: usually %rip+offset.   */
};

/*
 * Displacement can be of four different sizes in x86 instruction set: nothing,
 * 8-bit, 16-bit, 32-bit, and 64-bit.  These are traditionally treated slightly
 * differently by decoders: 8-bit are usually printed as signed offset, while
 * 32-bit (in ia32 mode) and 64-bit (in amd64 mode) are printed as unsigned
 * offset.
 */
enum DisplacementMode {
  DISPNONE,
  DISP8,
  DISP16,
  DISP32,
  DISP64
};

struct Instruction {
  const char *name;
  unsigned char operands_count;
  struct {
    unsigned char rex;        /* Mostly to distingush cases like %ah vs %spl. */
    Bool rex_b_spurious;
    Bool rex_x_spurious;
    Bool rex_r_spurious;
    Bool rex_w_spurious;
    Bool data16;              /* "Normal", non-rex prefixes. */
    Bool data16_spurious;
    Bool lock;
    Bool repnz;
    Bool repz;
    Bool branch_not_taken;
    Bool branch_taken;
  } prefix;
  struct {
    enum OperandName name;
    enum OperandType type;
  } operands[5];
  struct {
    enum OperandName base;
    enum OperandName index;
    int scale;
    int64_t offset;
    enum DisplacementMode disp_type;
  } rm;
  uint64_t imm[2];
  const char* att_instruction_suffix;
};

typedef void (*ProcessInstructionFunc) (const uint8_t *begin,
                                        const uint8_t *end,
                                        struct Instruction *instruction,
                                        void *userdata);

typedef void (*ProcessDecodingErrorFunc) (const uint8_t *ptr,
                                          void *userdata);

/*
 * kFullCPUIDFeatures is pre-defined constant of NaClCPUFeaturesX86 type with
 * all possible CPUID features enabled.
 */
extern const NaClCPUFeaturesX86 kFullCPUIDFeatures;

int DecodeChunkAMD64(const uint8_t *data, size_t size,
                     ProcessInstructionFunc process_instruction,
                     ProcessDecodingErrorFunc process_error, void *userdata);

int DecodeChunkIA32(const uint8_t *data, size_t size,
                    ProcessInstructionFunc process_instruction,
                    ProcessDecodingErrorFunc process_error, void *userdata);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DECODER_H_ */
