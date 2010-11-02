/* Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * API to generator routines for building x86 instruction set.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDECODE_TABLEGEN_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDECODE_TABLEGEN_H__

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator_x86/ncopcode_desc.h"

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

/* Define the default prefix to use. Typical use is top-level
 * routine in file defines the default. All helping routines
 * that define a specific local prefix (using NaClDefInstPrefix),
 * then only need to call NaClResetToDefaultInstPrefix.
 *
 * Note: automatically calls NaClDefInstPrefix on the given prefix.
 */
void NaClDefDefaultInstPrefix(const NaClInstPrefix prefix);

/* Resets the default opcode prefix to the value of the last
 * call to NaClDefDefaultInstPrefix.
 */
void NaClResetToDefaultInstPrefix();

/* By default, an opcode can only have either zero or one choice.
 * If you want to define more that one entry for an opcode sequence,
 * you must register the number expected with a call to this function.
 * Note: Assumes the current opcode prefix should be applied.
 */
void NaClDefInstChoices(const uint8_t opcode, const int count);

/* Same as NaClDefInstChoices, but extends the opcode with the
 * modrm opcode in the modrm byte (must be in [0..7]).
 */
void NaClDefInstMrmChoices(const uint8_t opcode,
                           const uint8_t modrm_opcode,
                           const int count);

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

/* Same as NaClDefInstChoices, except that the counts for
 * the 32 and 64 bit models can be separately epressed.
 */
void NaClDefInstChoices_32_64(const uint8_t opcode,
                              const int count_32,
                              const int count_64);

/* Same as NaClDefInstChoices_32_64, but extends the opcode with
 * the modrm opcode in the modrm byte (must be in [0..7]).
 */
void NaClDefInstMrmChoices_32_64(const uint8_t opcode,
                                 const uint8_t modrm_opcode,
                                 const int count_32,
                                 const int count_64);

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
void NaClDelaySanityChecks();

void NaClApplySanityChecks();

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
NaClInst* NaClGetDefInst();

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

/* Defines a specific sequence of byte codes for which the next NaClDefInst
 * should apply. When specified, restricts the match to be only defined for
 * that specific sequence of characters.
 */
void NaClDefInstSeq(const char* opcode_seq);

/* Defines an invalid instruction for the current prefix. */
void NaClDefInvalid(const uint8_t opcode);

/* Defines an invalid instruction for the given prefix. */
void NaClDefInvalidIcode(NaClInstPrefix prefix, const uint8_t opcode);

/* Adds the given operands description to the current instruction being
 * processed.
 */
void NaClAddOperandsDesc(const char* desc);

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
 *
 * Note: Index must be adjusted by one if the first (hidden) operand
 * is an opcode extention.
 */
void NaClAddOpFlags(uint8_t operand_index, NaClOpFlags more_flags);

/* Add additional operand flags to the indexed operand of hte current
 * instruction being processed (index is 0 based).
 *
 * Note: Index is positional. It will automatically be incremented by
 * one (internally) if the first (hidden) operand is an opcode extension.
 */
void NaClAddOperandFlags(uint8_t operand_index, NaClOpFlags more_flags);

/* Removes operand flags from the indexed operand of the current
 * instruction being processed (index is 0 based).
 */
void NaClRemoveOpFlags(uint8_t operand_index, NaClOpFlags flags);

/* Returns the set of operand size flags defined for the given instruction. */
NaClIFlags NaClOperandSizes(NaClInst* inst);

/* Defines one byte opcodes. */
void NaClDefOneByteInsts(struct NaClSymbolTable* context_st);

/* Defines two byte opcodes beginning with OF. */
void NaClDef0FInsts(struct NaClSymbolTable* context_st);

/* Defines two byte opcodes beginning with DC. */
void NaClDefDCInsts();

/* Defines SSE instructions (i.e. instructions using MMX and XMM registers). */
void NaClDefSseInsts(struct NaClSymbolTable* context_st);

/* Define x87 instructions. */
void NaClDefX87Insts(struct NaClSymbolTable* context_st);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDECODE_TABLEGEN_H__ */
