/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * API to generator routines for building x86 instruction set.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NCDECODE_TABLEGEN_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NCDECODE_TABLEGEN_H__

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator/x86/decoder/generator/modeled_nacl_inst.h"

struct NaClSymbolTable;

/* Report the given fatal error, and then quit. */
void NaClFatal(const char* s);

/* Possible run modes for instructions. */
typedef enum {
  X86_32,       /* Model x86-32 bit instructions. */
  X86_64,       /* Model x86-64-bit instructions. */
  /* Special end of list marker, denoting the number
   * of run modes;
   */
  NaClRunModeSize
} NaClRunMode;

/* Defines the run mode files that should be generated. */
extern NaClRunMode NACL_FLAGS_run_mode;

/* Change the current opcode prefix to the given value. */
void NaClDefInstPrefix(const NaClInstPrefix prefix);

/* Resets the default opcode prefix to the value of the last
 * call to NaClDefDefaultInstPrefix.
 */
void NaClResetToDefaultInstPrefix(void);

/* Same as NaClDefInstChoices, but you can explicitly define the
 * prefix associated with the opcode.
 */
void NaClDefPrefixInstChoices(const NaClInstPrefix prefix,
                              const uint8_t opcode,
                              const int count);

/* Same as NaClDefPrefixInstChoices, but extends the opcode with
 * the modrm opcode in the modrm byte (must be in [0..7]).
 */
void NaClDefPrefixInstMrmChoices(const NaClInstPrefix prefix,
                                 const uint8_t opcode,
                                 const uint8_t modrm_opcode,
                                 const int count);

/* Same as NaClDefInstChoices_32_64, but you can explicitly define the
 * prefix associated with the opcode.
 */
void NaClDefPrefixInstChoices_32_64(const NaClInstPrefix prefix,
                                    const uint8_t opcode,
                                    const int count_32,
                                    const int count_64);

/* Same as NaClDefPrefixInstChoices_32_64, but extends the opcode with
 * the modrm opcode in the modrm byte (must be in [0..7]).
 */
void NaClDefPrefixInstMrmChoices_32_64(const NaClInstPrefix prefix,
                                       const uint8_t opcode,
                                       const uint8_t modrm_opcode,
                                       const int count_32,
                                       const int count_64);

/* By default, sanity checks are applied as each defining
 * call is made. When this is called, these sanity checks
 * are turned off until the explicit call to NaClApplySanityChecks.
 */
void NaClDelaySanityChecks(void);

void NaClApplySanityChecks(void);

/* Define the next opcode (instruction), initializing with
 * no operands.
 */
void NaClDefInst(
    const uint8_t opcode,
    const NaClInstType insttype,
    NaClIFlags flags,
    const NaClMnemonic name);

/* Returns the current instruction being defined.
 * ***WARNING***: If you call any function within this header file
 * that modifies the current instruction will invalidate the contents
 * returned by this function.
 */
NaClModeledInst* NaClGetDefInst(void);

/* Define an opcode extension for the current instruction, which is
 * a value between 0 and 7, that appears in the modrm byte of the
 * instruction.
 */
void NaClDefOpcodeExtension(int opcode);

/* Defines an opcode extension stored in the ModRm r/m field (must be
 * in [0..7]).
 */
void NaClDefineOpcodeModRmRmExtension(int value);

/* Define a register value embedded in the opcode value. */
void NaClDefOpcodeRegisterValue(int r);

/* Add additional instruction flags to the current instruction being
 * processed.
 */
void NaClAddIFlags(NaClIFlags more_flags);

/* Remove instruction flags from the current instruction being processed. */
void NaClRemoveIFlags(NaClIFlags less_flags);

/*
 * Define the next operand of the current opcode to have the given kind
 * and flags.
 */
void NaClDefOp(NaClOpKind kind, NaClOpFlags flags);

/* Add additional operand flags to the indexed operand of the current
 * instruction being processed (index is 0 based).
 */
void NaClAddOpFlags(uint8_t operand_index, NaClOpFlags more_flags);

/* Add format string to the indexed oeprand of the current instruction
 * being processed (index is 0 based).
 *
 * Note: the passed in string is copied, and hence its contents can
 * change once this function returns.
 */
void NaClAddOpFormat(uint8_t operand_index, const char* format);

/* Removes operand flags from the indexed operand of the current
 * instruction being processed (index is 0 based).
 */
void NaClRemoveOpFlags(uint8_t operand_index, NaClOpFlags flags);

/* Returns the set of operand size flags defined for the given instruction. */
NaClIFlags NaClOperandSizes(NaClModeledInst* inst);

/* Defines one byte opcodes. */
void NaClDefOneByteInsts(struct NaClSymbolTable* context_st);

/* Defines two byte opcodes beginning with OF. */
void NaClDef0FInsts(struct NaClSymbolTable* context_st);

/* Defines two byte opcodes beginning with DC. */
void NaClDefDCInsts(void);

/* Defines SSE instructions (i.e. instructions using MMX and XMM registers). */
void NaClDefSseInsts(struct NaClSymbolTable* context_st);

/* Define x87 instructions. */
void NaClDefX87Insts(struct NaClSymbolTable* context_st);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NCDECODE_TABLEGEN_H__ */
