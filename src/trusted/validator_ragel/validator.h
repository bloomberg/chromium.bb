/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _VALIDATOR_X86_64_H_
#define _VALIDATOR_X86_64_H_

#include <inttypes.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum operand_type {
  /* OperandSize8bit and OperandSize16bit don't affect sandboxing state of the
     register: it may become different, but top half does not change it's value.
     R15 can not be used even with these operands.  */
  OperandNoSandboxEffect,
  OperandSize8bit = 0,
  OperandSize16bit = 0,
  /* If we store to 32bit register then it's restricted.  */
  OperandSandboxRestricted = 1,
  OperandSize32bit = 1,
  /* If we store to the 64bit register then it's unrestricted.  */
  OperandSandboxUnrestricted = 2,
  OperandSize64bit = 2,
  /* All other combinations work with specialized registers, or can not actually
     happen at all (for example they describe operand encoded in an instruction.  */
  OperandSandboxIrrelevant = 3,
  OperandSize2bit = 3,
  OperandSize128bit = 3,
  OperandSize256bit = 3,
  OperandFloatSize16bit = 3,
  OperandFloatSize32bit = 3,
  OperandFloatSize64bit = 3,
  OperandFloatSize80bit = 3,
  OperandX87Size16bit = 3,
  OperandX87Size32bit = 3,
  OperandX87Size64bit = 3,
  OperandX87BCD = 3,
  OperandX87ENV = 3,
  OperandX87STATE = 3,
  OperandX87MMXXMMSTATE = 3,
  OperandST = 3,
  OperandSelector = 3,
  OperandFarPtr = 3,
  OperandSegmentRegister = 3,
  OperandControlRegister = 3,
  OperandDebugRegister = 3,
  OperandMMX = 3,
  OperandXMM = 3,
  OperandYMM = 3
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
  REG_RM,	/* Address in memory via rm field.			      */
  REG_RIP,	/* RIP - used as base in x86-64 mode.			      */
  REG_RIZ,	/* EIZ/RIZ - used as "always zero index" register.	      */
  REG_IMM,	/* Fixed value in imm field.				      */
  REG_IMM2,	/* Fixed value in second imm field.			      */
  REG_DS_RBX,	/* Fox xlat: %ds(%rbx).					      */
  REG_ES_RDI,	/* For string instructions: %es:(%rsi).			      */
  REG_DS_RSI,	/* For string instructions: %ds:(%rdi).			      */
  REG_PORT_DX,	/* 16-bit DX: for in/out instructions.			      */
  REG_NONE,	/* For modrm: both index and base can be absent.	      */
  REG_ST,	/* For x87 instructions: implicit %st.			      */
  JMP_TO	/* Operand is jump target address: usually %rip+offset.	      */
};

typedef void (*process_error_func) (const uint8_t *ptr, void *userdata);

int ValidateChunkAMD64(const uint8_t *data, size_t size,
		       process_error_func process_error, void *userdata);

int ValidateChunkIA32(const uint8_t *data, size_t size,
		      process_error_func process_error, void *userdata);

#ifdef __cplusplus
}
#endif

#endif
