/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _DECODER_X86_64_H_
#define _DECODER_X86_64_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

enum operand_type {
  OperandSize2bit,       /* See VPERMIL2Px instruction for description.       */
  OperandSize8bit,
  OperandSize16bit,
  OperandSize32bit,
  OperandSize64bit,
  OperandSize128bit,
  OperandSize256bit,
  OperandFloatSize16bit, /* OperandFloatSize16bit, OperandFloatSize32bit,     */
  OperandFloatSize32bit, /* OperandFloatSize64bit, and OperandFloatSize80bit  */
  OperandFloatSize64bit, /* are used for in-memory operands and XMM:          */
  OperandFloatSize80bit, /*   X87 registers always have type OperandST.       */
  OperandX87Size16bit,   /* OperandX87Size16bit, OperandX87Size32bit, and     */
  OperandX87Size32bit,   /* OperandX87Size64bit are signed integers in memory.*/
  OperandX87Size64bit,   /* They are used for x87 instructions.               */
  OperandX87BCD,         /* 10-byte packed BCD value in memory.               */
  OperandX87ENV,         /* A 14-byte or 28-byte x87 environment.             */
  OperandX87STATE,       /* A 94-byte or 108-byte x87 state.                  */
  OperandX87MMXXMMSTATE, /* A 512-byte extended x87/MMX/XMM state.            */
  OperandST,
  OperandSelector,        /* Operand is 6bytes/10bytes selector in memory.    */
  OperandFarPtr,          /* Operand is 6bytes/10bytes far pointer in memory. */
  OperandSegmentRegister, /* Operand is segment register: %{e,c,s,d,f,g}s.    */
  OperandControlRegister, /* Operand is control register: %crX.               */
  OperandDebugRegister,   /* Operand is debug register: %drX.                 */
  OperandMMX,
  OperandXMM,
  OperandYMM
};

enum register_name {
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
  REG_RM,       /* Address in memory via rm field.                            */
  REG_RIP,      /* RIP - used as base in x86-64 mode.                         */
  REG_RIZ,      /* EIZ/RIZ - used as "always zero index" register.            */
  REG_IMM,      /* Fixed value in imm field.                                  */
  REG_IMM2,     /* Fixed value in second imm field.                           */
  REG_DS_RBX,   /* Fox xlat: %ds(%rbx).                                       */
  REG_ES_RDI,   /* For string instructions: %es:(%rsi).                       */
  REG_DS_RSI,   /* For string instructions: %ds:(%rdi).                       */
  REG_PORT_DX,  /* 16-bit DX: for in/out instructions.                        */
  REG_NONE,     /* For modrm: both index and base can be absent.              */
  REG_ST,       /* For x87 instructions: implicit %st.                        */
  JMP_TO        /* Operand is jump target address: usually %rip+offset.       */
};

struct instruction {
  const char *name;
  unsigned char operands_count;
  struct {
    unsigned char rex;        /* Mostly to distingush cases like %ah vs %spl. */
    _Bool data16:1;           /* "Normal", non-rex prefixes. */
    _Bool lock:1;
    _Bool repnz:1;
    _Bool repz:1;
    _Bool branch_not_taken:1;
    _Bool branch_taken:1;
  } prefix;
  struct {
    enum register_name name;
    enum operand_type type;
  } operands[5];
  struct {
    enum register_name base;
    enum register_name index;
    int scale;
    uint64_t offset;
  } rm;
  uint64_t imm[2];
};

typedef void (*process_instruction_func) (const uint8_t *begin,
                                          const uint8_t *end,
                                          struct instruction *instruction,
                                          void *userdata);

typedef void (*process_error_func) (const uint8_t *ptr, void *userdata);

int DecodeChunkAMD64(const uint8_t *data, size_t size, 
                     process_instruction_func process_instruction,
                     process_error_func process_error, void *userdata);

int DecodeChunkIA32(const uint8_t *data, size_t size, 
                    process_instruction_func process_instruction,
                    process_error_func process_error, void *userdata);

#ifdef __cplusplus
}
#endif

#endif
