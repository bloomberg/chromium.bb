// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/tree_node_model.h"

namespace ui {

class TreeNodeModelTest : public testing::Test, public TreeModelObserver {
 public:
  TreeNodeModelTest()
      : added_count_(0),
        removed_count_(0),
        changed_count_(0) {}

  void AssertObserverCount(int added_count, int removed_count,
                           int changed_count) {
    ASSERT_EQ(added_count, added_count_);
    ASSERT_EQ(removed_count, removed_count_);
    ASSERT_EQ(changed_count, changed_count_);
  }

  void ClearCounts() {
    added_count_ = removed_count_ = changed_count_ = 0;
  }

  // Begin TreeModelObserver implementation.
  virtual void TreeNodesAdded(TreeModel* model, TreeModelNode* parent,
                              int start, int count) {
    added_count_++;
  }
  virtual void TreeNodesRemoved(TreeModel* model, TreeModelNode* parent,
                                int start, int count) {
    removed_count_++;
  }
  virtual void TreeNodeChanged(TreeModel* model, TreeModelNode* node) {
    changed_count_++;
  }
  // End TreeModelObserver implementation.

 private:
  int added_count_;
  int removed_count_;
  int changed_count_;

  DISALLOW_COPY_AND_ASSIGN(TreeNodeModelTest);
};

// Verify if the model is properly adding a new node in the tree and
// notifying the observers.
// The tree looks like this:
// root
// |-- child1
// |   |-- foo1
// |   |-- foo2
// +-- child2
TEST_F(TreeNodeModelTest, AddNode) {
  TreeNodeWithValue<int>* root =
      new TreeNodeWithValue<int>(ASCIIToUTF16("root"), 0);
  TreeNodeModel<TreeNodeWithValue<int> > model(root);
  model.AddObserver(this);
  ClearCounts();

  // Create the first root child.
  TreeNodeWithValue<int>* child1 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("child 1"), 1);
  model.Add(root, 0, child1);

  AssertObserverCount(1, 0, 0);

  // Add two nodes under the |child1|.
  TreeNodeWithValue<int>* foo1 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("foo1"), 3);
  TreeNodeWithValue<int>* foo2 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("foo2"), 4);
  child1->Add(0, foo1);
  child1->Add(1, foo2);

  // Create the second root child.
  TreeNodeWithValue<int>* child2 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("child 2"), 2);
  root->Add(1, child2);

  // Check if there is two nodes under the root.
  ASSERT_EQ(2, model.GetChildCount(root));

  // Check if there is two nodes under |child1|.
  ASSERT_EQ(2, model.GetChildCount(child1));

  // Check if there is none nodes under |child2|.
  ASSERT_EQ(0, model.GetChildCount(child2));
}

// Verify if the model is properly removing a node from the tree
// and notifying the observers.
TEST_F(TreeNodeModelTest, RemoveNode) {
  TreeNodeWithValue<int>* root =
      new TreeNodeWithValue<int>(ASCIIToUTF16("root"), 0);
  TreeNodeModel<TreeNodeWithValue<int> > model(root);
  model.AddObserver(this);
  ClearCounts();

  // Create the first child node.
  TreeNodeWithValue<int>* child1 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("child 1"), 1);

  // And add it to the root node.
  root->Add(0, child1);

  ASSERT_EQ(1, model.GetChildCount(root));

  // Now remove the |child1| from root and release the memory.
  delete model.Remove(root, 0);

  AssertObserverCount(0, 1, 0);

  ASSERT_EQ(0, model.GetChildCount(root));
}

// Verify if the nodes added under the root are all deleted when calling
// RemoveAll. Note that is responsability of the caller to free the memory
// of the nodes removed after RemoveAll is called.
// The tree looks like this:
// root
// |-- child1
// |   |-- foo1
// |       |-- child0
// |       |-- child1
// +-------|-- child2
TEST_F(TreeNodeModelTest, RemoveAllNodes) {
  TreeNodeWithValue<int>* root =
      new TreeNodeWithValue<int>(ASCIIToUTF16("root"), 0);
  TreeNodeModel<TreeNodeWithValue<int> > model(root);
  model.AddObserver(this);
  ClearCounts();

  // Create the first child node.
  TreeNodeWithValue<int>* child1 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("child 1"), 1);
  model.Add(root, 0, child1);

  TreeNodeWithValue<int>* foo1 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("foo1"), 2);
  model.Add(child1, 0, foo1);

  // Add some nodes to |foo1|.
  for (int i = 0; i < 3; ++i) {
    model.Add(foo1, i,
              new TreeNodeWithValue<int>(ASCIIToUTF16("child") +
                                             base::IntToString16(i), i));
  }

  ASSERT_EQ(3, model.GetChildCount(foo1));

  // Now remove all nodes from root.
  root->RemoveAll();

  // Release memory, so we don't leak.
  delete child1;

  ASSERT_EQ(0, model.GetChildCount(root));
}

// Verify if the model returns correct indexes for the specified nodes.
// The tree looks like this:
// root
// |-- child1
// |   |-- foo1
// +-- child2
TEST_F(TreeNodeModelTest, GetIndexOf) {
  TreeNodeWithValue<int>* root =
      new TreeNodeWithValue<int>(ASCIIToUTF16("root"), 0);
  TreeNodeModel<TreeNodeWithValue<int> > model(root);
  model.AddObserver(this);
  ClearCounts();

  TreeNodeWithValue<int>* child1 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("child 1"), 1);
  model.Add(root, 0, child1);

  TreeNodeWithValue<int>* child2 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("child 2"), 2);
  model.Add(root, 1, child2);

  TreeNodeWithValue<int>* foo1 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("foo1"), 0);
  model.Add(child1, 0, foo1);

  ASSERT_EQ(0, model.GetIndexOf(root, child1));
  ASSERT_EQ(1, model.GetIndexOf(root, child2));
  ASSERT_EQ(0, model.GetIndexOf(child1, foo1));
  ASSERT_EQ(-1, model.GetIndexOf(root, foo1));
}

// Verify whether a specified node has or not an ancestor.
// The tree looks like this:
// root
// |-- child1
// |-- child2
TEST_F(TreeNodeModelTest, HasAncestor) {
  TreeNodeWithValue<int>* root =
      new TreeNodeWithValue<int>(ASCIIToUTF16("root"), 0);
  TreeNodeModel<TreeNodeWithValue<int> > model(root);

  TreeNodeWithValue<int>* child1 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("child 1"), 0);
  model.Add(root, 0, child1);

  TreeNodeWithValue<int>* child2 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("child 2"), 1);
  model.Add(root, 1, child2);

  ASSERT_TRUE(root->HasAncestor(root));
  ASSERT_FALSE(root->HasAncestor(child1));
  ASSERT_TRUE(child1->HasAncestor(root));
  ASSERT_FALSE(child1->HasAncestor(child2));
  ASSERT_FALSE(child2->HasAncestor(child1));
}

// The tree looks like this:
// root
// |-- child1
// |   |-- child2
// |       |-- child3
// |-- foo1
// |   |-- foo2
// |       |-- foo3
// |   |-- foo4
// +-- bar1
//
// The TotalNodeCount of root is:   9
// The TotalNodeCount of child1 is: 3
// The TotalNodeCount of bar1 is:   1
// And so on...
// The purpose here is to verify if the function returns the total of nodes
// under the specifed node correctly. The count should include the node it self.
TEST_F(TreeNodeModelTest, GetTotalNodeCount) {
  TreeNodeWithValue<int>* root =
      new TreeNodeWithValue<int>(ASCIIToUTF16("root"), 0);
  TreeNodeModel<TreeNodeWithValue<int> > model(root);

  TreeNodeWithValue<int>* child1 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("child1"), 1);
  TreeNodeWithValue<int>* child2 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("child2"), 2);
  TreeNodeWithValue<int>* child3 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("child3"), 3);
  TreeNodeWithValue<int>* foo1 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("foo1"), 4);
  TreeNodeWithValue<int>* foo2 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("foo2"), 5);
  TreeNodeWithValue<int>* foo3 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("foo3"), 6);
  TreeNodeWithValue<int>* foo4 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("foo4"), 7);
  TreeNodeWithValue<int>* bar1 =
      new TreeNodeWithValue<int>(ASCIIToUTF16("bar1"), 8);

  model.Add(root, 0, child1);
  model.Add(child1, 0, child2);
  model.Add(child2, 0, child3);

  model.Add(root, 1, foo1);
  model.Add(foo1, 0, foo2);
  model.Add(foo1, 1, foo4);
  model.Add(foo2, 0, foo3);

  model.Add(root, 0, bar1);

  ASSERT_EQ(9, root->GetTotalNodeCount());
  ASSERT_EQ(3, child1->GetTotalNodeCount());
  ASSERT_EQ(1, bar1->GetTotalNodeCount());
  ASSERT_EQ(2, foo2->GetTotalNodeCount());
}

// Makes sure that we are notified when the node is renamed,
// also makes sure the node is properly renamed.
TEST_F(TreeNodeModelTest, SetTitle) {
  TreeNodeWithValue<int>* root =
      new TreeNodeWithValue<int>(ASCIIToUTF16("root"), 0);
  TreeNodeModel<TreeNodeWithValue<int> > model(root);
  model.AddObserver(this);
  ClearCounts();

  const string16 title(ASCIIToUTF16("root2"));
  model.SetTitle(root, title);
  AssertObserverCount(0, 0, 1);
  EXPECT_EQ(title, root->GetTitle());
}

}  // namespace ui
