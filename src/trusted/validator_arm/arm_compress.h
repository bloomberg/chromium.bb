/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// This file takes the arm instruction trie, and compresses common sub-tries
// into a single trie node. This compression should allow us to generate a much
// smaller parser for ARM instructions.

#ifndef NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_COMPRESS_H__
#define NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_COMPRESS_H__

#include <vector>
#include <map>
#include <set>

#include "native_client/src/trusted/validator_arm/arm_categorize.h"

// Define an ordering function for arm instruction trie nodes.
class ArmInstructionTrieLessThan {
 public:
  // Returns true iff node1 < node2.
  bool operator()(const ArmInstructionTrie* node1,
                  const ArmInstructionTrie* node2) const;
};

// Define a class to compress an ARM instruction trie (in place).
class CompressedArmInstructionTrie {
 public:
  // Define a compressed arm instruction trie from the given
  // instruction trie. Note: One needs to call method compress
  // before the compressed trie can be used.
  CompressedArmInstructionTrie(ArmInstructionTrieData* trie_data);

  // Build the compressed instruction trie from the instruction
  // trie this is built on.
  void Compress();

  // Returns the number of compressed nodes.
  size_t Size() {
    return compressed_nodes_size_;
  }

  // Returns the number of compressed leaves.
  size_t NumberLeaves() {
    return number_leaves_visited_;
  }

 private:
  // Holds the arm instruction trie to be compressed.
  ArmInstructionTrieData* trie_data_;

  // Holds the number of compressed nodes.
  size_t compressed_nodes_size_;

  // Holds the number of leaves visited during compression.
  size_t number_leaves_visited_;

  // Do not use.
  CompressedArmInstructionTrie();

  // Do not use.
  CompressedArmInstructionTrie(const CompressedArmInstructionTrie& copy);
};


#endif  // NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_ARM_COMPRESS_H__
