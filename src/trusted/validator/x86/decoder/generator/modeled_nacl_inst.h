/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * modeled_nacl_inst.h - Extends (runtime) model of instructions (NaClInst)
 * to include information needed only during table generation.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NCOPCODE_DESC_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NCOPCODE_DESC_H__

#include "native_client/src/trusted/validator/x86/decoder/ncopcode_desc.h"

/* Models metadata about an instruction, defining a pattern. Note: Since the
 * same sequence of opcode bytes may define more than one pattern (depending on
 * other bytes in the parsed instruction), the patterns are
 * modeled using a singly linked list.
 */
typedef struct NaClModeledInst {
  /* The prefix associated with the instruction. */
  NaClInstPrefix prefix;
  /* The number of opcode bytes in the instruction. */
  uint8_t num_opcode_bytes;
  /* The actual opcode bytes. */
  uint8_t opcode[NACL_MAX_ALL_OPCODE_BYTES];
  /* Defines the origin of this instruction. */
  NaClInstType insttype;
  /* Flags defining additional facts about the instruction. */
  NaClIFlags flags;
  /* The instruction that this instruction implements. */
  NaClMnemonic name;
  /* Defines opcode extentions, which encodes values for OpcodeInModRm,
   * OpcodePlusR, and OpcodeInModRmRm. Note: to fit the possible 9
   * bits of information in 8 bits, we assume that OpcodeInModRm
   * and OpcodePlusR do not happen in the same instruction.
   */
  uint8_t opcode_ext;
  /* The number of operands modeled for this instruction. */
  uint8_t num_operands;
  /* The corresponding models of the operands. */
  NaClOp* operands;
  /* Pointer to the next pattern to try and match for the
   * given sequence of opcode bytes.
   */
  struct NaClModeledInst* next_rule;
} NaClModeledInst;

/* Sets the OpcodeInModRm value in the opcode_ext field. */
void NaClSetOpcodeInModRm(uint8_t value, uint8_t *opcode_ext);

/* Sets the OpcodeInModRmRm value in th opcode_ext field. */
void NaClSetOpcodeInModRmRm(uint8_t value, uint8_t *opcode_ext);

/* Sets the OpcodePlusR value in the opcode_ext field. */
void NaClSetOpcodePlusR(uint8_t value, uint8_t *opcode_ext);

/* Implements trie nodes for selecting instructions that must match
 * a specific sequence of bytes. Used to model NOP cases.
 */
typedef struct NaClModeledInstNode {
  /* The matching byte for the trie node. */
  uint8_t matching_byte;
  /* The matching modeled instruction, if byte matched. */
  NaClModeledInst* matching_inst;
  /* Node to match remaining bytes if matching_byte matches. */
  struct NaClModeledInstNode* success;
  /* Node to try next if match_byte doesn't match. Note:
   * The trie is generated in such a way that if the next input
   * byte is > matching_byte, no node in the fail subtree will
   * match the current input. That is, nodes in the trie are
   * sorted by the sequence of matching bytes.
   */
  struct NaClModeledInstNode* fail;
} NaClModeledInstNode;

/* Print out the given instruction to the given file. However, always
 * print the value NULL for next_rule, even if the value is non-null. This
 * function should be used to print out an individual opcode (instruction)
 * pattern.
 */
void NaClModeledInstPrint(struct Gio* f,  const NaClModeledInst* inst);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NCOPCODE_DESC_H__ */
