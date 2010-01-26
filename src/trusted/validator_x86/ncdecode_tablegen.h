/* Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * API to generator routines for building x86 instruction set.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDECODE_TABLEGEN_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDECODE_TABLEGEN_H__

#include "native_client/src/trusted/validator_x86/ncopcode_desc.h"

/* Possible run modes for instructions. */
typedef enum {
  X86_32,       /* Model x86-32 bit instructions. */
  X86_64,       /* Model x86-64-bit instructions. */
  /* Special end of list marker, denoting the number
   * of run modes;
   */
  RunModeSize
} RunMode;

/* Change the current opcode prefix to the given value. */
void DefineOpcodePrefix(OpcodePrefix prefix);

/* Define the default prefix to use. Typical use is top-level
 * routine in file defines the default. All helping routines
 * that define a specific local prefix (using DefineOpcodePrefix),
 * then only need to call ResetToDefaultOpcodePrefix.
 *
 * Note: automatically calls DefineOpcodePrefix on the given prefix.
 */
void DefineDefaultOpcodePrefix(OpcodePrefix prefix);

/* Resets the default opcode prefix to the value of the last
 * call to DefineDefaultOpcodePrefix.
 */
void ResetToDefaultOpcodePrefix();

/* Define the next opcode (instruction), initializing with
 * no operands.
 */
void DefineOpcode(
    const uint8_t opcode,
    const NaClInstType insttype,
    OpcodeFlags flags,
    const InstMnemonic name);

/*
 * Define the next operand of the current opcode to have the given kind
 * and flags.
 */
void DefineOperand(OperandKind kind, OperandFlags flags);


/* Defines one byte opcodes. */
void DefineOneByteOpcodes();

/* Defines two byte opcodes beginning with OF. */
void Define0FOpcodes();

/* Defines two byte opcodes beginning with DC. */
void DefineDCOpcodes();

/* Defines SSE instructions (i.e. instructions using MMX and XMM registers). */
void DefineSseOpcodes();

/* Define x87 instructions. */
void DefineX87Opcodes();

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDECODE_TABLEGEN_H__ */
