// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/tree_generator.h"

#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"

namespace ui {

TreeGenerator::TreeGenerator(int node_count)
    : node_count_(node_count), unique_tree_count_(1) {
  // (n-1)! for the possible trees.
  for (int i = 2; i < node_count_; i++)
    unique_tree_count_ *= i;
  // n! for the permutations of ids.
  for (int i = 2; i <= node_count_; i++)
    unique_tree_count_ *= i;
};

int TreeGenerator::UniqueTreeCount() const {
  return unique_tree_count_;
};

void TreeGenerator::BuildUniqueTree(int tree_index, AXTree* out_tree) const {
  std::vector<int> indices;
  std::vector<int> permuted;
  CHECK(tree_index <= unique_tree_count_);

  // Use the first few bits of |tree_index| to permute the indices.
  for (int i = 0; i < node_count_; i++)
    indices.push_back(i + 1);
  for (int i = 0; i < node_count_; i++) {
    int p = (node_count_ - i);
    int index = tree_index % p;
    tree_index /= p;
    permuted.push_back(indices[index]);
    indices.erase(indices.begin() + index);
  }

  // Build an AXTreeUpdate. The first two nodes of the tree always
  // go in the same place.
  AXTreeUpdate update;
  update.nodes.resize(node_count_);
  update.nodes[0].id = permuted[0];
  update.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  update.nodes[0].state = AX_STATE_NONE;
  update.nodes[0].child_ids.push_back(permuted[1]);
  update.nodes[1].id = permuted[1];
  update.nodes[1].state = AX_STATE_NONE;

  // The remaining nodes are assigned based on their parent
  // selected from the next bits from |tree_index|.
  for (int i = 2; i < node_count_; i++) {
    update.nodes[i].id = permuted[i];
    update.nodes[i].state = AX_STATE_NONE;
    int parent_index = (tree_index % i);
    tree_index /= i;
    update.nodes[parent_index].child_ids.push_back(permuted[i]);
  }

  // Unserialize the tree update into the destination tree.
  CHECK(out_tree->Unserialize(update)) << out_tree->error();
};

}  // namespace ui
