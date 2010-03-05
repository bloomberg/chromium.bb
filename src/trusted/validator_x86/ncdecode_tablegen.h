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
void DefineOpcodePrefix(const OpcodePrefix prefix);

/* Define the default prefix to use. Typical use is top-level
 * routine in file defines the default. All helping routines
 * that define a specific local prefix (using DefineOpcodePrefix),
 * then only need to call ResetToDefaultOpcodePrefix.
 *
 * Note: automatically calls DefineOpcodePrefix on the given prefix.
 */
void DefineDefaultOpcodePrefix(const OpcodePrefix prefix);

/* Resets the default opcode prefix to the value of the last
 * call to DefineDefaultOpcodePrefix.
 */
void ResetToDefaultOpcodePrefix();

/* By default, an opcode can only have either zero or one choice.
 * If you want to define more that one entry for an opcode sequence,
 * you must register the number expected with a call to this function.
 * Note: Assumes the current opcode prefix should be applied.
 */
void DefineOpcodeChoices(const uint8_t opcode, const int count);

/* Same as DefineOpcodeChoices, but extends the opcode with the
 * opcode in the modrm byte.
 */
void DefineOpcodeMrmChoices(const uint8_t opcode,
                            const OperandKind modrm_opcode,
                            const int count);

/* Same as DefineOpcodeChoices, but you can explicitly define the
 * prefix associated with the opcode.
 */
void DefinePrefixOpcodeChoices(const OpcodePrefix prefix,
                               const uint8_t opcode,
                               const int count);

/* Same as DefinePrefixOpcodeChoices, but extends the opcode with
 * the opcode in the modrm byte.
 */
void DefinePrefixOpcodeMrmChoices(const OpcodePrefix prefix,
                                  const uint8_t opcode,
                                  const OperandKind modrm_opcode,
                                  const int count);

/* Same as DefineOpcodeChoices, except that the counts for
 * the 32 and 64 bit models can be separately epressed.
 */
void DefineOpcodeChoices_32_64(const uint8_t opcode,
                               const int count_32,
                               const int count_64);

/* Same as DefineOpcodeChoices_32_64, but extends the opcode with
 * the opcode in the modrm byte.
 */
void DefineOpcodeMrmChoices_32_64(const uint8_t opcode,
                                  const OperandKind modrm_opcode,
                                  const int count_32,
                                  const int count_64);

/* Same as DefineOpcodeChoices_32_64, but you can explicitly define the
 * prefix associated with the opcode.
 */
void DefinePrefixOpcodeChoices_32_64(const OpcodePrefix prefix,
                                     const uint8_t opcode,
                                     const int count_32,
                                     const int count_64);

/* Same as DefinePrefixOpcodeChoices_32_64, but extends the opcode with
 * the opcode in the modrm byte.
 */
void DefinePrefixOpcodeMrmChoices_32_64(const OpcodePrefix prefix,
                                        const uint8_t opcode,
                                        const OperandKind modrm_opcode,
                                        const int count_32,
                                        const int count_64);

/* By default, sanity checks are applied as each defining
 * call is made. When this is called, these sanity checks
 * are turned off until the explicit call to ApplySanityChecks.
 */
void DelaySanityChecks();

void ApplySanityChecks();

/* Define the next opcode (instruction), initializing with
 * no operands.
 */
void DefineOpcode(
    const uint8_t opcode,
    const NaClInstType insttype,
    OpcodeFlags flags,
    const InstMnemonic name);

/* Defines a specific sequence of byte codes for which the next DefineOpcode
 * should apply. When specified, restricts the match to be only defined for
 * that specific sequence of characters.
 */
void DefineOpcodeSequence(const char* opcode_seq);

/* Add additional opcode flags to the current instruction being processed. */
void AddOpcodeFlags(OpcodeFlags more_flags);

/*
 * Define the next operand of the current opcode to have the given kind
 * and flags.
 */
void DefineOperand(OperandKind kind, OperandFlags flags);

/* Add additional operand flags to the indexed operand of the current
 * instruction being processed (index is 0 based).
 */
void AddOperandFlags(uint8_t operand_index, OperandFlags more_flags);


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
