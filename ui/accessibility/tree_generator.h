// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace ui {

class AXTree;

// A class to create all possible trees with <n> nodes and the ids [1...n].
//
// There are two parts to the algorithm:
//
// The tree structure is formed as follows: without loss of generality,
// the first node becomes the root and the second node becomes its
// child. Thereafter, choose every possible parent for every other node.
//
// So for node i in (3...n), there are (i - 1) possible choices for its
// parent, for a total of (n-1)! (n minus 1 factorial) possible trees.
//
// The second part is the assignment of ids to the nodes in the tree.
// There are exactly n! (n factorial) permutations of the sequence 1...n,
// and each of these is assigned to every node in every possible tree.
//
// The total number of trees returned for a given <n>, then, is
// n! * (n-1)!
//
// n = 2: 2 trees
// n = 3: 12 trees
// n = 4: 144 trees
// n = 5: 2880 trees
//
// This grows really fast! Luckily it's very unlikely that there'd be
// bugs that affect trees with >4 nodes that wouldn't affect a smaller tree
// too.
class TreeGenerator {
 public:
  TreeGenerator(int node_count);

  int UniqueTreeCount() const;

  void BuildUniqueTree(int tree_index, AXTree* out_tree) const;

 private:
  int node_count_;
  int unique_tree_count_;
};

}  // namespace ui
