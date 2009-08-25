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
