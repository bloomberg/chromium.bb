// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tree/tree_view.h"

#include <string>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "ui/base/models/tree_node_model.h"
#include "ui/views/test/views_test_base.h"

using ui::TreeModel;
using ui::TreeModelNode;
using ui::TreeNode;

namespace views {

class TestNode : public TreeNode<TestNode> {
 public:
  TestNode() {}
  virtual ~TestNode() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNode);
};

// Creates the following structure:
// 'root'
//   'a'
//   'b'
//     'b1'
//   'c'
class TreeViewViewsTest : public ViewsTestBase {
 public:
  TreeViewViewsTest() : model_(new TestNode) {
    static_cast<TestNode*>(model_.GetRoot())->SetTitle(ASCIIToUTF16("root"));
    Add(model_.GetRoot(), 0, "a");
    Add(Add(model_.GetRoot(), 1, "b"), 0, "b1");
    Add(model_.GetRoot(), 2, "c");
  }

 protected:
  TestNode* Add(TestNode* parent,
                int index,
                const std::string& title);

  std::string TreeViewContentsAsString();

  std::string GetSelectedNodeTitle();

  std::string GetEditingNodeTitle();

  TestNode* GetNodeByTitle(const std::string& title);

  void IncrementSelection(bool next);
  void CollapseOrSelectParent();
  void ExpandOrSelectChild();
  int GetRowCount();

  ui::TreeNodeModel<TestNode > model_;
  TreeView tree_;

 private:
  std::string InternalNodeAsString(TreeView::InternalNode* node);

  TestNode* GetNodeByTitleImpl(TestNode* node, const string16& title);

  DISALLOW_COPY_AND_ASSIGN(TreeViewViewsTest);
};

TestNode* TreeViewViewsTest::Add(TestNode* parent,
                                 int index,
                                 const std::string& title) {
  TestNode* new_node = new TestNode;
  new_node->SetTitle(ASCIIToUTF16(title));
  model_.Add(parent, new_node, index);
  return new_node;
}

std::string TreeViewViewsTest::TreeViewContentsAsString() {
  return InternalNodeAsString(&tree_.root_);
}

std::string TreeViewViewsTest::GetSelectedNodeTitle() {
  TreeModelNode* model_node = tree_.GetSelectedNode();
  return model_node ? UTF16ToASCII(model_node->GetTitle()) : std::string();
}

std::string TreeViewViewsTest::GetEditingNodeTitle() {
  TreeModelNode* model_node = tree_.GetEditingNode();
  return model_node ? UTF16ToASCII(model_node->GetTitle()) : std::string();
}

TestNode* TreeViewViewsTest::GetNodeByTitle(const std::string& title) {
  return GetNodeByTitleImpl(model_.GetRoot(), ASCIIToUTF16(title));
}

void TreeViewViewsTest::IncrementSelection(bool next) {
  tree_.IncrementSelection(next ? TreeView::INCREMENT_NEXT :
                           TreeView::INCREMENT_PREVIOUS);
}

void TreeViewViewsTest::CollapseOrSelectParent() {
  tree_.CollapseOrSelectParent();
}

void TreeViewViewsTest::ExpandOrSelectChild() {
  tree_.ExpandOrSelectChild();
}

int TreeViewViewsTest::GetRowCount() {
  return tree_.GetRowCount();
}

TestNode* TreeViewViewsTest::GetNodeByTitleImpl(TestNode* node,
                                                const string16& title) {
  if (node->GetTitle() == title)
    return node;
  for (int i = 0; i < node->child_count(); ++i) {
    TestNode* child = GetNodeByTitleImpl(node->GetChild(i), title);
    if (child)
      return child;
  }
  return NULL;
}

std::string TreeViewViewsTest::InternalNodeAsString(
    TreeView::InternalNode* node) {
  std::string result = UTF16ToASCII(node->model_node()->GetTitle());
  if (node->is_expanded() && node->child_count()) {
    result += " [";
    for (int i = 0; i < node->child_count(); ++i) {
      if (i > 0)
        result += " ";
      result += InternalNodeAsString(node->GetChild(i));
    }
    result += "]";
  }
  return result;
}

// Verifies setting model correctly updates internal state.
TEST_F(TreeViewViewsTest, SetModel) {
  tree_.SetModel(&model_);
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(4, GetRowCount());
}

// Verifies SetSelectedNode works.
TEST_F(TreeViewViewsTest, SetSelectedNode) {
  tree_.SetModel(&model_);
  EXPECT_EQ("root", GetSelectedNodeTitle());

  // NULL should clear the selection.
  tree_.SetSelectedNode(NULL);
  EXPECT_EQ(std::string(), GetSelectedNodeTitle());

  // Select 'c'.
  tree_.SetSelectedNode(GetNodeByTitle("c"));
  EXPECT_EQ("c", GetSelectedNodeTitle());

  // Select 'b1', which should expand 'b'.
  tree_.SetSelectedNode(GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("b1", GetSelectedNodeTitle());
}

// Makes sure SetRootShown doesn't blow up.
TEST_F(TreeViewViewsTest, HideRoot) {
  tree_.SetModel(&model_);
  tree_.SetRootShown(false);
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ(3, GetRowCount());
}

// Expands a node and verifies the children are loaded correctly.
TEST_F(TreeViewViewsTest, Expand) {
  tree_.SetModel(&model_);
  tree_.Expand(GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("root",GetSelectedNodeTitle());
  EXPECT_EQ(5, GetRowCount());
}

// Collapes a node and verifies state.
TEST_F(TreeViewViewsTest, Collapse) {
  tree_.SetModel(&model_);
  tree_.Expand(GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ(5, GetRowCount());
  tree_.SetSelectedNode(GetNodeByTitle("b1"));
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  tree_.Collapse(GetNodeByTitle("b"));
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  // Selected node should have moved to 'b'
  EXPECT_EQ("b", GetSelectedNodeTitle());
  EXPECT_EQ(4, GetRowCount());
}

// Verifies adding nodes works.
TEST_F(TreeViewViewsTest, TreeNodesAdded) {
  tree_.SetModel(&model_);
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  // Add a node between b and c.
  Add(model_.GetRoot(), 2, "B");
  EXPECT_EQ("root [a b B c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(5, GetRowCount());

  // Add a child of b1, which hasn't been loaded and shouldn't do anything.
  Add(GetNodeByTitle("b1"), 0, "b11");
  EXPECT_EQ("root [a b B c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(5, GetRowCount());

  // Add a child of b, which isn't expanded yet, so it shouldn't effect
  // anything.
  Add(GetNodeByTitle("b"), 1, "b2");
  EXPECT_EQ("root [a b B c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(5, GetRowCount());

  // Expand b and make sure b2 is there.
  tree_.Expand(GetNodeByTitle("b"));
  EXPECT_EQ("root [a b [b1 b2] B c]", TreeViewContentsAsString());
  EXPECT_EQ("root",GetSelectedNodeTitle());
  EXPECT_EQ(7, GetRowCount());
}

// Verifies removing nodes works.
TEST_F(TreeViewViewsTest, TreeNodesRemoved) {
  // Add c1 as a child of c and c11 as a child of c1.
  Add(Add(GetNodeByTitle("c"), 0, "c1"), 0, "c11");
  tree_.SetModel(&model_);

  // Remove c11, which shouldn't have any effect on the tree.
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(4, GetRowCount());

  // Expand b1, then collapse it and remove its only child, b1. This shouldn't
  // effect the tree.
  tree_.Expand(GetNodeByTitle("b"));
  tree_.Collapse(GetNodeByTitle("b"));
  delete model_.Remove(GetNodeByTitle("b1")->parent(), GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(4, GetRowCount());

  // Remove 'b'.
  delete model_.Remove(GetNodeByTitle("b")->parent(), GetNodeByTitle("b"));
  EXPECT_EQ("root [a c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(3, GetRowCount());

  // Remove 'c11', shouldn't visually change anything.
  delete model_.Remove(GetNodeByTitle("c11")->parent(), GetNodeByTitle("c11"));
  EXPECT_EQ("root [a c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(3, GetRowCount());

  // Select 'c1', remove 'c' and make sure selection changes.
  tree_.SetSelectedNode(GetNodeByTitle("c1"));
  EXPECT_EQ("c1", GetSelectedNodeTitle());
  delete model_.Remove(GetNodeByTitle("c")->parent(), GetNodeByTitle("c"));
  EXPECT_EQ("root [a]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(2, GetRowCount());

  tree_.SetRootShown(false);
  // Add 'b' select it and remove it. Because we're not showing the root
  // selection should change to 'a'.
  Add(GetNodeByTitle("root"), 1, "b");
  tree_.SetSelectedNode(GetNodeByTitle("b"));
  delete model_.Remove(GetNodeByTitle("b")->parent(), GetNodeByTitle("b"));
  EXPECT_EQ("root [a]", TreeViewContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ(1, GetRowCount());
}

// Verifies changing a node title works.
TEST_F(TreeViewViewsTest, TreeNodeChanged) {
  // Add c1 as a child of c and c11 as a child of c1.
  Add(Add(GetNodeByTitle("c"), 0, "c1"), 0, "c11");
  tree_.SetModel(&model_);

  // Change c11, shouldn't do anything.
  model_.SetTitle(GetNodeByTitle("c11"), ASCIIToUTF16("c11.new"));
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(4, GetRowCount());

  // Change 'b1', shouldn't do anything.
  model_.SetTitle(GetNodeByTitle("b1"), ASCIIToUTF16("b1.new"));
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(4, GetRowCount());

  // Change 'b'.
  model_.SetTitle(GetNodeByTitle("b"), ASCIIToUTF16("b.new"));
  EXPECT_EQ("root [a b.new c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(4, GetRowCount());
}

// Verifies IncrementSelection() works.
TEST_F(TreeViewViewsTest, IncrementSelection) {
  tree_.SetModel(&model_);

  IncrementSelection(true);
  EXPECT_EQ("a", GetSelectedNodeTitle());
  IncrementSelection(true);
  EXPECT_EQ("b", GetSelectedNodeTitle());
  IncrementSelection(true);
  tree_.Expand(GetNodeByTitle("b"));
  IncrementSelection(false);
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  IncrementSelection(true);
  EXPECT_EQ("c", GetSelectedNodeTitle());
  IncrementSelection(true);
  EXPECT_EQ("c", GetSelectedNodeTitle());

  tree_.SetRootShown(false);
  tree_.SetSelectedNode(GetNodeByTitle("a"));
  EXPECT_EQ("a", GetSelectedNodeTitle());
  IncrementSelection(false);
  EXPECT_EQ("a", GetSelectedNodeTitle());
}

// Verifies CollapseOrSelectParent works.
TEST_F(TreeViewViewsTest, CollapseOrSelectParent) {
  tree_.SetModel(&model_);

  tree_.SetSelectedNode(GetNodeByTitle("root"));
  CollapseOrSelectParent();
  EXPECT_EQ("root", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());

  // Hide the root, which should implicitly expand the root.
  tree_.SetRootShown(false);
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());

  tree_.SetSelectedNode(GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  CollapseOrSelectParent();
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("b", GetSelectedNodeTitle());
  CollapseOrSelectParent();
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("b", GetSelectedNodeTitle());
}

// Verifies ExpandOrSelectChild works.
TEST_F(TreeViewViewsTest, ExpandOrSelectChild) {
  tree_.SetModel(&model_);

  tree_.SetSelectedNode(GetNodeByTitle("root"));
  ExpandOrSelectChild();
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());

  ExpandOrSelectChild();
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());

  tree_.SetSelectedNode(GetNodeByTitle("b"));
  ExpandOrSelectChild();
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("b", GetSelectedNodeTitle());
  ExpandOrSelectChild();
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  ExpandOrSelectChild();
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("b1", GetSelectedNodeTitle());
}

}  // namespace views
