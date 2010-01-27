/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Internal implementation of the state associated with matching instructions.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_INST_STATE_INTERNAL_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_INST_STATE_INTERNAL_H_

#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"

/* The meta model of an x86 opcode instruction. */
struct Opcode;

/* Model of a code segment. */
struct NcSegment;

/* Defines the type used to align OpExpressionNodes when memory allocating. */
typedef uint64_t OpExpressionElement;

/* Model data needed to decode an x86 instruction. */
struct NcInstState {
  /* Define the start location for the bytes defining the instruction. */
  uint8_t* mpc;
  /* Define the (virtual pc) address associated with the instruction being
   * matched.
   */
  PcAddress vpc;
  /* Define the number of bytes in the instruction. */
  uint8_t length;
  /* Define the upper limit on how many bytes can be in the instruction. */
  uint8_t length_limit;
  /* Define the number of prefix bytes processed. */
  uint8_t num_prefix_bytes; /* 0..4 */
  /* Handle that gcc generates multiple 66 prefix values for nops. */
  uint8_t num_prefix_66;
  /* If REX prefix found, its value. Otherwise zero. */
  uint8_t rexprefix;
  /* If Mod/RM byte defined, its value. Otherwise zero. */
  uint8_t modrm;
  /* True only if the instruction has an SIB byte. */
  Bool has_sib;
  /* If a SIB byte is defined, its value. Otherwise zero. */
  uint8_t sib;
  /* Define the number of displacement bytes matched by the instruction. */
  uint8_t num_disp_bytes;
  /* Define the index of the first displacement byte of the instruction, or
   * zero if num_disp_bytes == 0.
   */
  uint8_t first_disp_byte;
  /* Define the number of immediate bytes defined by the instruction. */
  uint8_t num_imm_bytes;
  /* Define the index of the first immediate byte of the instruction, or
   * zero if num_imm_bytes == 0;.
   */
  uint8_t first_imm_byte;
  /* Define the number of bytes to the second immediate value if defined
   * (defaults to zero).
   */
  uint8_t num_imm2_bytes;
  /* The computed (default) operand size associated with the instruction. */
  uint8_t operand_size;
  /* The computed (default) address size associated with the instruction. */
  uint8_t address_size;
  /* True if allowed in native client. */
  Bool is_nacl_legal;
  /* The set of prefix byte kinds associated with the instruction
   * (See kPrefixXXXX #define's in ncdecode.h)
   */
  uint32_t prefix_mask;
  /* The (opcode) instruction pattern used to match the instruction.
   * Note: If this value is NULL, we have not yet tried to match
   * the current instruction with the corresponding instruction iterator.
   */
  struct Opcode* opcode;
  /* The corresponding expression tree denoted by the matched instruction. */
  ExprNodeVector nodes;
};

/* Model of an instruction iterator. */
struct NcInstIter {
  /* Defines the segment to process */
  struct NcSegment* segment;
  /* Defines the current (relative pc) index into the segment. */
  MemorySize index;
  /* Defines the index of the current instruction, relative to
   * the beginning of the segment.
   */
  MemorySize inst_count;
  /* The following fields define a ring buffer, where buffer_index
   * is the index of the current instruction in the buffer, and
   * buffer_size is the number of iterator states in the buffer.
   */
  size_t buffer_size;
  size_t buffer_index;
  struct NcInstState* buffer;
};

/* Given the current location of the (relative) pc of the given instruction
 * iterator, update the given state to hold the matched opcode
 * (instruction) pattern. If no matching pattern exists, set the state
 * to a matched undefined opcode (instruction) pattern. In all cases,
 * update the state to hold all information on the matched bytes of the
 * instruction.
 */
void DecodeInstruction(struct NcInstIter* iter, struct NcInstState* state);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_INST_STATE_INTERNAL_H_ */
