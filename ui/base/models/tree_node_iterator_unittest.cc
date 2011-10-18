// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "ui/base/models/tree_node_iterator.h"
#include "ui/base/models/tree_node_model.h"

namespace ui {

TEST(TreeNodeIteratorTest, Test) {
  TreeNodeWithValue<int> root;
  root.Add(new TreeNodeWithValue<int>(1), 0);
  root.Add(new TreeNodeWithValue<int>(2), 1);
  TreeNodeWithValue<int>* f3 = new TreeNodeWithValue<int>(3);
  root.Add(f3, 2);
  TreeNodeWithValue<int>* f4 = new TreeNodeWithValue<int>(4);
  f3->Add(f4, 0);
  f4->Add(new TreeNodeWithValue<int>(5), 0);

  TreeNodeIterator<TreeNodeWithValue<int> > iterator(&root);
  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(root.GetChild(0), iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(root.GetChild(1), iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(root.GetChild(2), iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(f4, iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(f4->GetChild(0), iterator.Next());

  ASSERT_FALSE(iterator.has_next());
}

static bool PruneOdd(TreeNodeWithValue<int>* node) {
  return node->value % 2;
}

static bool PruneEven(TreeNodeWithValue<int>* node) {
  return !(node->value % 2);
}

// The tree used for testing:
// * + 1
//   + 2
//   + 3 + 4 + 5
//       + 7

TEST(TreeNodeIteratorPruneTest, Test) {
  TreeNodeWithValue<int> root;
  root.Add(new TreeNodeWithValue<int>(1), 0);
  root.Add(new TreeNodeWithValue<int>(2), 1);
  TreeNodeWithValue<int>* f3 = new TreeNodeWithValue<int>(3);
  root.Add(f3, 2);
  TreeNodeWithValue<int>* f4 = new TreeNodeWithValue<int>(4);
  f3->Add(f4, 0);
  f4->Add(new TreeNodeWithValue<int>(5), 0);
  f3->Add(new TreeNodeWithValue<int>(7), 1);

  TreeNodeIterator<TreeNodeWithValue<int> > oddIterator(&root, PruneOdd);
  ASSERT_TRUE(oddIterator.has_next());
  ASSERT_EQ(2, oddIterator.Next()->value);
  ASSERT_FALSE(oddIterator.has_next());

  TreeNodeIterator<TreeNodeWithValue<int> > evenIterator(&root, PruneEven);
  ASSERT_TRUE(evenIterator.has_next());
  ASSERT_EQ(1, evenIterator.Next()->value);
  ASSERT_TRUE(evenIterator.has_next());
  ASSERT_EQ(3, evenIterator.Next()->value);
  ASSERT_TRUE(evenIterator.has_next());
  ASSERT_EQ(7, evenIterator.Next()->value);
  ASSERT_FALSE(evenIterator.has_next());
}

}  // namespace ui
