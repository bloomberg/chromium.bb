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

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDECODE_TABLEGEN_H__ */
