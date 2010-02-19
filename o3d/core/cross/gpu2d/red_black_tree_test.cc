/*
 * Copyright 2010, Google Inc.
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

// Tests for the red-black tree class.

#include <vector>

#include "core/cross/gpu2d/red_black_tree.h"
#include "core/cross/gpu2d/tree_test_helpers.h"
#include "gtest/gtest.h"

namespace o3d {
namespace gpu2d {

TEST(RedBlackTreeTest, TestSingleElementInsertion) {
  RedBlackTree<int> tree;
  tree.Insert(5);
  ASSERT_TRUE(tree.Verify());
  EXPECT_TRUE(tree.Contains(5));
}

TEST(RedBlackTreeTest, TestMultipleElementInsertion) {
  RedBlackTree<int> tree;
  tree.Insert(4);
  ASSERT_TRUE(tree.Verify());
  EXPECT_TRUE(tree.Contains(4));
  tree.Insert(3);
  ASSERT_TRUE(tree.Verify());
  EXPECT_TRUE(tree.Contains(3));
  tree.Insert(5);
  ASSERT_TRUE(tree.Verify());
  EXPECT_TRUE(tree.Contains(5));
  EXPECT_TRUE(tree.Contains(4));
  EXPECT_TRUE(tree.Contains(3));
}

TEST(RedBlackTreeTest, TestDuplicateElementInsertion) {
  RedBlackTree<int> tree;
  tree.Insert(3);
  ASSERT_TRUE(tree.Verify());
  tree.Insert(3);
  ASSERT_TRUE(tree.Verify());
  tree.Insert(3);
  ASSERT_TRUE(tree.Verify());
  EXPECT_EQ(3, tree.NumElements());
  EXPECT_TRUE(tree.Contains(3));
}

TEST(RedBlackTreeTest, TestSingleElementInsertionAndDeletion) {
  RedBlackTree<int> tree;
  tree.Insert(5);
  ASSERT_TRUE(tree.Verify());
  EXPECT_TRUE(tree.Contains(5));
  tree.Delete(5);
  ASSERT_TRUE(tree.Verify());
  EXPECT_FALSE(tree.Contains(5));
}

TEST(RedBlackTreeTest, TestMultipleElementInsertionAndDeletion) {
  RedBlackTree<int> tree;
  tree.Insert(4);
  ASSERT_TRUE(tree.Verify());
  EXPECT_TRUE(tree.Contains(4));
  tree.Insert(3);
  ASSERT_TRUE(tree.Verify());
  EXPECT_TRUE(tree.Contains(3));
  tree.Insert(5);
  ASSERT_TRUE(tree.Verify());
  EXPECT_TRUE(tree.Contains(5));
  EXPECT_TRUE(tree.Contains(4));
  EXPECT_TRUE(tree.Contains(3));
  tree.Delete(4);
  ASSERT_TRUE(tree.Verify());
  EXPECT_TRUE(tree.Contains(3));
  EXPECT_FALSE(tree.Contains(4));
  EXPECT_TRUE(tree.Contains(5));
  tree.Delete(5);
  ASSERT_TRUE(tree.Verify());
  EXPECT_TRUE(tree.Contains(3));
  EXPECT_FALSE(tree.Contains(4));
  EXPECT_FALSE(tree.Contains(5));
  EXPECT_EQ(1, tree.NumElements());
}

TEST(RedBlackTreeTest, TestDuplicateElementInsertionAndDeletion) {
  RedBlackTree<int> tree;
  tree.Insert(3);
  ASSERT_TRUE(tree.Verify());
  tree.Insert(3);
  ASSERT_TRUE(tree.Verify());
  tree.Insert(3);
  ASSERT_TRUE(tree.Verify());
  EXPECT_EQ(3, tree.NumElements());
  EXPECT_TRUE(tree.Contains(3));
  tree.Delete(3);
  ASSERT_TRUE(tree.Verify());
  tree.Delete(3);
  ASSERT_TRUE(tree.Verify());
  EXPECT_EQ(1, tree.NumElements());
  EXPECT_TRUE(tree.Contains(3));
  tree.Delete(3);
  ASSERT_TRUE(tree.Verify());
  EXPECT_EQ(0, tree.NumElements());
  EXPECT_FALSE(tree.Contains(3));
}

TEST(RedBlackTreeTest, FailingInsertionRegressionTest1) {
  RedBlackTree<int> tree;
  tree.Insert(5113);
  ASSERT_TRUE(tree.Verify());
  tree.Insert(4517);
  ASSERT_TRUE(tree.Verify());
  tree.Insert(3373);
  ASSERT_TRUE(tree.Verify());
  tree.Insert(9307);
  ASSERT_TRUE(tree.Verify());
  tree.Insert(7077);
  ASSERT_TRUE(tree.Verify());
}

namespace {
void InsertionAndDeletionTest(const int32 seed, const int tree_size) {
  InitRandom(seed);
  const int max_val = tree_size;
  // Build the tree.
  RedBlackTree<int> tree;
  std::vector<int> values;
  for (int i = 0; i < tree_size; i++) {
    int val = NextRandom(max_val);
    tree.Insert(val);
    EXPECT_TRUE(tree.Verify()) << "Test failed for seed " << seed;
    values.push_back(val);
  }
  // Churn the tree's contents.
  for (int i = 0; i < tree_size; i++) {
    // Pick a random value to remove.
    int idx = NextRandom(tree_size);
    int val = values[idx];
    // Remove this value.
    tree.Delete(val);
    EXPECT_TRUE(tree.Verify()) << "Test failed for seed " << seed;
    // Replace it with a new one.
    val = NextRandom(max_val);
    values[idx] = val;
    tree.Insert(val);
    EXPECT_TRUE(tree.Verify()) << "Test failed for seed " << seed;
  }
}
}  // anonymous namespace

TEST(RedBlackTreeTest, RandomDeletionAndInsertionRegressionTest1) {
  InsertionAndDeletionTest(12311, 100);
}

TEST(RedBlackTreeTest, TestRandomDeletionAndInsertion) {
  InsertionAndDeletionTest(GenerateSeed(), 100);
}

}  // namespace gpu2d
}  // namespace o3d

