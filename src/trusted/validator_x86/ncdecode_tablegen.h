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
  NaClRunModeSize
} NaClRunMode;

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
 * opcode in the modrm byte.
 */
void NaClDefInstMrmChoices(const uint8_t opcode,
                           const NaClOpKind modrm_opcode,
                           const int count);

/* Same as NaClDefInstChoices, but you can explicitly define the
 * prefix associated with the opcode.
 */
void NaClDefPrefixInstChoices(const NaClInstPrefix prefix,
                              const uint8_t opcode,
                              const int count);

/* Same as NaClDefPrefixInstChoices, but extends the opcode with
 * the opcode in the modrm byte.
 */
void NaClDefPrefixInstMrmChoices(const NaClInstPrefix prefix,
                                 const uint8_t opcode,
                                 const NaClOpKind modrm_opcode,
                                 const int count);

/* Same as NaClDefInstChoices, except that the counts for
 * the 32 and 64 bit models can be separately epressed.
 */
void NaClDefInstChoices_32_64(const uint8_t opcode,
                              const int count_32,
                              const int count_64);

/* Same as NaClDefInstChoices_32_64, but extends the opcode with
 * the opcode in the modrm byte.
 */
void NaClDefInstMrmChoices_32_64(const uint8_t opcode,
                                 const NaClOpKind modrm_opcode,
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
 * the opcode in the modrm byte.
 */
void NaClDefPrefixInstMrmChoices_32_64(const NaClInstPrefix prefix,
                                       const uint8_t opcode,
                                       const NaClOpKind modrm_opcode,
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

/* Defines a specific sequence of byte codes for which the next NaClDefInst
 * should apply. When specified, restricts the match to be only defined for
 * that specific sequence of characters.
 */
void NaClDefInstSeq(const char* opcode_seq);

/* Add additional opcode flags to the current instruction being processed. */
void NaClAddIFlags(NaClIFlags more_flags);

/*
 * Define the next operand of the current opcode to have the given kind
 * and flags.
 */
void NaClDefOp(NaClOpKind kind, NaClOpFlags flags);

/* Add additional operand flags to the indexed operand of the current
 * instruction being processed (index is 0 based).
 */
void NaClAddOpFlags(uint8_t operand_index, NaClOpFlags more_flags);


/* Defines one byte opcodes. */
void NaClDefOneByteInsts();

/* Defines two byte opcodes beginning with OF. */
void NaClDef0FInsts();

/* Defines two byte opcodes beginning with DC. */
void NaClDefDCInsts();

/* Defines SSE instructions (i.e. instructions using MMX and XMM registers). */
void NaClDefSseInsts();

/* Define x87 instructions. */
void NaClDefX87Insts();

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDECODE_TABLEGEN_H__ */
