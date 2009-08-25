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

/* This file takes the arm instruction trie, and compresses common sub-tries
 * into a single trie node. This compression should allow us to generate a much
 * smaller parser for ARM instructions.
 *
 * Note: Compressed indices of compressed trie nodes correspond to the visit
 * order of the depth first spanning walk of the generated compressed trie.
 */

#include <stdio.h>

#include <map>

#include "native_client/src/trusted/validator_arm/arm_compress.h"
#include "native_client/src/trusted/validator_arm/arm_cat_compress.h"
#include "native_client/src/trusted/validator_arm/arm_categorize_memman.h"

#define DEBUGGING 0
#include "native_client/src/trusted/validator_arm/debugging.h"

typedef std::set<ArmInstructionTrie*,
                 ArmInstructionTrieLessThan> ArmTrieNodeSet;

// Model a set of mergable trie nodes.
struct ArmTrieMergeSet {
  ArmTrieNodeSet compressed_nodes_;
};

// Get the compressed index associated with given (compressed) trie node.
static inline int GetTrieCompressedIndex(const ArmInstructionTrie* node) {
  return NULL == node ? -1 : node->compressed_index;
}

struct ArmTrieMergeSet* CreateArmTrieMergeSet() {
  return new ArmTrieMergeSet();
}

void DestroyArmTrieMergeSet(struct ArmTrieMergeSet* nodes_ptr) {
  delete nodes_ptr;
}

// Get the compressed index associated with the given successor of the
// given trie node.
static inline int GetSuccessorTrieCompressedIndex(
    const ArmInstructionTrie* node,
    int bit_value) {
  return GetTrieCompressedIndex(node->successors[bit_value]);
}

// Returns true iff instruction list l1 is lexicographically smaller than l2.
static int InstructionsCompare(ArmInstructionList* l1,
                               ArmInstructionList* l2) {
  while (NULL != l1 && NULL != l2) {
    int diff = 0;
    if (l1 != l2) {
      if (l1->index < l2->index) {
        diff = -1;
      } else if (l1->index > l2->index) {
        diff = 1;
      }
    }
    if (diff != 0) return diff;
    l1 = l1->next;
    l2 = l2->next;
  }
  // Note: either l1 or l2 must be NULL to reach this statement.
  if (NULL != l1) return 1;
  if (NULL != l2) return -1;
  return 0;
}

bool ArmInstructionTrieLessThan::operator()(
    const ArmInstructionTrie* node1,
    const ArmInstructionTrie* node2) const {
  if (node1 == node2) {
    return false;
  } else if (NULL == node1) {
    return NULL != node2;
  } else if (NULL == node2) {
    return false;
  } else {
    DEBUG(printf("-> compare %d , %d\n",
                 GetTrieNodeId(node1),
                 GetTrieNodeId(node2)));
    int diff = node1->bit_index - node2->bit_index;
    if (0 != diff) {
      DEBUG(printf("<- bit index differs = %d\n", diff));
      return diff < 0;
    }
    diff = int(node1->bit_kind) - int(node2->bit_kind);
    if (0 != diff) {
      DEBUG(printf("<- bit kinds differ = %d\n", diff));
      return diff < 0;
    }
    diff = InstructionsCompare(node1->matching_instructions,
                               node2->matching_instructions);
    if (0 != diff) {
      DEBUG(printf("<- instructions differ = %d\n", diff));
      return diff < 0;
    }
    // To order successors, use the subtle property defined by
    // the order the trie is compressed. That is, nodes are added
    // via a depth-first search, merging nodes as found. Hence,
    // successors can be ordered by their ordering defined by the
    // compressed_index, since it corresponds to the order defined by the
    // depth-first spanning tree of the (about to be generated) compressed
    // trie.
    for (int i = 0; i < 2; ++i) {
      diff = GetSuccessorTrieCompressedIndex(node1, i)
          - GetSuccessorTrieCompressedIndex(node2, i);
      if (diff != 0) {
        DEBUG(printf("<- succ %d differ = %d\n", i, diff));
        return diff < 0;
      }
    }
    DEBUG(printf("<- same\n"));
    return false;
  }
}

CompressedArmInstructionTrie::CompressedArmInstructionTrie(
    ArmInstructionTrieData* trie_data)
    : trie_data_(trie_data),
      compressed_nodes_size_(0),
      number_leaves_visited_(0)
{}

// Number of patterns to process before showing a progress message.
static const int kPatternProgressInterval = 1000000;

// Compress successor nodes of the given node, updating
// the set of compressed nodes, and updating the number of
// leaves (and hence, the number of instruction patterns matched).
// Returns true if either (compressed) successor contains a leaf
// that defines an instruction match.
static bool CompressNodeSuccessors(
    ArmInstructionTrieData* data,
    ArmInstructionTrie* node,
    Bool apply_recursively,
    size_t* number_leaves_visited) {
  // Keep is true if it is a leaf that defines an instruction match,
  // or has a successor which is a leaf that defines an instruction match.
  // Note: If no leaf defines an instruction match, this path is not worth
  // following.
  bool keep = false;
  ArmInstructionTrie* previous_old_succ = NULL;
  ArmInstructionTrie* previous_new_succ = NULL;
  for (int i = 0; i < 2; ++i) {
    ArmInstructionTrie* old_succ = node->successors[i];
    if (old_succ == previous_old_succ) {
      node->successors[i] = previous_new_succ;
    } else {
      ArmInstructionTrie* new_succ =
          (apply_recursively
           ? CompressNode(data, old_succ, TRUE, number_leaves_visited)
           : old_succ);
      node->successors[i] = new_succ;
      if (NULL != new_succ) keep = true;
      previous_old_succ = old_succ;
      previous_new_succ = new_succ;
    }
  }
  return keep;
}

// Compress the given instruction node, updating the set of compressed nodes,
// and updating the number of leaves (and hence, the number of instruction
// patterns matched). Returns the compressed node (null if this subtrie
// doesn't contains any possible leaf instruction matches).
ArmInstructionTrie* CompressNode(
    ArmInstructionTrieData* data,
    ArmInstructionTrie* node,
    Bool apply_recursively,
    size_t* number_leaves_visited) {
  if (node == NULL) return NULL;

  DEBUG(ArmInstructionTrie* in_node = node);

  DEBUG(printf("-> compress %d:%d : %d\n",
               GetTrieNodeId(node),
               node->bit_index,
               *number_leaves_visited));

  // Keep is true if it is a leaf that defines an instruction match,
  // or has a successor which is a leaf that defines an instruction match.
  // Note: If no leaf defines an instruction match, this path is not worth
  // following.
  bool keep(
      CompressNodeSuccessors(data, node, apply_recursively,
                             number_leaves_visited));

  // Undefine bit_kind and parent, since it is no longer applicable,
  // once nodes are merged.
  node->bit_kind = ArmBitUndefined;
  node->parent = NULL;

  // Undefine the instruction list, unless it is a leaf, since
  // the other lists were only added during construction to
  // guarantee all applicable patterns are covered.
  // Note: we can't delete any of the instruction list nodes,
  // because they may be shared (as part of the construction).
  // Hence, we collect the candidates, and those we know that
  // can't be deleted, and wait until we finish compressing
  // before we actually delete nodes.
  if (0 == node->bit_index) {
    ArmInstructionList* next = node->matching_instructions;
    if (NULL != next) {
      keep = true;
    }
  } else {
    FreeArmInstructionList(node->matching_instructions);
    node->matching_instructions = NULL;
  }

  // Show progress so that the caller of this application will
  // not get too restless waiting.
  if (0 == node->bit_index) {
    ++(*number_leaves_visited);
    if (*number_leaves_visited % kPatternProgressInterval == 0) {
      printf("%d patterns => %d compressed nodes\n",
             static_cast<int>(*number_leaves_visited),
             static_cast<int>(data->compressed_nodes
                              ->compressed_nodes_.size()));
    }
  } else if ((32 == node->bit_index) &&
             (0 != *number_leaves_visited % kPatternProgressInterval)) {
    // Finished compression, print out final progress report.
    printf("%d patterns => %d compressed nodes\n",
           static_cast<int>(*number_leaves_visited),
           static_cast<int>(data->compressed_nodes
                            ->compressed_nodes_.size()));
  }

  // Now see if this node is in the set of compressed nodes, and reuse
  // if possible.
  if (keep) {
    ArmTrieNodeSet::iterator pos =
        data->compressed_nodes->compressed_nodes_.find(node);
    if (pos == data->compressed_nodes->compressed_nodes_.end()) {
      node->compressed_index =
          data->compressed_nodes->compressed_nodes_.size();
      data->compressed_nodes->compressed_nodes_.insert(node);
    } else {
      // Note: This was allocated by malloc in arm_categorize.c, so be
      // sure to free it in a compatable manner.
      FreeArmInstructionTrie(node);
      node = *pos;
    }
    DEBUG(printf("<- compress %d:%d -> %d : %d\n",
                 GetTrieNodeId(in_node),
                 in_node->bit_index,
                 GetTrieNodeId(node),
                 *number_leaves_visited));
    return node;
  } else {
    FreeArmInstructionTrie(node);
    DEBUG(printf("<- compress %d:%d -> null : %d\n",
                 GetTrieNodeId(in_node),
                 in_node->bit_index,
                 *number_leaves_visited));
    return NULL;
  }
}

void CompressedArmInstructionTrie::Compress() {
  printf("Compressing patterns\n");
  trie_data_->compressed_nodes = CreateArmTrieMergeSet();

  // Compress the trie.
  trie_data_->root = CompressNode(trie_data_,
                                  trie_data_->root,
                                  TRUE,
                                  &number_leaves_visited_);
  compressed_nodes_size_ = trie_data_->compressed_nodes->compressed_nodes_.size();
  DestroyArmTrieMergeSet(trie_data_->compressed_nodes);
  trie_data_->compressed_nodes = NULL;
}

int GetTrieNodeId(const ArmInstructionTrie* node) {
  static std::map<const ArmInstructionTrie*, int> trie_id_map;
  std::map<const ArmInstructionTrie*, int>::iterator pos
      = trie_id_map.find(node);
  if (pos == trie_id_map.end()) {
    int index = trie_id_map.size() + 1;
    trie_id_map.insert(std::make_pair(node, index));
    return index;
  } else {
    return pos->second;
  }
}
