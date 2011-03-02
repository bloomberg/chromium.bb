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

// Tests for the interval tree class.

#include "core/cross/gpu2d/interval_tree.h"
#include "core/cross/gpu2d/tree_test_helpers.h"
#include "gtest/gtest.h"

namespace o3d {
namespace gpu2d {

TEST(IntervalTreeTest, TestInsertion) {
  IntervalTree<float> tree;
  tree.Insert(Interval<float>(2, 4));
  ASSERT_TRUE(tree.Verify());
}

TEST(IntervalTreeTest, TestInsertionAndQuery) {
  IntervalTree<float> tree;
  tree.Insert(Interval<float>(2, 4));
  ASSERT_TRUE(tree.Verify());
  std::vector<Interval<float> > res =
      tree.AllOverlaps(Interval<float>(1, 3));
  EXPECT_EQ(1U, res.size());
  EXPECT_EQ(2, res[0].low());
  EXPECT_EQ(4, res[0].high());
}

TEST(IntervalTreeTest, TestQueryAgainstZeroSizeInterval) {
  IntervalTree<float> tree;
  tree.Insert(Interval<float>(1, 2.5));
  tree.Insert(Interval<float>(3.5, 5));
  tree.Insert(Interval<float>(2, 4));
  ASSERT_TRUE(tree.Verify());
  std::vector<Interval<float> > res =
      tree.AllOverlaps(Interval<float>(3, 3));
  EXPECT_EQ(1U, res.size());
  EXPECT_EQ(2, res[0].low());
  EXPECT_EQ(4, res[0].high());
}

TEST(IntervalTreeTest, TestDuplicateElementInsertion) {
  IntervalTree<float, int*> tree;
  int tmp1 = 1;
  int tmp2 = 2;
  typedef IntervalTree<float, int*>::IntervalType IntervalType;
  IntervalType interval1(1, 3, &tmp1);
  IntervalType interval2(1, 3, &tmp2);
  tree.Insert(interval1);
  tree.Insert(interval2);
  ASSERT_TRUE(tree.Verify());
  EXPECT_TRUE(tree.Contains(interval1));
  EXPECT_TRUE(tree.Contains(interval2));
  EXPECT_TRUE(tree.Delete(interval1));
  EXPECT_TRUE(tree.Contains(interval2));
  EXPECT_FALSE(tree.Contains(interval1));
  EXPECT_TRUE(tree.Delete(interval2));
  EXPECT_EQ(0, tree.NumElements());
}

namespace {
struct UserData1 {
 public:
  UserData1() : a(0), b(1) {
  }

  float a;
  int b;
};
}  // anonymous namespace

TEST(IntervalTreeTest, TestInsertionOfComplexUserData) {
  IntervalTree<float, UserData1> tree;
  UserData1 data1;
  data1.a = 5;
  data1.b = 6;
  tree.Insert(tree.MakeInterval(2, 4, data1));
  ASSERT_TRUE(tree.Verify());
}

TEST(IntervalTreeTest, TestQueryingOfComplexUserData) {
  IntervalTree<float, UserData1> tree;
  UserData1 data1;
  data1.a = 5;
  data1.b = 6;
  tree.Insert(tree.MakeInterval(2, 4, data1));
  ASSERT_TRUE(tree.Verify());
  std::vector<Interval<float, UserData1> > overlaps =
      tree.AllOverlaps(tree.MakeInterval(3, 5, data1));
  EXPECT_EQ(1U, overlaps.size());
  EXPECT_EQ(5, overlaps[0].data().a);
  EXPECT_EQ(6, overlaps[0].data().b);
}

namespace {
class EndpointType1 {
 public:
  explicit EndpointType1(int val) : val_(val) {
  }

  bool operator<(const EndpointType1& other) const {
    return val_ < other.val_;
  }

  bool operator==(const EndpointType1& other) const {
    return val_ == other.val_;
  }

 private:
  int val_;
  // These operators should not be required by the interval tree.
  bool operator>(const EndpointType1& other);
  bool operator<=(const EndpointType1& other);
  bool operator>=(const EndpointType1& other);
  bool operator!=(const EndpointType1& other);
};
}  // anonymous namespace

TEST(IntervalTreeTest, TestTreeDoesNotRequireMostOperators) {
  IntervalTree<EndpointType1> tree;
  tree.Insert(tree.MakeInterval(EndpointType1(1), EndpointType1(2)));
  ASSERT_TRUE(tree.Verify());
}

static void
InsertionAndDeletionTest(unsigned seed, int tree_size) {
  InitRandom(seed);
  int max_val = tree_size;
  // Build the tree
  IntervalTree<int> tree;
  std::vector<Interval<int> > added_elements;
  std::vector<Interval<int> > removed_elements;
  for (int i = 0; i < tree_size; i++) {
    int left = NextRandom(max_val);
    int len = NextRandom(max_val);
    Interval<int> interval(left, left + len);
    tree.Insert(interval);
    added_elements.push_back(interval);
  }
  // Churn the tree's contents.
  // First remove half of the elements in random order.
  for (int i = 0; i < tree_size / 2; i++) {
    int idx = NextRandom(added_elements.size());
    tree.Insert(added_elements[idx]);
    EXPECT_TRUE(tree.Verify()) << "Test failed for seed " << seed;
    added_elements.erase(added_elements.begin() + idx);
  }
  // Now randomly add or remove elements.
  for (int i = 0; i < 2 * tree_size; i++) {
    bool add = false;
    if (added_elements.empty()) {
      add = true;
    } else if (removed_elements.empty()) {
      add = false;
    } else {
      add = (NextRandom(2) == 1);
    }
    if (add) {
      int idx = NextRandom(removed_elements.size());
      tree.Insert(removed_elements[idx]);
      added_elements.push_back(removed_elements[idx]);
      removed_elements.erase(removed_elements.begin() + idx);
    } else {
      int idx = NextRandom(added_elements.size());
      EXPECT_TRUE(tree.Contains(added_elements[idx]))
          << "Test failed for seed " << seed;
      EXPECT_TRUE(tree.Delete(added_elements[idx]))
          << "Test failed for seed " << seed;
      removed_elements.push_back(added_elements[idx]);
      added_elements.erase(added_elements.begin() + idx);
    }
    EXPECT_TRUE(tree.Verify()) << "Test failed for seed " << seed;
  }
}

TEST(IntervalTreeTest, RandomDeletionAndInsertionRegressionTest1) {
  InsertionAndDeletionTest((unsigned) 13972, 100);
}

TEST(IntervalTreeTest, TestRandomDeletionAndInsertion) {
  InsertionAndDeletionTest((unsigned) GenerateSeed(), 1000);
}

}  // namespace gpu2d
}  // namespace o3d

