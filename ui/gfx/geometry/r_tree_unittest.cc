// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/r_tree.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {

class RTreeTest : public ::testing::Test {
 protected:
  // Given a pointer to an RTree, traverse it and verify its internal structure
  // is consistent with the RTree semantics.
  void ValidateRTree(RTree* rt) {
    // If RTree is empty it should have an empty rectangle.
    if (!rt->root_->count()) {
      EXPECT_TRUE(rt->root_->rect().IsEmpty());
      EXPECT_EQ(rt->root_->level(), 0);
      return;
    }
    // Root is allowed to have fewer than min_children_ but never more than
    // max_children_.
    EXPECT_LE(rt->root_->count(), rt->max_children_);
    // The root should never be a record node.
    EXPECT_GT(rt->root_->level(), -1);
    EXPECT_FALSE(rt->root_->key());
    // The root should never have a parent pointer.
    EXPECT_FALSE(rt->root_->parent());
    // Bounds must be consistent on the root.
    CheckBoundsConsistent(rt->root_.get());
    // We traverse root's children ourselves, so we can avoid asserts about
    // root's potential inconsistencies.
    for (size_t i = 0; i < rt->root_->children_.size(); ++i) {
      ValidateNode(
          rt->root_->children_[i], rt->min_children_, rt->max_children_);
    }
  }

  // Recursive descent method used by ValidateRTree to check each node within
  // the RTree for consistency with RTree semantics.
  void ValidateNode(RTree::Node* node,
                    size_t min_children,
                    size_t max_children) {
    // Record nodes have different requirements, handle up front.
    if (node->level() == -1) {
      // Record nodes may have no children.
      EXPECT_EQ(node->count(), 0U);
      // They must have an associated non-NULL key.
      EXPECT_TRUE(node->key());
      // They must always have a parent.
      EXPECT_TRUE(node->parent());
      return;
    }
    // Non-record node, normal expectations apply.
    EXPECT_GE(node->count(), min_children);
    EXPECT_LE(node->count(), max_children);
    EXPECT_EQ(node->key(), 0);
    CheckBoundsConsistent(node);
    for (size_t i = 0; i < node->children_.size(); ++i) {
      ValidateNode(node->children_[i], min_children, max_children);
    }
  }

  // Check bounds are consistent with children bounds, and other checks
  // convenient to do while enumerating the children of node.
  void CheckBoundsConsistent(RTree::Node* node) {
    EXPECT_FALSE(node->rect_.IsEmpty());
    Rect check_bounds;
    for (size_t i = 0; i < node->children_.size(); ++i) {
      RTree::Node* child_node = node->children_[i];
      check_bounds.Union(child_node->rect());
      EXPECT_EQ(node->level() - 1, child_node->level());
      EXPECT_EQ(node, child_node->parent());
    }
    EXPECT_EQ(node->rect_, check_bounds);
  }

  // Adds count squares stacked around the point (0,0) with key equal to width.
  void AddStackedSquares(RTree* rt, int count) {
    for (int i = 1; i <= count; ++i) {
      rt->Insert(Rect(0, 0, i, i), i);
      ValidateRTree(rt);
    }
  }

  // Given an unordered list of matching keys, verify that it contains all
  // values [1..length] for the length of that list.
  void VerifyAllKeys(const base::hash_set<intptr_t>& keys) {
    // Verify that the keys are in values [1,count].
    for (size_t i = 1; i <= keys.size(); ++i) {
      EXPECT_EQ(keys.count(i), 1U);
    }
  }

  // Given a node and a rectangle, builds an expanded rectangle list where the
  // ith element of the rectangle is union of the recangle of the ith child of
  // the node and the argument rectangle.
  void BuildExpandedRects(RTree::Node* node,
                          const Rect& rect,
                          std::vector<Rect>* expanded_rects) {
    expanded_rects->clear();
    expanded_rects->reserve(node->children_.size());
    for (size_t i = 0; i < node->children_.size(); ++i) {
      Rect expanded_rect(rect);
      expanded_rect.Union(node->children_[i]->rect_);
      expanded_rects->push_back(expanded_rect);
    }
  }
};

// An empty RTree should never return Query results, and RTrees should be empty
// upon construction.
TEST_F(RTreeTest, QueryEmptyTree) {
  RTree rt(2, 10);
  ValidateRTree(&rt);
  base::hash_set<intptr_t> results;
  Rect test_rect(25, 25);
  rt.Query(test_rect, &results);
  EXPECT_EQ(results.size(), 0U);
  ValidateRTree(&rt);
}

// Clear should empty the tree, meaning that all queries should not return
// results after.
TEST_F(RTreeTest, ClearEmptiesTreeOfSingleNode) {
  RTree rt(2, 5);
  rt.Insert(Rect(0, 0, 100, 100), 1);
  rt.Clear();
  base::hash_set<intptr_t> results;
  Rect test_rect(1, 1);
  rt.Query(test_rect, &results);
  EXPECT_EQ(results.size(), 0U);
  ValidateRTree(&rt);
}

// Even with a complex internal structure, clear should empty the tree, meaning
// that all queries should not return results after.
TEST_F(RTreeTest, ClearEmptiesTreeOfManyNodes) {
  RTree rt(2, 5);
  AddStackedSquares(&rt, 100);
  rt.Clear();
  base::hash_set<intptr_t> results;
  Rect test_rect(1, 1);
  rt.Query(test_rect, &results);
  EXPECT_EQ(results.size(), 0U);
  ValidateRTree(&rt);
}

// Duplicate inserts should overwrite previous inserts.
TEST_F(RTreeTest, DuplicateInsertsOverwrite) {
  RTree rt(2, 5);
  // Add 100 stacked squares, but always with duplicate key of 1.
  for (int i = 1; i <= 100; ++i) {
    rt.Insert(Rect(0, 0, i, i), 1);
    ValidateRTree(&rt);
  }
  base::hash_set<intptr_t> results;
  Rect test_rect(1, 1);
  rt.Query(test_rect, &results);
  EXPECT_EQ(results.size(), 1U);
  EXPECT_EQ(results.count(1), 1U);
}

// Call Remove() once on something that's been inserted repeatedly.
TEST_F(RTreeTest, DuplicateInsertRemove) {
  RTree rt(3, 9);
  AddStackedSquares(&rt, 25);
  for (int i = 1; i <= 100; ++i) {
    rt.Insert(Rect(0, 0, i, i), 26);
    ValidateRTree(&rt);
  }
  rt.Remove(26);
  base::hash_set<intptr_t> results;
  Rect test_rect(1, 1);
  rt.Query(test_rect, &results);
  EXPECT_EQ(results.size(), 25U);
  VerifyAllKeys(results);
}

// Call Remove() repeatedly on something that's been inserted once.
TEST_F(RTreeTest, InsertDuplicateRemove) {
  RTree rt(7, 15);
  AddStackedSquares(&rt, 101);
  for (int i = 0; i < 100; ++i) {
    rt.Remove(101);
    ValidateRTree(&rt);
  }
  base::hash_set<intptr_t> results;
  Rect test_rect(1, 1);
  rt.Query(test_rect, &results);
  EXPECT_EQ(results.size(), 100U);
  VerifyAllKeys(results);
}

// Stacked rects should meet all matching queries regardless of nesting.
TEST_F(RTreeTest, QueryStackedSquaresNestedHit) {
  RTree rt(2, 5);
  AddStackedSquares(&rt, 100);
  base::hash_set<intptr_t> results;
  Rect test_rect(1, 1);
  rt.Query(test_rect, &results);
  EXPECT_EQ(results.size(), 100U);
  VerifyAllKeys(results);
}

// Stacked rects should meet all matching queries when contained completely by
// the query rectangle.
TEST_F(RTreeTest, QueryStackedSquaresContainedHit) {
  RTree rt(2, 10);
  AddStackedSquares(&rt, 100);
  base::hash_set<intptr_t> results;
  Rect test_rect(0, 0, 100, 100);
  rt.Query(test_rect, &results);
  EXPECT_EQ(results.size(), 100U);
  VerifyAllKeys(results);
}

// Stacked rects should miss a missing query when the query has no intersection
// with the rects.
TEST_F(RTreeTest, QueryStackedSquaresCompleteMiss) {
  RTree rt(2, 7);
  AddStackedSquares(&rt, 100);
  base::hash_set<intptr_t> results;
  Rect test_rect(150, 150, 100, 100);
  rt.Query(test_rect, &results);
  EXPECT_EQ(results.size(), 0U);
}

// Removing half the nodes after insertion should still result in a valid tree.
TEST_F(RTreeTest, RemoveHalfStackedRects) {
  RTree rt(2, 11);
  AddStackedSquares(&rt, 200);
  for (int i = 101; i <= 200; ++i) {
    rt.Remove(i);
    ValidateRTree(&rt);
  }
  base::hash_set<intptr_t> results;
  Rect test_rect(1, 1);
  rt.Query(test_rect, &results);
  EXPECT_EQ(results.size(), 100U);
  VerifyAllKeys(results);
  // Add the nodes back in.
  for (int i = 101; i <= 200; ++i) {
    rt.Insert(Rect(0, 0, i, i), i);
    ValidateRTree(&rt);
  }
  results.clear();
  rt.Query(test_rect, &results);
  EXPECT_EQ(results.size(), 200U);
  VerifyAllKeys(results);
}

TEST_F(RTreeTest, InsertNegativeCoordsRect) {
  RTree rt(5, 11);
  for (int i = 1; i <= 100; ++i) {
    rt.Insert(Rect(-i, -i, i, i), (i * 2) - 1);
    ValidateRTree(&rt);
    rt.Insert(Rect(0, 0, i, i), i * 2);
    ValidateRTree(&rt);
  }
  base::hash_set<intptr_t> results;
  Rect test_rect(-1, -1, 2, 2);
  rt.Query(test_rect, &results);
  EXPECT_EQ(results.size(), 200U);
  VerifyAllKeys(results);
}

TEST_F(RTreeTest, RemoveNegativeCoordsRect) {
  RTree rt(7, 21);
  // Add 100 positive stacked squares.
  AddStackedSquares(&rt, 100);
  // Now add 100 negative stacked squares.
  for (int i = 101; i <= 200; ++i) {
    rt.Insert(Rect(100 - i, 100 - i, i - 100, i - 100), 301 - i);
    ValidateRTree(&rt);
  }
  // Now remove half of the negative squares.
  for (int i = 101; i <= 150; ++i) {
    rt.Remove(301 - i);
    ValidateRTree(&rt);
  }
  // Queries should return 100 positive and 50 negative stacked squares.
  base::hash_set<intptr_t> results;
  Rect test_rect(-1, -1, 2, 2);
  rt.Query(test_rect, &results);
  EXPECT_EQ(results.size(), 150U);
  VerifyAllKeys(results);
}

TEST_F(RTreeTest, InsertEmptyRectReplacementRemovesKey) {
  RTree rt(10, 31);
  AddStackedSquares(&rt, 50);
  ValidateRTree(&rt);

  // Replace last square with empty rect.
  rt.Insert(Rect(), 50);
  ValidateRTree(&rt);

  // Now query large area to get all rects in tree.
  base::hash_set<intptr_t> results;
  Rect test_rect(0, 0, 100, 100);
  rt.Query(test_rect, &results);

  // Should only be 49 rects in tree.
  EXPECT_EQ(results.size(), 49U);
  VerifyAllKeys(results);
}

TEST_F(RTreeTest, NodeRemoveNodesForReinsert) {
  // Make a leaf node for testing removal from.
  scoped_ptr<RTree::Node> test_node(new RTree::Node(0));
  // Build 20 record nodes with rectangle centers going from (1,1) to (20,20)
  for (int i = 1; i <= 20; ++i) {
    test_node->AddChild(new RTree::Node(Rect(i - 1, i - 1, 2, 2), i));
  }
  // Quick verification of the node before removing children.
  ValidateNode(test_node.get(), 1U, 20U);
  // Use a scoped vector to delete all children that get removed from the Node.
  ScopedVector<RTree::Node> removals;
  test_node->RemoveNodesForReinsert(1, &removals);
  // Should have gotten back 1 node pointers.
  EXPECT_EQ(removals.size(), 1U);
  // There should be 19 left in the test_node.
  EXPECT_EQ(test_node->count(), 19U);
  // If we fix up the bounds on the test_node, it should verify.
  test_node->RecomputeBoundsNoParents();
  ValidateNode(test_node.get(), 2U, 20U);
  // The node we removed should be node 10, as it was exactly in the center.
  EXPECT_EQ(removals[0]->key(), 10);

  // Now remove the next 2.
  removals.clear();
  test_node->RemoveNodesForReinsert(2, &removals);
  EXPECT_EQ(removals.size(), 2U);
  EXPECT_EQ(test_node->count(), 17U);
  test_node->RecomputeBoundsNoParents();
  ValidateNode(test_node.get(), 2U, 20U);
  // Lastly the 2 nodes we should have gotten back are keys 9 and 11, as their
  // centers were the closest to the center of the node bounding box.
  base::hash_set<intptr_t> results_hash;
  results_hash.insert(removals[0]->key());
  results_hash.insert(removals[1]->key());
  EXPECT_EQ(results_hash.count(9), 1U);
  EXPECT_EQ(results_hash.count(11), 1U);
}

TEST_F(RTreeTest, NodeCompareVertical) {
  // One rect with lower y than another should always sort lower than higher y.
  RTree::Node low(Rect(0, 1, 10, 10), 1);
  RTree::Node middle(Rect(0, 5, 10, 10), 5);
  EXPECT_TRUE(RTree::Node::CompareVertical(&low, &middle));
  EXPECT_FALSE(RTree::Node::CompareVertical(&middle, &low));

  // Try a non-overlapping higher-y rectangle.
  RTree::Node high(Rect(-10, 20, 10, 1), 10);
  EXPECT_TRUE(RTree::Node::CompareVertical(&low, &high));
  EXPECT_FALSE(RTree::Node::CompareVertical(&high, &low));

  // Ties are broken by lowest bottom y value.
  RTree::Node shorter_tie(Rect(10, 1, 100, 2), 2);
  EXPECT_TRUE(RTree::Node::CompareVertical(&shorter_tie, &low));
  EXPECT_FALSE(RTree::Node::CompareVertical(&low, &shorter_tie));
}

TEST_F(RTreeTest, NodeCompareHorizontal) {
  // One rect with lower x than another should always sort lower than higher x.
  RTree::Node low(Rect(1, 0, 10, 10), 1);
  RTree::Node middle(Rect(5, 0, 10, 10), 5);
  EXPECT_TRUE(RTree::Node::CompareHorizontal(&low, &middle));
  EXPECT_FALSE(RTree::Node::CompareHorizontal(&middle, &low));

  // Try a non-overlapping higher-x rectangle.
  RTree::Node high(Rect(20, -10, 1, 10), 10);
  EXPECT_TRUE(RTree::Node::CompareHorizontal(&low, &high));
  EXPECT_FALSE(RTree::Node::CompareHorizontal(&high, &low));

  // Ties are broken by lowest bottom x value.
  RTree::Node shorter_tie(Rect(1, 10, 2, 100), 2);
  EXPECT_TRUE(RTree::Node::CompareHorizontal(&shorter_tie, &low));
  EXPECT_FALSE(RTree::Node::CompareHorizontal(&low, &shorter_tie));
}

TEST_F(RTreeTest, NodeCompareCenterDistanceFromParent) {
  // Create a test node we can add children to, for distance comparisons.
  scoped_ptr<RTree::Node> parent(new RTree::Node(0));

  // Add three children, one each with centers at (0, 0), (10, 10), (-9, -9),
  // around which a bounding box will be centered at (0, 0)
  RTree::Node* center_zero = new RTree::Node(Rect(-1, -1, 2, 2), 1);
  parent->AddChild(center_zero);

  RTree::Node* center_positive = new RTree::Node(Rect(9, 9, 2, 2), 2);
  parent->AddChild(center_positive);

  RTree::Node* center_negative = new RTree::Node(Rect(-10, -10, 2, 2), 3);
  parent->AddChild(center_negative);

  ValidateNode(parent.get(), 1U, 5U);
  EXPECT_EQ(parent->rect_, Rect(-10, -10, 21, 21));

  EXPECT_TRUE(RTree::Node::CompareCenterDistanceFromParent(center_zero,
                                                           center_positive));
  EXPECT_FALSE(RTree::Node::CompareCenterDistanceFromParent(center_positive,
                                                            center_zero));

  EXPECT_TRUE(RTree::Node::CompareCenterDistanceFromParent(center_zero,
                                                           center_negative));
  EXPECT_FALSE(RTree::Node::CompareCenterDistanceFromParent(center_negative,
                                                            center_zero));

  EXPECT_TRUE(RTree::Node::CompareCenterDistanceFromParent(center_negative,
                                                           center_positive));
  EXPECT_FALSE(RTree::Node::CompareCenterDistanceFromParent(center_positive,
                                                            center_negative));
}

TEST_F(RTreeTest, NodeOverlapIncreaseToAdd) {
  // Create a test node with three children, for overlap comparisons.
  scoped_ptr<RTree::Node> parent(new RTree::Node(0));

  // Add three children, each 4 wide and tall, at (0, 0), (3, 3), (6, 6) with
  // overlapping corners.
  Rect top(0, 0, 4, 4);
  parent->AddChild(new RTree::Node(top, 1));
  Rect middle(3, 3, 4, 4);
  parent->AddChild(new RTree::Node(middle, 2));
  Rect bottom(6, 6, 4, 4);
  parent->AddChild(new RTree::Node(bottom, 3));
  ValidateNode(parent.get(), 1U, 5U);

  // Test a rect in corner.
  Rect corner(0, 0, 1, 1);
  Rect expanded = top;
  expanded.Union(corner);
  // It should not add any overlap to add this to the first child at (0, 0);
  EXPECT_EQ(parent->OverlapIncreaseToAdd(corner, 0, expanded), 0);

  expanded = middle;
  expanded.Union(corner);
  // Overlap for middle rectangle should increase from 2 pixels at (3, 3) and
  // (6, 6) to 17 pixels, as it will now cover 4x4 rectangle top,
  // so a change of +15
  EXPECT_EQ(parent->OverlapIncreaseToAdd(corner, 1, expanded), 15);

  expanded = bottom;
  expanded.Union(corner);
  // Overlap for bottom rectangle should increase from 1 pixel at (6, 6) to
  // 32 pixels, as it will now cover both 4x4 rectangles top and middle,
  // so a change of 31
  EXPECT_EQ(parent->OverlapIncreaseToAdd(corner, 2, expanded), 31);

  // Test a rect that doesn't overlap with anything, in the far right corner.
  Rect far_corner(9, 0, 1, 1);
  expanded = top;
  expanded.Union(far_corner);
  // Overlap of top should go from 1 to 4, as it will now cover the entire first
  // row of pixels in middle.
  EXPECT_EQ(parent->OverlapIncreaseToAdd(far_corner, 0, expanded), 3);

  expanded = middle;
  expanded.Union(far_corner);
  // Overlap of middle should go from 2 to 8, as it will cover the rightmost 4
  // pixels of top and the top 4 pixles of bottom as it expands.
  EXPECT_EQ(parent->OverlapIncreaseToAdd(far_corner, 1, expanded), 6);

  expanded = bottom;
  expanded.Union(far_corner);
  // Overlap of bottom should go from 1 to 4, as it will now cover the rightmost
  // 4 pixels of middle.
  EXPECT_EQ(parent->OverlapIncreaseToAdd(far_corner, 2, expanded), 3);
}

TEST_F(RTreeTest, NodeBuildLowBounds) {
  ScopedVector<RTree::Node> nodes;
  nodes.reserve(10);
  for (int i = 1; i <= 10; ++i) {
    nodes.push_back(new RTree::Node(Rect(0, 0, i, i), i));
  }
  std::vector<Rect> vertical_bounds;
  std::vector<Rect> horizontal_bounds;
  RTree::Node::BuildLowBounds(
      nodes.get(), nodes.get(), &vertical_bounds, &horizontal_bounds);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(vertical_bounds[i], Rect(0, 0, i + 1, i + 1));
    EXPECT_EQ(horizontal_bounds[i], Rect(0, 0, i + 1, i + 1));
  }
}

TEST_F(RTreeTest, NodeBuildHighBounds) {
  ScopedVector<RTree::Node> nodes;
  nodes.reserve(25);
  for (int i = 0; i < 25; ++i) {
    nodes.push_back(new RTree::Node(Rect(i, i, 25 - i, 25 - i), i));
  }
  std::vector<Rect> vertical_bounds;
  std::vector<Rect> horizontal_bounds;
  RTree::Node::BuildHighBounds(
      nodes.get(), nodes.get(), &vertical_bounds, &horizontal_bounds);
  for (int i = 0; i < 25; ++i) {
    EXPECT_EQ(vertical_bounds[i], Rect(i, i, 25 - i, 25 - i));
    EXPECT_EQ(horizontal_bounds[i], Rect(i, i, 25 - i, 25 - i));
  }
}

TEST_F(RTreeTest, NodeChooseSplitAxisAndIndex) {
  std::vector<Rect> low_vertical_bounds;
  std::vector<Rect> high_vertical_bounds;
  std::vector<Rect> low_horizontal_bounds;
  std::vector<Rect> high_horizontal_bounds;
  // In this test scenario we describe a mirrored, stacked configuration of
  // horizontal, 1 pixel high rectangles labeled a-f like this:
  //
  // shape: | v sort: | h sort: |
  // -------+---------+---------+
  // aaaaa  |    0    |    0    |
  //  bbb   |    1    |    2    |
  //   c    |    2    |    4    |
  //   d    |    3    |    5    |
  //  eee   |    4    |    3    |
  // fffff  |    5    |    1    |
  //
  // These are already sorted vertically from top to bottom. Bounding rectangles
  // of these vertically sorted will be 5 wide, i tall bounding boxes.
  for (int i = 0; i < 6; ++i) {
    low_vertical_bounds.push_back(Rect(0, 0, 5, i + 1));
    high_vertical_bounds.push_back(Rect(0, i, 5, 6 - i));
  }

  // Low bounds of horizontal sort start with bounds of box a and then jump to
  // cover everything, as box f is second in horizontal sort.
  low_horizontal_bounds.push_back(Rect(0, 0, 5, 1));
  for (int i = 0; i < 5; ++i) {
    low_horizontal_bounds.push_back(Rect(0, 0, 5, 6));
  }

  // High horizontal bounds are hand-calculated.
  high_horizontal_bounds.push_back(Rect(0, 0, 5, 6));
  high_horizontal_bounds.push_back(Rect(0, 1, 5, 5));
  high_horizontal_bounds.push_back(Rect(1, 1, 3, 4));
  high_horizontal_bounds.push_back(Rect(1, 2, 3, 3));
  high_horizontal_bounds.push_back(Rect(2, 2, 1, 2));
  high_horizontal_bounds.push_back(Rect(2, 3, 1, 1));

  // This should split vertically, right down the middle.
  EXPECT_TRUE(RTree::Node::ChooseSplitAxis(low_vertical_bounds,
                                           high_vertical_bounds,
                                           low_horizontal_bounds,
                                           high_horizontal_bounds,
                                           2,
                                           5));
  EXPECT_EQ(3U,
            RTree::Node::ChooseSplitIndex(
                2, 5, low_vertical_bounds, high_vertical_bounds));

  // We rotate the shape to test horizontal split axis detection:
  //
  //         +--------+
  //         | a    f |
  //         | ab  ef |
  // sort:   | abcdef |
  //         | ab  ef |
  //         | a    f |
  //         |--------+
  // v sort: | 024531 |
  // h sort: | 012345 |
  //         +--------+
  //
  // Clear out old bounds first.
  low_vertical_bounds.clear();
  high_vertical_bounds.clear();
  low_horizontal_bounds.clear();
  high_horizontal_bounds.clear();

  // Low bounds of vertical sort start with bounds of box a and then jump to
  // cover everything, as box f is second in vertical sort.
  low_vertical_bounds.push_back(Rect(0, 0, 1, 5));
  for (int i = 0; i < 5; ++i) {
    low_vertical_bounds.push_back(Rect(0, 0, 6, 5));
  }

  // High vertical bounds are hand-calculated.
  high_vertical_bounds.push_back(Rect(0, 0, 6, 5));
  high_vertical_bounds.push_back(Rect(1, 0, 5, 5));
  high_vertical_bounds.push_back(Rect(1, 1, 4, 3));
  high_vertical_bounds.push_back(Rect(2, 1, 3, 3));
  high_vertical_bounds.push_back(Rect(2, 2, 2, 1));
  high_vertical_bounds.push_back(Rect(3, 2, 1, 1));

  // These are already sorted horizontally from left to right. Bounding
  // rectangles of these horizontally sorted will be i wide, 5 tall bounding
  // boxes.
  for (int i = 0; i < 6; ++i) {
    low_horizontal_bounds.push_back(Rect(0, 0, i + 1, 5));
    high_horizontal_bounds.push_back(Rect(i, 0, 6 - i, 5));
  }

  // This should split horizontally, right down the middle.
  EXPECT_FALSE(RTree::Node::ChooseSplitAxis(low_vertical_bounds,
                                            high_vertical_bounds,
                                            low_horizontal_bounds,
                                            high_horizontal_bounds,
                                            2,
                                            5));
  EXPECT_EQ(3U,
            RTree::Node::ChooseSplitIndex(
                2, 5, low_horizontal_bounds, high_horizontal_bounds));
}

TEST_F(RTreeTest, NodeDivideChildren) {
  // Create a test node to split.
  scoped_ptr<RTree::Node> test_node(new RTree::Node(0));
  std::vector<RTree::Node*> sorted_children;
  std::vector<Rect> low_bounds;
  std::vector<Rect> high_bounds;
  // Insert 10 record nodes, also inserting them into our children array.
  for (int i = 1; i <= 10; ++i) {
    RTree::Node* node = new RTree::Node(Rect(0, 0, i, i), i);
    test_node->AddChild(node);
    sorted_children.push_back(node);
    low_bounds.push_back(Rect(0, 0, i, i));
    high_bounds.push_back(Rect(0, 0, 10, 10));
  }
  // Split the children in half.
  scoped_ptr<RTree::Node> split_node(
      test_node->DivideChildren(low_bounds, high_bounds, sorted_children, 5));
  // Both nodes should be valid.
  ValidateNode(test_node.get(), 1U, 10U);
  ValidateNode(split_node.get(), 1U, 10U);
  // Both nodes should have five children.
  EXPECT_EQ(test_node->children_.size(), 5U);
  EXPECT_EQ(split_node->children_.size(), 5U);
  // Test node should have children 1-5, split node should have children 6-10.
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(test_node->children_[i]->key(), i + 1);
    EXPECT_EQ(split_node->children_[i]->key(), i + 6);
  }
}

TEST_F(RTreeTest, NodeRemoveChildNoOrphans) {
  scoped_ptr<RTree::Node> test_parent(new RTree::Node(0));
  scoped_ptr<RTree::Node> child_one(new RTree::Node(Rect(0, 0, 1, 1), 1));
  scoped_ptr<RTree::Node> child_two(new RTree::Node(Rect(0, 0, 2, 2), 2));
  scoped_ptr<RTree::Node> child_three(new RTree::Node(Rect(0, 0, 3, 3), 3));
  test_parent->AddChild(child_one.get());
  test_parent->AddChild(child_two.get());
  test_parent->AddChild(child_three.get());
  ValidateNode(test_parent.get(), 1U, 5U);
  // Remove the middle node.
  ScopedVector<RTree::Node> orphans;
  EXPECT_EQ(test_parent->RemoveChild(child_two.get(), &orphans), 2U);
  EXPECT_EQ(orphans.size(), 0U);
  EXPECT_EQ(test_parent->count(), 2U);
  test_parent->RecomputeBoundsNoParents();
  ValidateNode(test_parent.get(), 1U, 5U);
  // Remove the end node.
  EXPECT_EQ(test_parent->RemoveChild(child_three.get(), &orphans), 1U);
  EXPECT_EQ(orphans.size(), 0U);
  EXPECT_EQ(test_parent->count(), 1U);
  test_parent->RecomputeBoundsNoParents();
  ValidateNode(test_parent.get(), 1U, 5U);
  // Remove the first node.
  EXPECT_EQ(test_parent->RemoveChild(child_one.get(), &orphans), 0U);
  EXPECT_EQ(orphans.size(), 0U);
  EXPECT_EQ(test_parent->count(), 0U);
}

TEST_F(RTreeTest, NodeRemoveChildOrphans) {
  // Build flattened binary tree of Nodes 4 deep, from the record nodes up.
  ScopedVector<RTree::Node> nodes;
  nodes.resize(15);
  // Indicies 7 through 15 are record nodes.
  for (int i = 7; i < 15; ++i) {
    nodes[i] = new RTree::Node(Rect(0, 0, i, i), i);
  }
  // Nodes 3 through 6 are level 0 (leaves) and get 2 record nodes each.
  for (int i = 3; i < 7; ++i) {
    nodes[i] = new RTree::Node(0);
    nodes[i]->AddChild(nodes[(i * 2) + 1]);
    nodes[i]->AddChild(nodes[(i * 2) + 2]);
  }
  // Nodes 1 and 2 are level 1 and get 2 leaves each.
  for (int i = 1; i < 3; ++i) {
    nodes[i] = new RTree::Node(1);
    nodes[i]->AddChild(nodes[(i * 2) + 1]);
    nodes[i]->AddChild(nodes[(i * 2) + 2]);
  }
  // Node 0 is level 2 and gets 2 childen.
  nodes[0] = new RTree::Node(2);
  nodes[0]->AddChild(nodes[1]);
  nodes[0]->AddChild(nodes[2]);
  // This should now be a valid node structure.
  ValidateNode(nodes[0], 2U, 2U);

  // Now remove the level 0 nodes, so we get the record nodes as orphans.
  ScopedVector<RTree::Node> orphans;
  EXPECT_EQ(nodes[1]->RemoveChild(nodes[3], &orphans), 1U);
  EXPECT_EQ(nodes[1]->RemoveChild(nodes[4], &orphans), 0U);
  EXPECT_EQ(nodes[2]->RemoveChild(nodes[5], &orphans), 1U);
  EXPECT_EQ(nodes[2]->RemoveChild(nodes[6], &orphans), 0U);

  // Orphans should be nodes 7 through 15 in order.
  EXPECT_EQ(orphans.size(), 8U);
  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(orphans[i], nodes[i + 7]);
  }

  // Now we remove nodes 1 and 2 from the root, expecting no further orphans.
  // This prevents a crash due to double-delete on test exit, as no node should
  // own any other node right now.
  EXPECT_EQ(nodes[0]->RemoveChild(nodes[1], &orphans), 1U);
  EXPECT_EQ(orphans.size(), 8U);
  EXPECT_EQ(nodes[0]->RemoveChild(nodes[2], &orphans), 0U);
  EXPECT_EQ(orphans.size(), 8U);

  // Prevent double-delete of nodes by both nodes and orphans.
  orphans.weak_clear();
}

TEST_F(RTreeTest, NodeRemoveAndReturnLastChild) {
  scoped_ptr<RTree::Node> test_parent(new RTree::Node(0));
  scoped_ptr<RTree::Node> child_one(new RTree::Node(Rect(0, 0, 1, 1), 1));
  scoped_ptr<RTree::Node> child_two(new RTree::Node(Rect(0, 0, 2, 2), 2));
  scoped_ptr<RTree::Node> child_three(new RTree::Node(Rect(0, 0, 3, 3), 3));
  test_parent->AddChild(child_one.get());
  test_parent->AddChild(child_two.get());
  test_parent->AddChild(child_three.get());
  ValidateNode(test_parent.get(), 1U, 5U);

  EXPECT_EQ(test_parent->RemoveAndReturnLastChild().release(),
            child_three.get());
  EXPECT_EQ(test_parent->count(), 2U);
  test_parent->RecomputeBoundsNoParents();
  ValidateNode(test_parent.get(), 1U, 5U);

  EXPECT_EQ(test_parent->RemoveAndReturnLastChild().release(), child_two.get());
  EXPECT_EQ(test_parent->count(), 1U);
  test_parent->RecomputeBoundsNoParents();
  ValidateNode(test_parent.get(), 1U, 5U);

  EXPECT_EQ(test_parent->RemoveAndReturnLastChild().release(), child_one.get());
  EXPECT_EQ(test_parent->count(), 0U);
}

TEST_F(RTreeTest, NodeLeastOverlapIncrease) {
  scoped_ptr<RTree::Node> test_parent(new RTree::Node(0));
  // Construct 4 nodes with 1x2 retangles spaced horizontally 1 pixel apart, or:
  //
  // a b c d
  // a b c d
  //
  for (int i = 0; i < 4; ++i) {
    test_parent->AddChild(new RTree::Node(Rect(i * 2, 0, 1, 2), i + 1));
  }

  ValidateNode(test_parent.get(), 1U, 5U);

  // Test rect at (7, 0) should require minimum overlap on the part of the
  // fourth rectangle to add:
  //
  // a b c dT
  // a b c d
  //
  Rect test_rect_far(7, 0, 1, 1);
  std::vector<Rect> expanded_rects;
  BuildExpandedRects(test_parent.get(), test_rect_far, &expanded_rects);
  RTree::Node* result =
      test_parent->LeastOverlapIncrease(test_rect_far, expanded_rects);
  EXPECT_EQ(result->key(), 4);

  // Test rect covering the bottom half of all children should be a 4-way tie,
  // so LeastOverlapIncrease should return NULL:
  //
  // a b c d
  // TTTTTTT
  //
  Rect test_rect_tie(0, 1, 7, 1);
  BuildExpandedRects(test_parent.get(), test_rect_tie, &expanded_rects);
  result = test_parent->LeastOverlapIncrease(test_rect_tie, expanded_rects);
  EXPECT_TRUE(result == NULL);

  // Test rect completely inside c should return the third rectangle:
  //
  // a b T d
  // a b c d
  //
  Rect test_rect_inside(4, 0, 1, 1);
  BuildExpandedRects(test_parent.get(), test_rect_inside, &expanded_rects);
  result = test_parent->LeastOverlapIncrease(test_rect_inside, expanded_rects);
  EXPECT_EQ(result->key(), 3);

  // Add a rectangle that overlaps completely with rectangle c, to test
  // when there is a tie between two completely contained rectangles:
  //
  // a b Ted
  // a b eed
  //
  test_parent->AddChild(new RTree::Node(Rect(4, 0, 2, 2), 9));
  BuildExpandedRects(test_parent.get(), test_rect_inside, &expanded_rects);
  result = test_parent->LeastOverlapIncrease(test_rect_inside, expanded_rects);
  EXPECT_TRUE(result == NULL);
}

TEST_F(RTreeTest, NodeLeastAreaEnlargement) {
  scoped_ptr<RTree::Node> test_parent(new RTree::Node(0));
  // Construct 4 nodes in a cross-hairs style configuration:
  //
  //  a
  // b c
  //  d
  //
  test_parent->AddChild(new RTree::Node(Rect(1, 0, 1, 1), 1));
  test_parent->AddChild(new RTree::Node(Rect(0, 1, 1, 1), 2));
  test_parent->AddChild(new RTree::Node(Rect(2, 1, 1, 1), 3));
  test_parent->AddChild(new RTree::Node(Rect(1, 2, 1, 1), 4));

  ValidateNode(test_parent.get(), 1U, 5U);

  // Test rect at (1, 3) should require minimum area to add to Node d:
  //
  //  a
  // b c
  //  d
  //  T
  //
  Rect test_rect_below(1, 3, 1, 1);
  std::vector<Rect> expanded_rects;
  BuildExpandedRects(test_parent.get(), test_rect_below, &expanded_rects);
  RTree::Node* result =
      test_parent->LeastAreaEnlargement(test_rect_below, expanded_rects);
  EXPECT_EQ(result->key(), 4);

  // Test rect completely inside b should require minimum area to add to Node b:
  //
  //  a
  // T c
  //  d
  //
  Rect test_rect_inside(0, 1, 1, 1);
  BuildExpandedRects(test_parent.get(), test_rect_inside, &expanded_rects);
  result = test_parent->LeastAreaEnlargement(test_rect_inside, expanded_rects);
  EXPECT_EQ(result->key(), 2);

  // Add e at (0, 1) to overlap b and c, to test tie-breaking:
  //
  //  a
  // eee
  //  d
  //
  test_parent->AddChild(new RTree::Node(Rect(0, 1, 3, 1), 7));

  ValidateNode(test_parent.get(), 1U, 5U);

  // Test rect at (3, 1) should tie between c and e, but c has smaller area so
  // the algorithm should select c:
  //
  //
  //  a
  // eeeT
  //  d
  //
  Rect test_rect_tie_breaker(3, 1, 1, 1);
  BuildExpandedRects(test_parent.get(), test_rect_tie_breaker, &expanded_rects);
  result =
      test_parent->LeastAreaEnlargement(test_rect_tie_breaker, expanded_rects);
  EXPECT_EQ(result->key(), 3);
}

}  // namespace gfx
