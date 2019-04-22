// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/tree_generator.h"

#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"

namespace ui {

static int UniqueTreeCountForNodeCount(int node_count,
                                       bool permutations) {
  int unique_tree_count = 1;

  // (n-1)! for the possible trees.
  for (int i = 2; i < node_count; ++i)
    unique_tree_count *= i;

  // n! for the permutations of ids.
  if (permutations)
    unique_tree_count = unique_tree_count * unique_tree_count * node_count;

  return unique_tree_count;
}

TreeGenerator::TreeGenerator(int max_node_count, bool permutations)
    : max_node_count_(max_node_count),
      permutations_(permutations),
      total_unique_tree_count_(0) {
  unique_tree_count_by_size_.push_back(0);
  for (int i = 1; i <= max_node_count; ++i) {
    int unique_tree_count = UniqueTreeCountForNodeCount(i, permutations);
    unique_tree_count_by_size_.push_back(unique_tree_count);
    total_unique_tree_count_ += unique_tree_count;
  }
}

TreeGenerator::~TreeGenerator() {
}

int TreeGenerator::UniqueTreeCount() const {
  return total_unique_tree_count_;
}

void TreeGenerator::BuildUniqueTree(int tree_index, AXTree* out_tree) const {
  CHECK_LT(tree_index, total_unique_tree_count_);

  int unique_tree_count_so_far = 0;
  for (int node_count = 1; node_count <= max_node_count_; ++node_count) {
    int unique_tree_count = unique_tree_count_by_size_[node_count];
    if (tree_index - unique_tree_count_so_far < unique_tree_count) {
      BuildUniqueTreeWithSize(node_count,
                              tree_index - unique_tree_count_so_far,
                              out_tree);
      return;
    }
    unique_tree_count_so_far += unique_tree_count;
  }
}

void TreeGenerator::BuildUniqueTreeWithSize(
    int node_count, int tree_index, AXTree* out_tree) const {
  std::vector<int> indices;
  std::vector<int> permuted;
  int unique_tree_count = unique_tree_count_by_size_[node_count];
  CHECK_LT(tree_index, unique_tree_count);

  if (permutations_) {
    // Use the first few bits of |tree_index| to permute the indices.
    for (int i = 0; i < node_count; ++i)
      indices.push_back(i + 1);
    for (int i = 0; i < node_count; ++i) {
      int p = (node_count - i);
      int index = tree_index % p;
      tree_index /= p;
      permuted.push_back(indices[index]);
      indices.erase(indices.begin() + index);
    }
  } else {
    for (int i = 0; i < node_count; ++i)
      permuted.push_back(i + 1);
  }

  // Build an AXTreeUpdate. The first two nodes of the tree always
  // go in the same place.
  AXTreeUpdate update;
  update.root_id = permuted[0];
  update.nodes.resize(node_count);
  update.nodes[0].id = permuted[0];
  if (node_count > 1) {
    update.nodes[0].child_ids.push_back(permuted[1]);
    update.nodes[1].id = permuted[1];
  }

  // The remaining nodes are assigned based on their parent
  // selected from the next bits from |tree_index|.
  for (int i = 2; i < node_count; ++i) {
    update.nodes[i].id = permuted[i];
    int parent_index = (tree_index % i);
    tree_index /= i;
    update.nodes[parent_index].child_ids.push_back(permuted[i]);
  }

  // Unserialize the tree update into the destination tree.
  CHECK(out_tree->Unserialize(update)) << out_tree->error();
}

}  // namespace ui
