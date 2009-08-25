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
 * Model a trie of possible instruction decodings, and check them
 * for consistency.
 */

#ifndef NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_CATEGORIZE_H__
#define NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_CATEGORIZE_H__

#include "native_client/src/trusted/validator_arm/arm_insts.h"

EXTERN_C_BEGIN

/*
 * Defines how many bits in an ARM instruction.
 */
#define ARM_BITS_PER_INSTRUCTION 32

/*
 * Define a (singly) linked list of instructions.
 */
typedef struct ArmInstructionList {
  /* Index of instruction matched (in the list of defined instructions. */
  int index;
  /* The next instruction in the list. */
  struct ArmInstructionList* next;
} ArmInstructionList;

/*
 * Define possible patterns for bits that can appear in
 * extracted instruction matching.
 */
typedef enum {
  /* Assumed to be first in list. Denotes that the
   * possible values for the bit is not defined.
   */
  ArmBitUndefined,
  /* Assumes the bit is set to zero. */
  ArmBitZero,
  /* Assumes the bit is set to one. */
  ArmBitOne,
  /* Assumes that the sequence of bits this bit is associated with
   * should contain an least one non-zero bit.
   */
  ArmBitNonZero,
  /* Assumes that the sequence of bits this bit is associated with
   * should contain at least one zero bit.
   */
  ArmBitContainsZero,
  /* Assumes that the sequence of bits this bit is associated with
   * can be any sequence of zeroes and ones.
   */
  ArmBitAny,
  /* Special end of list marker. */
  ArmBitKindSize
} ArmBitKind;

/*
 * Increments to the next ArmBitKind in the enumerated type.
 */
ArmBitKind IncrementArmBitKind(ArmBitKind kind);

/*
 * Defines the bit pattern extracted for each bit in the instruction/
 */
typedef ArmBitKind ArmInstructionBits[ARM_BITS_PER_INSTRUCTION];

/*
 * Defines a type to hold the sequence of bit masks used to
 * extact out each bit of an instruction.
 */
typedef int32_t ArmBitMasks[ARM_BITS_PER_INSTRUCTION];

/*
 * Model a (lexicon) trie of possible bit matches to
 * the set of arm instructions (by matching left to right).
 * This structure lets us categorize the set of implemented
 * instructions, as well as automatically generate a matcher
 * for instructions.
 */
typedef struct ArmInstructionTrie {
  /* Index of the bit being matched in the instruction.
   * Note: 32 implies to the left of the leftmost bit in
   * the instruction.
   */
  int bit_index;
  /* The set of possible values for the indexed bit. */
  ArmBitKind bit_kind;
  /* The parent node in the trie for this node (i.e. the one
   * that defines the bit immediately to the left (+1) of this
   * bit.
   */
  struct ArmInstructionTrie* parent;
  /* The list of instructions that can be matched by the sequences
   * of bits to the left of this (including this).
   */
  ArmInstructionList* matching_instructions;
  /* Defines successor based on 0/1 bit values. Note:
   * If one can match any bit value, we don't build two
   * distinct successors. Rather, both entries in the
   * array point to the same node.
   */
  struct ArmInstructionTrie* successors[2];
  /* Special index added during compression. Value defaults to zero. */
  int compressed_index;
} ArmInstructionTrie;

/*
 * define choices for printing out the generated trie of
 * possible bit patterns.
 */
typedef enum {
  /* Print all nodes in the generated trie. */
  kPrintArmInstructionFormAll,
  /* Only print leaf (i.e. fully matched bit patterns) nodes. */
  kPrintArmInstructionFormLeaf,
  /* Only print out trie nodes that define more than one
   * possible match to an instruction.
   */
  kPrintArmInstructionFormError
} PrintArmInstructionForm;

/* Models (in arm_compress.cc) a set of merged instruction trie nodes. */
struct ArmTrieMergeSet;

/*
 * Defines data used to construct an arm instruction trie of possible
 * arm instruction matches.
 */
typedef struct {
  /* Defines the trie to build. */
  ArmInstructionTrie* root;
  /* Defines the sequence of bit patterns defined for each ARM instruction
   * in the trie. */
  ArmInstructionBits arm_instruction_bits[MAX_ARM_INSTRUCTION_SET_SIZE];
  /* Holds the set of bit masks used to extract each bit of an instruciton,
   * for each type of instruction.
   */
  ArmBitMasks arm_bit_masks[ARM_INST_TYPE_SIZE];
  /* Defined during the compress phase. Holds the set of instruction trie
   * nodes that have been compressed so far.
   */
  struct ArmTrieMergeSet* compressed_nodes;
} ArmInstructionTrieData;

/*
 * Initialize the given ArmInstructionTrieData.
 */
void InitializeArmInstructionTrieData(ArmInstructionTrieData* data);

/*
 * Builds the instruction trie of possible instructions, and updates
 * the corresponding instruction trie data to hold the generated
 * instruction trie. If compress_also is true, then compression happens
 * simultaneously during the build.
 *
 * Note: Once you have compressed the trie, you can no longer validate it.
 */
void BuildArmInstructionTrie(ArmInstructionTrieData* data, Bool compress_also);

/*
 * Walk the generated trie of possible (instruction) bit patterns, printing
 * out the nodes specified by the given form. During the walk, increment
 * global variable "number_visited_leaves" as appropriate. Returns the
 * number of leaves visited while printing the trie.
 */
int PrintArmInstructionTrie(ArmInstructionTrieData* data,
                            PrintArmInstructionForm form);

/*
 * Walk the generated trie of bit patterns, looking for possible
 * violations (i.e. where the trie isn't a decision trie on
 * the sequence of bits in an instruction).
 *
 * Note: Once you have compressed the trie, you can no longer validate it.
 * Calling this function on a compressed trie has unpredictable results.
 */
void ValidateArmDecisionTrie(ArmInstructionTrieData* data);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_CATEGORIZE_H__ */
