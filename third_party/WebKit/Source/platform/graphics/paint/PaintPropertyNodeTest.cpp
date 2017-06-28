// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by node BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintPropertyNode.h"

#include "platform/geometry/FloatRoundedRect.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PaintPropertyNodeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root = ClipPaintPropertyNode::Root();
    node = ClipPaintPropertyNode::Create(root, nullptr, FloatRoundedRect());
    child1 = ClipPaintPropertyNode::Create(node, nullptr, FloatRoundedRect());
    child2 = ClipPaintPropertyNode::Create(node, nullptr, FloatRoundedRect());
    grandchild1 =
        ClipPaintPropertyNode::Create(child1, nullptr, FloatRoundedRect());
    grandchild2 =
        ClipPaintPropertyNode::Create(child2, nullptr, FloatRoundedRect());

    //          root
    //           |
    //          node
    //         /   \
    //     child1   child2
    //       |        |
    // grandchild1 grandchild2
  }

  void ResetAllChanged() {
    grandchild1->ClearChangedToRoot();
    grandchild2->ClearChangedToRoot();
  }

  void ExpectInitialState() {
    EXPECT_FALSE(root->Changed(*root));
    EXPECT_FALSE(node->Changed(*root));
    EXPECT_FALSE(child1->Changed(*root));
    EXPECT_FALSE(child2->Changed(*root));
    EXPECT_FALSE(grandchild1->Changed(*root));
    EXPECT_FALSE(grandchild2->Changed(*root));
  }

  RefPtr<ClipPaintPropertyNode> root;
  RefPtr<ClipPaintPropertyNode> node;
  RefPtr<ClipPaintPropertyNode> child1;
  RefPtr<ClipPaintPropertyNode> child2;
  RefPtr<ClipPaintPropertyNode> grandchild1;
  RefPtr<ClipPaintPropertyNode> grandchild2;
};

TEST_F(PaintPropertyNodeTest, LowestCommonAncestor) {
  EXPECT_EQ(node, &LowestCommonAncestor(*node, *node));
  EXPECT_EQ(root, &LowestCommonAncestor(*root, *root));

  EXPECT_EQ(node, &LowestCommonAncestor(*grandchild1, *grandchild2));
  EXPECT_EQ(node, &LowestCommonAncestor(*grandchild1, *child2));
  EXPECT_EQ(root, &LowestCommonAncestor(*grandchild1, *root));
  EXPECT_EQ(child1, &LowestCommonAncestor(*grandchild1, *child1));

  EXPECT_EQ(node, &LowestCommonAncestor(*grandchild2, *grandchild1));
  EXPECT_EQ(node, &LowestCommonAncestor(*grandchild2, *child1));
  EXPECT_EQ(root, &LowestCommonAncestor(*grandchild2, *root));
  EXPECT_EQ(child2, &LowestCommonAncestor(*grandchild2, *child2));

  EXPECT_EQ(node, &LowestCommonAncestor(*child1, *child2));
  EXPECT_EQ(node, &LowestCommonAncestor(*child2, *child1));
}

TEST_F(PaintPropertyNodeTest, InitialStateAndReset) {
  ExpectInitialState();
  ResetAllChanged();
  ExpectInitialState();
}

TEST_F(PaintPropertyNodeTest, ChangeNode) {
  node->Update(root, nullptr, FloatRoundedRect(1, 2, 3, 4));
  EXPECT_TRUE(node->Changed(*root));
  EXPECT_FALSE(node->Changed(*node));
  EXPECT_TRUE(child1->Changed(*root));
  EXPECT_FALSE(child1->Changed(*node));
  EXPECT_TRUE(grandchild1->Changed(*root));
  EXPECT_FALSE(grandchild1->Changed(*node));

  EXPECT_FALSE(grandchild1->Changed(*child2));
  EXPECT_FALSE(grandchild1->Changed(*grandchild2));

  ResetAllChanged();
  ExpectInitialState();
}

TEST_F(PaintPropertyNodeTest, ChangeOneChild) {
  child1->Update(node, nullptr, FloatRoundedRect(1, 2, 3, 4));
  EXPECT_FALSE(node->Changed(*root));
  EXPECT_FALSE(node->Changed(*node));
  EXPECT_TRUE(child1->Changed(*root));
  EXPECT_TRUE(child1->Changed(*node));
  EXPECT_TRUE(grandchild1->Changed(*node));
  EXPECT_FALSE(grandchild1->Changed(*child1));
  EXPECT_FALSE(child2->Changed(*node));
  EXPECT_FALSE(grandchild2->Changed(*node));

  EXPECT_TRUE(child2->Changed(*child1));
  EXPECT_TRUE(child1->Changed(*child2));
  EXPECT_TRUE(child2->Changed(*grandchild1));
  EXPECT_TRUE(child1->Changed(*grandchild2));
  EXPECT_TRUE(grandchild1->Changed(*child2));
  EXPECT_TRUE(grandchild1->Changed(*grandchild2));
  EXPECT_TRUE(grandchild2->Changed(*child1));
  EXPECT_TRUE(grandchild2->Changed(*grandchild1));

  ResetAllChanged();
  ExpectInitialState();
}

TEST_F(PaintPropertyNodeTest, Reparent) {
  child1->Update(child2, nullptr, FloatRoundedRect(1, 2, 3, 4));
  EXPECT_FALSE(node->Changed(*root));
  EXPECT_TRUE(child1->Changed(*node));
  EXPECT_TRUE(child1->Changed(*child2));
  EXPECT_FALSE(child2->Changed(*node));
  EXPECT_TRUE(grandchild1->Changed(*node));
  EXPECT_FALSE(grandchild1->Changed(*child1));
  EXPECT_TRUE(grandchild1->Changed(*child2));

  ResetAllChanged();
  ExpectInitialState();
}

}  // namespace blink
