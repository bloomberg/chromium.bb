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

/* Note: This code has been rewritten to reduce the memory foot print
 * of arm instruction tries. The problem was that the amount of space
 * allocated (in 32-bit compilations) is just about 2.7 Gb's of memory.
 * Hence, we were fighting the wall for memory space. To fix this,
 * we have modified the code to allocate large blocks of memory, and
 * (trivially) allocate space from these large blocks, even at the
 * cost of not being able to free the memory. In exchange, we get
 * back about 0.7Gb's of memory that is lost if we go through the
 * default memory allocator.
 *
 * To minimize this decision, and to be able to change it, we
 * have explicitly defined allocation/free routines for structures
 * ArmInstructionTrie and ArmInstructionList. Therefore, and changes
 * only require rewriting the bodies of these functions.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/shared/utils/formatting.h"

#include "native_client/src/trusted/validator_arm/arm_categorize.h"
#include "native_client/src/trusted/validator_arm/arm_cat_compress.h"
#include "native_client/src/trusted/validator_arm/arm_categorize_memman.h"
#include "native_client/src/trusted/validator_arm/arm_inst_modeling.h"

#define DEBUGGING 0
#include "native_client/src/trusted/validator_arm/debugging.h"

/* When non-negative, specifies the number of instruction patterns
 * to install into the instruction trie. Used as a debugging tool
 * that allows testing on small tries.
 */
int FLAGS_patterns = -1;

/* Default size for character buffers. */
#define BUFFER_SIZE 1024

/*
 * Returns the name for the corresponding bit pattern.
 */
static const char* GetArmBitKindName(ArmBitKind kind) {
  if (kind < ArmBitKindSize) {
    static const char* kArmBitKindName[ArmBitKindSize] = {
      "u",
      "0",
      "1",
      "n",
      "c",
      "a"
    };
    return kArmBitKindName[kind];
  } else {
    return "?";
  }
}

ArmBitKind IncrementArmBitKind(ArmBitKind kind) {
  assert(kind < ArmBitKindSize);
  return kind + 1;
}

/*
 * Returns true if the bit pattern should be considered
 * concrete (or multiple).
 */
static Bool IsArmBitKindAbstract(ArmBitKind kind) {
  if (kind >= ArmBitKindSize) {
    /* This shouldn't happen, but be safe. */
    return TRUE;
  } else {
    static Bool kIsAbstract[ArmBitKindSize] = {
      TRUE,
      FALSE,
      FALSE,
      TRUE,
      TRUE,
      TRUE
    };
    return kIsAbstract[kind];
  }
}

/*
 * Build a Arm instruction trie node for the given bit having the
 * specified bit pattern.
 */
static ArmInstructionTrie* CreateArmInstructionTrie(int bit_index,
                                                    ArmBitKind  bit_kind) {
  int i;
  ArmInstructionTrie* trie = MallocArmInstructionTrie();
  /* If debugging, force order of print identifiers to match creation order!
   * This makes comparison between various construction parameters easier to
   * track.
   */
  DEBUG(GetTrieNodeId(trie));
  trie->bit_index = bit_index;
  trie->bit_kind = bit_kind;
  trie->parent = NULL;
  trie->matching_instructions = NULL;
  for (i = 0; i < 2; ++i) {
    trie->successors[i] = NULL;
  }
  trie->compressed_index = 0;
  return trie;
}

/*
 * Print into the given buffer the sequence of bit patterns defined by the
 * bit array of bit patterns. Returns true if buffer overflow doesn't occur.
 */
static Bool DescribeArmInstructionBits(char* buffer,
                                       size_t buffer_size,
                                       ArmInstructionBits* bits) {
  int i;
  size_t cursor = 0;
  FormatAppend(buffer, buffer_size, "[", &cursor);
  for ( i = ARM_BITS_PER_INSTRUCTION - 1; i >= 0; --i) {
    FormatAppend(buffer,
                 buffer_size,
                 GetArmBitKindName((*bits)[i]),
                 &cursor);
  }
  FormatAppend(buffer, buffer_size, "]", &cursor);
  return cursor < buffer_size;
}

/*
 * Print out the sequence of bit patterns defined for ARM instruction
 * with the given index, followed by a description of the given
 * corresponding instruction bit pattern.
 */
static void PrintIndexedInstructionBits(ArmInstructionBits* bits,
                                        int inst_index) {
  char buffer[BUFFER_SIZE];
  DescribeArmInstructionBits(buffer, sizeof(buffer), bits);
  printf("bits = %s\n", buffer);
  PrintIndexedInstruction(inst_index);
}

/*
 * Print out the sequence of bit patterns defined for ARM instruction
 * with the given index, followed by a description of the corresponding
 * instruction pattern that generated it.
 */
static void PrintBitIndexedInstruction(ArmInstructionTrieData* data,
                                       int inst_index) {
  PrintIndexedInstructionBits(&(data->arm_instruction_bits[inst_index]),
                              inst_index);
}

/*
 * Convert the given bit of the instruction value pattern to
 * a corresponding bit pattern.
 */
static ArmBitKind DecodeArmBit(int32_t value, int index) {
  switch (value) {
    case NOT_USED:
      return ArmBitUndefined;
    case ANY:
      return ArmBitAny;
    case NON_ZERO:
      return ArmBitNonZero;
    case CONTAINS_ZERO:
      return ArmBitContainsZero;
    default:
      return GetBit(value, index) ? ArmBitOne : ArmBitZero;
  }
}

/*
 * Set the given bit pattern at the given index (of the sequence of bits) to
 * the specified bit pattern. Report an error if a bit in a bit pattern is
 * set to two different patterns.
 */
static void SetArmBit(ArmInstructionBits* bits,
                      int index,
                      ArmBitKind kind,
                      int inst_index) {
  ArmBitKind prior_kind = (*bits)[index];
  if (ArmBitUndefined == prior_kind) {
    (*bits)[index] = kind;
  } else {
    fprintf(stdout,
            "*error*, bit %d defined as both %s and %s in:\n",
            index,
            GetArmBitKindName(prior_kind),
            GetArmBitKindName(kind));
    PrintIndexedInstruction(inst_index);
  }
}

/*
 * Define an enumerated type for the different fields in the
 * structure InstValues.
 */
typedef enum {
  InstValuesCondField,
  InstValuesPrefixField,
  InstValuesOpcodeField,
  InstValuesArg1Field,
  InstValuesArg2Field,
  InstValuesArg3Field,
  InstValuesArg4Field,
  InstValuesShiftField,
  InstValuesImmediateField,
  /* Special marker for end of list. */
  InstValuesFieldsSize
} InstValuesFields;

/*
 * Given the enumerated value for a given field in the InstValues structure,
 * return the corresponding value of that field.
 */
static int32_t GetInstValuesField(const InstValues* values,
                                  InstValuesFields field) {
  switch (field) {
    case InstValuesCondField:
      return values->cond;
    case InstValuesPrefixField:
      return values->prefix;
    case InstValuesOpcodeField:
      return values->opcode;
    case InstValuesArg1Field:
      return values->arg1;
    case InstValuesArg2Field:
      return values->arg2;
    case InstValuesArg3Field:
      return values->arg3;
    case InstValuesArg4Field:
      return values->arg4;
    case InstValuesShiftField:
      return values->shift;
    case InstValuesImmediateField:
      return values->immediate;
    default:
      /* This shouldn't happen. */
      fprintf(stderr, "*error*: Unknown InstValues field specified: %d\n",
              field);
      return 0;
  }
}

/*
 * Fill arm_bit_masks with the appropriate bit masks.
 */
static void InstallArmBitMasks(ArmInstructionTrieData* data) {
  int i;
  ArmInstType type;
  InstValuesFields field;
  const InstValues* mask;
  int32_t mask_value;

  for (type = ARM_UNDEFINED; type < ARM_INST_TYPE_SIZE; ++type) {
    ArmBitMasks* bit_masks = &(data->arm_bit_masks[type]);

    /* Initialize to being in no bit mask. */
    for (i = 0; i < ARM_BITS_PER_INSTRUCTION; ++i) {
      (*bit_masks)[i] = 0x0;
    }

    /* Now walk instruction pattern and fill bits. */
    mask = GetArmInstMasks(type);
    for (field = InstValuesCondField; field < InstValuesFieldsSize; ++field) {
      mask_value = GetInstValuesField(mask, field);

      /* Install masks for each bit in mask. */
      for (i = 0; i < ARM_BITS_PER_INSTRUCTION; ++i) {
        if (GetBit(mask_value, i)) {
          int32_t prior_mask = (*bit_masks)[i];
          if (0 == prior_mask) {
            (*bit_masks)[i] = mask_value;
          } else {
            printf("*error* %s - Bit %d defined as both %08x and %08x\n",
                   GetArmInstTypeName(type),
                   i,
                   prior_mask,
                   mask_value);
          }
        }
      }
    }
    DEBUG(printf("masks[%s]:\n", GetArmInstTypeName(type));
          for (i = 0; i < ARM_BITS_PER_INSTRUCTION; ++i) {
            printf("  [%2d] = %08x\n", i, (*bit_masks)[i]);
          })
  }
}

/*
 * Extract the bit patterns defined by the given instruction (index) into
 * the given instruction bits.
 */
static void ExtractArmBitPattern(int inst_index, ArmInstructionBits* bits) {
  int i;
  InstValuesFields field;
  const InstValues* mask;
  const ModeledOpInfo* inst;

  /* Start by clearing bits. */
  inst = modeled_arm_instruction_set.instructions[inst_index];
  for (i = 0; i < ARM_BITS_PER_INSTRUCTION; ++i) {
    (*bits)[i] = ArmBitUndefined;
  }

  /* Now walk instruction pattern and fill bits. */
  mask = GetArmInstMasks(inst->inst_type);
  for (field = InstValuesCondField; field < InstValuesFieldsSize; ++field) {
    int rightmost_bit = ARM_BITS_PER_INSTRUCTION;
    int32_t mask_value = GetInstValuesField(mask, field);
    int32_t field_value = GetInstValuesField(&(inst->expected_values), field);
    DEBUG(printf("processing field: %d\n", field));

    /* Do some trivial sanity checks for consistency. */
    if (0 == mask_value && NOT_USED != field_value) {
      fprintf(stdout,
              "*error* mask zero but field != NOT_USED: %d\n", field);
      PrintIndexedInstruction(inst_index);
    } else if (NOT_USED == field_value && 0 != mask_value) {
      fprintf(stdout,
              "*error* field value NOT_USED but mask != 0: %d\n", field);
      PrintIndexedInstruction(inst_index);
    }

    /* install bit pattern for the instruction. */
    for (i = 0; i < ARM_BITS_PER_INSTRUCTION; ++i) {
      if (GetBit(mask_value, i)) {
        ArmBitKind bit_kind;
        if (ARM_BITS_PER_INSTRUCTION == rightmost_bit) {
          rightmost_bit = i;
          DEBUG(printf("rightmost_bit = %d\n", i));
        }
        bit_kind = DecodeArmBit(field_value, i - rightmost_bit);
        SetArmBit(bits, i, bit_kind, inst_index);
      }
    }
  }

  /* Verify all bits  are defined (sanity check). */
  for (i = 0; i < ARM_BITS_PER_INSTRUCTION; ++i) {
    if (ArmBitUndefined == (*bits)[i]) {
      char buffer[BUFFER_SIZE];
      DescribeArmInstructionBits(buffer, sizeof(buffer), bits);
      fprintf(stdout,
              "*error* Instruction defines undefined bits %s in:\n",
              buffer);
      PrintIndexedInstruction(inst_index);
      break;
    }
  }
  DEBUG(PrintIndexedInstructionBits(bits, inst_index));
}

/*
 * Adds the given isntruction index to the list of instructions associated with
 * the given trie node.
 */
static void AddInstructionIndexToList(int inst_index,
                               ArmInstructionTrie* trie) {
  ArmInstructionList* node = MallocArmInstructionListNode();
  node->index = inst_index;
  node->next = trie->matching_instructions;
  trie->matching_instructions = node;
}

static ArmInstructionTrie* CreateEmptyArmInstructionTrie() {
  return CreateArmInstructionTrie(ARM_BITS_PER_INSTRUCTION, ArmBitUndefined);
}

/*
 * Check any  prefix bit of the corresponding masked bits (corresponding to a
 * prefix of the given trie node), matches the bit pattern wanted.
 */
static Bool TriePrefixHasMaskedValue(ArmInstructionTrie* node,
                                     ArmBitKind value,
                                     int32_t mask) {
  while (NULL != node->parent) {
    if (GetBit(mask, node->bit_index) && node->bit_kind == value) {
      return TRUE;
    }
    node = node->parent;
  }
  return FALSE;
}

/*
 * Assume that the non-zero bit pattern can be matched at the given bit index,
 * for the given indexed instruction, based on the prefix bit pattern defined
 * by the corresponding trie node.
 */
static Bool ArmBitNonZeroMatches(ArmInstructionTrieData* data,
                                 ArmInstructionTrie* node,
                                 int inst_index,
                                 int bit_index) {
  /* Can be non-zero if either (a) masked prefix has a one or non-zero, or
   * (b) not last bit of mask.
   */
  const ModeledOpInfo* info;
  int32_t mask;
  int i;

  info = modeled_arm_instruction_set.instructions[inst_index];
  mask = data->arm_bit_masks[info->inst_type][bit_index];
  if (TriePrefixHasMaskedValue(node, ArmBitOne, mask) ||
      TriePrefixHasMaskedValue(node, ArmBitNonZero, mask)) {
    return TRUE;
  } else {
    for (i = 0; i < bit_index; ++i) {
      if (GetBit(mask, i)) return TRUE;
    }
    return FALSE;
  }
}

/*
 * Assume that the contains zero bit pattern can be matched at the given bit
 * index, for the given indexed instruction, based on the prefix bit pattern
 * defined by the corresponding trie node.
 */
static Bool ArmBitContainsZeroMatches(ArmInstructionTrieData* data,
                                      ArmInstructionTrie* node,
                                      int inst_index,
                                      int bit_index) {
  /* Can contain zero if either (a) masked prefix has a zero or contains zero,
   * or (b) not last bit of mask.
   */
  const ModeledOpInfo* info;
  int32_t mask;
  int i;

  info = modeled_arm_instruction_set.instructions[inst_index];
  mask = data->arm_bit_masks[info->inst_type][bit_index];
  if (TriePrefixHasMaskedValue(node, ArmBitZero, mask) ||
      TriePrefixHasMaskedValue(node, ArmBitContainsZero, mask)) {
    return TRUE;
  } else {
    for (i = 0; i < bit_index; ++i) {
      if (GetBit(mask, i)) return TRUE;
    }
    return FALSE;
  }
}

/*
 * Assume that a one can be matched at the given bit index, for the given
 * indexed instruction, based on the prefix bit pattern defined
 * by the corresponding trie node.
 */
static Bool ArmBitKindCanBeOne(ArmInstructionTrieData* data,
                               ArmInstructionTrie* node,
                               int inst_index,
                               int bit_index,
                               ArmBitKind kind) {
  switch (kind) {
    case ArmBitOne:
    case ArmBitNonZero:
    case ArmBitAny:
      return TRUE;
    case ArmBitContainsZero:
      return ArmBitContainsZeroMatches(data, node, inst_index, bit_index);
    default:
      return FALSE;
  }
}

/*
 * Assume that a zero can be matched at the given bit index, for the given
 * indexed instruction, based on the prefix bit pattern defined by the
 * corresponding trie node.
 */
static Bool ArmBitKindCanBeZero(ArmInstructionTrieData* data,
                                ArmInstructionTrie* node,
                                int inst_index,
                                int bit_index,
                                ArmBitKind kind) {
  switch (kind) {
    case ArmBitZero:
    case ArmBitContainsZero:
    case ArmBitAny:
      return TRUE;
    case ArmBitNonZero:
      return ArmBitNonZeroMatches(data, node, inst_index, bit_index);
    default:
      return FALSE;
  }
}

/*
 * Assume that a bit of the given bit pattern can be matched at the given bit
 * index, for the the given indexed instruction, based on the prefix bit pattern
 * defined by the corresponding trie node.
 */
static Bool ArmBitKindCanBeKind(ArmInstructionTrieData* data,
                                ArmInstructionTrie* node,
                                int inst_index,
                                int bit_index,
                                ArmBitKind kind) {
  switch (kind) {
    case ArmBitNonZero:
      return ArmBitNonZeroMatches(data, node, inst_index, bit_index);
    case ArmBitContainsZero:
      return ArmBitContainsZeroMatches(data, node, inst_index, bit_index);
    default:
      return TRUE;
  }
}

/*
 * Given a trie node, make sure that there is a successor node for the given
 * instruction (index), at the given bit, for the given bit pattern.
 * Returns the built successor trie node.
 */
static ArmInstructionTrie* AddSuccessorTrie(ArmInstructionTrie* node,
                                            int inst_index,
                                            int bit_index,
                                            ArmBitKind inst_kind,
                                            Bool succ_true) {
  ArmInstructionTrie* succ = node->successors[succ_true];
  if (NULL == succ) {
    succ = CreateArmInstructionTrie(bit_index, inst_kind);
    succ->parent = node;
    node->successors[succ_true] = succ;
  }
  AddInstructionIndexToList(inst_index, succ);
  return succ;
}

/*
 * Given the node of a trie with the corresponding list of applicable
 * instructions, expand out the trie to define all possible sequence
 * of bit patterns that can be concretely realizable. Returns
 * the instruction trie node generated by the flush.
 *
 * Note: Unless compression is being done simultaneously with the build,
 * the return value will always be the same as argument node. When compressing,
 * this function will return the corresponding compressed trie node for the
 * passed node.
 *
 * Argument number_leaves_visited is only updated if compression is also
 * being done. In that case, its value will be updated to the number of leaves
 * flushed out that have at least one instrution associated with them.
 */
static ArmInstructionTrie* FlushOutInstructionTrie(
    ArmInstructionTrieData* data,
    ArmInstructionTrie* node,
    size_t* number_leaves_visited) {
  int instructions[MAX_ARM_INSTRUCTION_SET_SIZE];
  int num_instructions;
  ArmInstructionList* next_inst;
  ArmBitKind first_kind;
  ArmBitKind inst_kind;
  int bit_index;
  Bool first_insertion;
  Bool all_same;
  int inst_index;
  int i;
  ArmInstructionTrie* previous_old_succ = NULL;
  ArmInstructionTrie* previous_new_succ = NULL;

  if (NULL == node) return NULL;

  DEBUG(printf("-> flush(%d:%d)\n", GetTrieNodeId(node), node->bit_index));

  /* Start by building successors */
  if (0 != node->bit_index) {
    /* Collect list of instructions to flush out. */
    num_instructions = 0;
    next_inst = node->matching_instructions;
    while (NULL != next_inst) {
      instructions[num_instructions++] = next_inst->index;
      next_inst = next_inst->next;
    }
    if (0 == num_instructions) {
      DEBUG(printf("<- flush(%d:%d -> %d, no instructions!)\n",
                   GetTrieNodeId(node),
                   node->bit_index,
                   GetTrieNodeId(node)));
      return NULL;
    }

    bit_index = node->bit_index - 1;
    first_insertion = TRUE;
    first_kind = ArmBitUndefined;
    all_same = TRUE;        /* until proven otherwise. */

    /* First see if successor can be handled by "same" pattern". */
    for (i = 0; i < num_instructions; ++i) {
      int inst_index = instructions[i];
      inst_kind = data->arm_instruction_bits[inst_index][bit_index];
      if (first_insertion) {
        first_kind = inst_kind;
        first_insertion = FALSE;
      }
      if (inst_kind != first_kind ||
          !ArmBitKindCanBeKind(data, node, inst_index, bit_index, inst_kind)) {
        all_same = FALSE;
        break;
      }
    }

    /* Now install instructions. */
    for (i = num_instructions - 1; i >= 0; --i) {
      inst_index = instructions[i];
      inst_kind = data->arm_instruction_bits[inst_index][bit_index];
      if (all_same) {
        node->successors[TRUE] =
            AddSuccessorTrie(node, inst_index, bit_index, inst_kind, FALSE);
      } else if (IsArmBitKindAbstract(inst_kind)) {
        if (ArmBitKindCanBeOne(data, node, inst_index, bit_index, inst_kind)) {
          AddSuccessorTrie(node, inst_index, bit_index, ArmBitOne, TRUE);
        }
        if (ArmBitKindCanBeZero(data, node, inst_index, bit_index, inst_kind)) {
          AddSuccessorTrie(node, inst_index, bit_index, ArmBitZero, FALSE);
        }
      } else {
        AddSuccessorTrie(node, inst_index, bit_index, inst_kind,
                         (ArmBitZero == inst_kind ? FALSE : TRUE));
      }
    }

    /* Now flush out the successors of this node. Note: be sure to
     * to visit unique successors only once.
     */
    for (i = 0; i < 2; ++i) {
      ArmInstructionTrie* old_succ = node->successors[i];
      if (old_succ == previous_old_succ) {
        node->successors[i] = previous_new_succ;
      } else {
        ArmInstructionTrie* new_succ =
            FlushOutInstructionTrie(data, old_succ, number_leaves_visited);
        node->successors[i] = new_succ;
        previous_old_succ = old_succ;
        previous_new_succ = new_succ;
      }
    }
  }

  /* Finially, if we should compress also, do the compression. */
  if (NULL == data->compressed_nodes) {
    DEBUG(printf("<- flush(%d:%d->%d) done\n",
                 GetTrieNodeId(node),
                 node->bit_index,
                 GetTrieNodeId(node)));
  } else {
    /* Keep track of the input argument, so that at the
     * end of the method, we can show what it was updated to.
     */
    DEBUG(ArmInstructionTrie* in_node = node);
    node = CompressNode(data, node, FALSE, number_leaves_visited);
    DEBUG(printf("<- flush(%d:%d->%d) done\n",
               GetTrieNodeId(in_node),
               in_node->bit_index,
               GetTrieNodeId(node)));
  }
  return node;
}

void InitializeArmInstructionTrieData(ArmInstructionTrieData* data) {
  int i;
  int install_size =
      ((FLAGS_patterns < 0 || FLAGS_patterns > modeled_arm_instruction_set.size)
       ? modeled_arm_instruction_set.size
       : FLAGS_patterns);
  data->root = CreateEmptyArmInstructionTrie();
  InstallArmBitMasks(data);

  /* Model instruction bits and install into trie root. */
  for (i = install_size - 1; i >= 0; --i) {
    ArmInstructionBits* bits = &(data->arm_instruction_bits[i]);
    ExtractArmBitPattern(i, bits);
    AddInstructionIndexToList(i, data->root);
  }
  data->compressed_nodes = NULL;
}

void BuildArmInstructionTrie(ArmInstructionTrieData* data, Bool compress_also) {
  size_t number_leaves_visited = 0;
  if (compress_also) {
    data->compressed_nodes = CreateArmTrieMergeSet();
  }
  /* Compute decision trie for given instructions. */
  FlushOutInstructionTrie(data, data->root, &number_leaves_visited);
  if (compress_also) {
    printf("*Note*: compressed %d uniquely categorized patterns\n",
           number_leaves_visited);
    DestroyArmTrieMergeSet(data->compressed_nodes);
    data->compressed_nodes = NULL;
  }
}

/*
 * Print out the list of matched instructions (and thier corresponding
 * bit patterns).
 */
static void PrintArmInstructionList(ArmInstructionTrieData* data,
                                    ArmInstructionList* node) {
  while (NULL != node) {
    PrintBitIndexedInstruction(data, node->index);
    node = node->next;
  }
}

/*
 * Print out the sequence of bit patterns that define how this
 * trie node is reachable.
 */
static void PrintArmInstructionTriePrefix(ArmInstructionTrie* node) {
  if (NULL == node->parent) {
    return;
  } else {
    PrintArmInstructionTriePrefix(node->parent);
    printf("%s", GetArmBitKindName(node->bit_kind));
  }
}

/*
 * Walk the generated sub- trie of possible (instruction) bit patterns, printing
 * out the nodes specified by the given form. During the walk, increment
 * global variable "number_visited_leaves" as appropriate. Returns the
 * number of leaves visited while printing the trie.
 */
static int PrintArmInstructionTrieNode(ArmInstructionTrieData* data,
                                       ArmInstructionTrie* node,
                                       PrintArmInstructionForm form) {
  if (NULL == node) {
    return 0;
  } else {
    ArmInstructionTrie* previous_succ;
    int i;
    int number_visited_leaves = 0;
    DEBUG(printf("-> print %d\n", GetTrieNodeId(node)));
    /* Start by printing internal node information if all nodes
     * should be printed.
     */
    if (kPrintArmInstructionFormAll == form) {
      Bool has_succs = FALSE;
      printf("node: %p\n", (void*) node);
      for (i = 0; i < 2; ++i) {
        ArmInstructionTrie* succ = node->successors[i];
        if (NULL != succ) {
          if (!has_succs) {
            printf("successors:\n");
            has_succs = TRUE;
          }
          printf("  %d -> %p\n", i, (void*) succ);
        }
      }
    }

    /* Now print out leaf-specific pattern matches */
    if (0 == node->bit_index) {
      ++number_visited_leaves;
    }
    if (kPrintArmInstructionFormAll == form ||
        (0 == node->bit_index &&
         (kPrintArmInstructionFormLeaf == form
          || NULL != node->matching_instructions->next))) {
      if (kPrintArmInstructionFormAll != form
          && NULL != node->matching_instructions->next) {
        printf("*warn* Multiple instructions for pattern!\n");
      }
      printf("Case: ");
      PrintArmInstructionTriePrefix(node);
      printf("\n");
      PrintArmInstructionList(data, node->matching_instructions);
    }

    /* Finally, print out successor nodes. Note: be sure to
     * only print unique successors.
     */
    previous_succ = NULL;
    for (i = 0; i < 2; ++i) {
      ArmInstructionTrie* succ = node->successors[i];
      if (succ != previous_succ) {
        number_visited_leaves += PrintArmInstructionTrieNode(data, succ, form);
        previous_succ = succ;
      }
    }
    DEBUG(printf("<- print %d = %d\n",
                 GetTrieNodeId(node),
                 number_visited_leaves));
    return number_visited_leaves;
  }
}

int PrintArmInstructionTrie(ArmInstructionTrieData* data,
                         PrintArmInstructionForm form) {
  return PrintArmInstructionTrieNode(data, data->root, form);
}

/*
 * Walk the generated sub-trie of bit patterns, looking for possible
 * violations (i.e. where the trie isn't a decision trie on
 * the sequence of bits in an instruction).
 */
static void ValidateArmDecisionTrieNode(ArmInstructionTrieData* data,
                                        ArmInstructionTrie* node) {
  Bool has_zero;
  Bool has_one;
  int succ_count;
  int i;
  ArmInstructionTrie* succ;
  ArmInstructionTrie* previous_succ;

  has_zero = FALSE;
  has_one = FALSE;
  succ_count = 0;

  /* Note: be sure to only validate successors once. That is, if both
   * successors point to the same trie node, only recursively check one
   * of the successors.
   */
  previous_succ = NULL;
  for (i = 0; i < 2; ++i) {
    succ = node->successors[i];
    if (previous_succ != succ && NULL != succ) {
      ValidateArmDecisionTrieNode(data, succ);
      ++succ_count;
      switch (succ->bit_kind) {
        case ArmBitZero:
          has_zero = TRUE;
          break;
        case ArmBitOne:
          has_one = TRUE;
          break;
        default:
         /* nothing to do */
         break;
      }
      previous_succ = succ;
    }
  }
  if (succ_count > 1) {
    int count = 0;
    if (has_zero) ++count;
    if (has_one) ++count;
    if (succ_count != count) {
      printf("*error* bad decision prefix: ");
      PrintArmInstructionTriePrefix(node);
      printf("\n");
    }
  }
}

void ValidateArmDecisionTrie(ArmInstructionTrieData* data) {
  ValidateArmDecisionTrieNode(data, data->root);
}
