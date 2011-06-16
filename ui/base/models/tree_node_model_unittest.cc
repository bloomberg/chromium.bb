// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
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

  void AssertObserverCount(int added_count,
                           int removed_count,
                           int changed_count) {
    ASSERT_EQ(added_count, added_count_);
    ASSERT_EQ(removed_count, removed_count_);
    ASSERT_EQ(changed_count, changed_count_);
  }

  void ClearCounts() {
    added_count_ = removed_count_ = changed_count_ = 0;
  }

  // Begin TreeModelObserver implementation.
  virtual void TreeNodesAdded(TreeModel* model,
                              TreeModelNode* parent,
                              int start,
                              int count) OVERRIDE {
    added_count_++;
  }
  virtual void TreeNodesRemoved(TreeModel* model,
                                TreeModelNode* parent,
                                int start,
                                int count) OVERRIDE {
    removed_count_++;
  }
  virtual void TreeNodeChanged(TreeModel* model, TreeModelNode* node) OVERRIDE {
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
  TreeNodeWithValue<int>* root = new TreeNodeWithValue<int>();
  TreeNodeModel<TreeNodeWithValue<int> > model(root);
  model.AddObserver(this);
  ClearCounts();

  TreeNodeWithValue<int>* child1 = new TreeNodeWithValue<int>();
  model.Add(root, child1, 0);

  AssertObserverCount(1, 0, 0);

  for (int i = 0; i < 2; ++i)
    child1->Add(new TreeNodeWithValue<int>(), i);

  TreeNodeWithValue<int>* child2 = new TreeNodeWithValue<int>();
  model.Add(root, child2, 1);

  AssertObserverCount(2, 0, 0);

  ASSERT_EQ(2, root->child_count());
  ASSERT_EQ(2, child1->child_count());
  ASSERT_EQ(0, child2->child_count());
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
  root->Add(child1, 0);

  ASSERT_EQ(1, model.GetChildCount(root));

  // Now remove the |child1| from root and release the memory.
  delete model.Remove(root, child1);

  AssertObserverCount(0, 1, 0);

  ASSERT_EQ(0, model.GetChildCount(root));
}

// Verify if the nodes added under the root are all deleted when calling
// RemoveAll. Note that is responsability of the caller to free the memory
// of the nodes removed after RemoveAll is called.
// The tree looks like this:
// root
// |-- child1
// |   |-- foo
// |       |-- bar0
// |       |-- bar1
// |       |-- bar2
// |-- child2
// +-- child3
TEST_F(TreeNodeModelTest, RemoveAllNodes) {
  TreeNodeWithValue<int> root;

  TreeNodeWithValue<int> child1;
  TreeNodeWithValue<int> child2;
  TreeNodeWithValue<int> child3;

  root.Add(&child1, 0);
  root.Add(&child2, 1);
  root.Add(&child3, 2);

  TreeNodeWithValue<int>* foo = new TreeNodeWithValue<int>(2);
  child1.Add(foo, 0);

  // Add some nodes to |foo|.
  for (int i = 0; i < 3; ++i)
    foo->Add(new TreeNodeWithValue<int>(i), i);

  ASSERT_EQ(3, root.child_count());
  ASSERT_EQ(1, child1.child_count());
  ASSERT_EQ(3, foo->child_count());

  // Now remove the child nodes from root.
  root.RemoveAll();

  ASSERT_EQ(0, root.child_count());
  ASSERT_TRUE(root.empty());

  ASSERT_EQ(1, child1.child_count());
  ASSERT_EQ(3, foo->child_count());
}

// Verify if the model returns correct indexes for the specified nodes.
// The tree looks like this:
// root
// |-- child1
// |   |-- foo1
// +-- child2
TEST_F(TreeNodeModelTest, GetIndexOf) {
  TreeNodeWithValue<int> root;

  TreeNodeWithValue<int>* child1 = new TreeNodeWithValue<int>(1);
  root.Add(child1, 0);

  TreeNodeWithValue<int>* child2 = new TreeNodeWithValue<int>(2);
  root.Add(child2, 1);

  TreeNodeWithValue<int>* foo1 = new TreeNodeWithValue<int>(0);
  child1->Add(foo1, 0);

  ASSERT_EQ(-1, root.GetIndexOf(&root));
  ASSERT_EQ(0, root.GetIndexOf(child1));
  ASSERT_EQ(1, root.GetIndexOf(child2));
  ASSERT_EQ(-1, root.GetIndexOf(foo1));

  ASSERT_EQ(-1, child1->GetIndexOf(&root));
  ASSERT_EQ(-1, child1->GetIndexOf(child1));
  ASSERT_EQ(-1, child1->GetIndexOf(child2));
  ASSERT_EQ(0, child1->GetIndexOf(foo1));

  ASSERT_EQ(-1, child2->GetIndexOf(&root));
  ASSERT_EQ(-1, child2->GetIndexOf(child2));
  ASSERT_EQ(-1, child2->GetIndexOf(child1));
  ASSERT_EQ(-1, child2->GetIndexOf(foo1));
}

// Verify whether a specified node has or not an ancestor.
// The tree looks like this:
// root
// |-- child1
// |   |-- foo1
// +-- child2
TEST_F(TreeNodeModelTest, HasAncestor) {
  TreeNodeWithValue<int> root;
  TreeNodeWithValue<int>* child1 = new TreeNodeWithValue<int>(0);
  TreeNodeWithValue<int>* child2 = new TreeNodeWithValue<int>(0);

  root.Add(child1, 0);
  root.Add(child2, 1);

  TreeNodeWithValue<int>* foo1 = new TreeNodeWithValue<int>(0);
  child1->Add(foo1, 0);

  ASSERT_TRUE(root.HasAncestor(&root));
  ASSERT_FALSE(root.HasAncestor(child1));
  ASSERT_FALSE(root.HasAncestor(child2));
  ASSERT_FALSE(root.HasAncestor(foo1));

  ASSERT_TRUE(child1->HasAncestor(child1));
  ASSERT_TRUE(child1->HasAncestor(&root));
  ASSERT_FALSE(child1->HasAncestor(child2));
  ASSERT_FALSE(child1->HasAncestor(foo1));

  ASSERT_TRUE(child2->HasAncestor(child2));
  ASSERT_TRUE(child2->HasAncestor(&root));
  ASSERT_FALSE(child2->HasAncestor(child1));
  ASSERT_FALSE(child2->HasAncestor(foo1));

  ASSERT_TRUE(foo1->HasAncestor(foo1));
  ASSERT_TRUE(foo1->HasAncestor(child1));
  ASSERT_TRUE(foo1->HasAncestor(&root));
  ASSERT_FALSE(foo1->HasAncestor(child2));
}

// Verify if GetTotalNodeCount returns the correct number of nodes from the node
// specifed. The count should include the node itself.
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
// The TotalNodeCount of root is:            9
// The TotalNodeCount of child1 is:          3
// The TotalNodeCount of child2 and foo2 is: 2
// The TotalNodeCount of bar1 is:            1
// And so on...
TEST_F(TreeNodeModelTest, GetTotalNodeCount) {
  TreeNodeWithValue<int> root;

  TreeNodeWithValue<int>* child1 = new TreeNodeWithValue<int>();
  TreeNodeWithValue<int>* child2 = new TreeNodeWithValue<int>();
  TreeNodeWithValue<int>* child3 = new TreeNodeWithValue<int>();

  root.Add(child1, 0);
  child1->Add(child2, 0);
  child2->Add(child3, 0);

  TreeNodeWithValue<int>* foo1 = new TreeNodeWithValue<int>();
  TreeNodeWithValue<int>* foo2 = new TreeNodeWithValue<int>();
  TreeNodeWithValue<int>* foo3 = new TreeNodeWithValue<int>();
  TreeNodeWithValue<int>* foo4 = new TreeNodeWithValue<int>();

  root.Add(foo1, 1);
  foo1->Add(foo2, 0);
  foo2->Add(foo3, 0);
  foo1->Add(foo4, 1);

  TreeNodeWithValue<int>* bar1 = new TreeNodeWithValue<int>();

  root.Add(bar1, 2);

  ASSERT_EQ(9, root.GetTotalNodeCount());
  ASSERT_EQ(3, child1->GetTotalNodeCount());
  ASSERT_EQ(2, child2->GetTotalNodeCount());
  ASSERT_EQ(2, foo2->GetTotalNodeCount());
  ASSERT_EQ(1, bar1->GetTotalNodeCount());
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

TEST_F(TreeNodeModelTest, BasicOperations) {
  TreeNodeWithValue<int>* root = new TreeNodeWithValue<int>(0);
  EXPECT_EQ(0, root->child_count());

  TreeNodeWithValue<int>* child1 = new TreeNodeWithValue<int>(1);
  root->Add(child1, root->child_count());
  EXPECT_EQ(1, root->child_count());
  EXPECT_EQ(root, child1->parent());

  TreeNodeWithValue<int>* child2 = new TreeNodeWithValue<int>(1);
  root->Add(child2, root->child_count());
  EXPECT_EQ(2, root->child_count());
  EXPECT_EQ(child1->parent(), child2->parent());

  root->Remove(child2);
  EXPECT_EQ(1, root->child_count());
  EXPECT_EQ(NULL, child2->parent());
  delete child2;

  delete root->Remove(child1);
  EXPECT_EQ(0, root->child_count());

  delete root;
}

TEST_F(TreeNodeModelTest, IsRoot) {
  TreeNodeWithValue<int> root;
  EXPECT_TRUE(root.is_root());

  TreeNodeWithValue<int>* child1 = new TreeNodeWithValue<int>(1);
  root.Add(child1, root.child_count());
  EXPECT_FALSE(child1->is_root());
}

}  // namespace ui
