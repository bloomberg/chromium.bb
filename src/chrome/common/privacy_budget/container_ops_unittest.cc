// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "chrome/common/privacy_budget/container_ops.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace internal {

namespace {

typedef ::testing::Types<std::unordered_set<int>, std::set<int>> ContainerTypes;

}  // namespace

template <typename T>
class ContainerOpsTest : public ::testing::Test {};

TYPED_TEST_SUITE_P(ContainerOpsTest);

TYPED_TEST_P(ContainerOpsTest, TestExtractRandomSubset) {
  TypeParam from = {1, 2, 3, 4};
  TypeParam to = {5};

  EXPECT_TRUE(ExtractRandomSubset(&from, &to, 1));
  EXPECT_EQ(3u, from.size());
  EXPECT_EQ(2u, to.size());
}

TYPED_TEST_P(ContainerOpsTest, TestExtractRandomSubset_Zero) {
  TypeParam from = {1, 2, 3, 4};
  TypeParam to = {5};

  EXPECT_FALSE(ExtractRandomSubset(&from, &to, 0));
  EXPECT_EQ(from, (TypeParam{1, 2, 3, 4}));
}

TYPED_TEST_P(ContainerOpsTest, TestExtractRandomSubset_Empty) {
  TypeParam to = {5};

  // Moving from an empty set.
  TypeParam from_two;
  EXPECT_FALSE(ExtractRandomSubset(&from_two, &to, 1));
}

TYPED_TEST_P(ContainerOpsTest, TestExtractRandomSubset_Overflow) {
  TypeParam from = {1, 2, 3, 4};
  TypeParam to = {5};
  // Overflow.
  EXPECT_TRUE(ExtractRandomSubset(&from, &to, 100));
  EXPECT_EQ(0u, from.size());
  EXPECT_EQ(5u, to.size());
}

TYPED_TEST_P(ContainerOpsTest, TestExtractRandomSubset_Merge) {
  TypeParam from = {1, 2, 3, 4};
  TypeParam to = {1, 2, 3, 4, 5};
  // Moved element disappears because |from| is a subset of |to|. Assuming the
  // underlying set implementation is correct (we'd be in pretty bad shape if
  // that were not true) verifies that the element being moved is one we expect.
  EXPECT_TRUE(ExtractRandomSubset(&from, &to, 1));
  EXPECT_EQ(3u, from.size());
  EXPECT_EQ(5u, to.size());
}

TYPED_TEST_P(ContainerOpsTest, TestIntersects) {
  TypeParam a = {1, 2, 3, 4};
  TypeParam b = {4, 5, 6, 7};
  TypeParam c = {10, 11};
  TypeParam d;

  EXPECT_TRUE(Intersects(a, b));
  EXPECT_FALSE(Intersects(b, c));
  EXPECT_FALSE(Intersects(b, d));
  EXPECT_FALSE(Intersects(d, d));
}

TYPED_TEST_P(ContainerOpsTest, TestExtractIf_Basic) {
  TypeParam from = {1, 2, 3, 4, 5, 6};
  TypeParam to;

  // Extracts only when predicate is true.
  EXPECT_TRUE(ExtractIf(&from, &to, [](const auto& v) { return v % 2 == 0; }));
  EXPECT_EQ(from, (TypeParam{1, 3, 5}));
  EXPECT_EQ(to, (TypeParam{2, 4, 6}));
}

TYPED_TEST_P(ContainerOpsTest, TestExtractIf_AlwaysFalse) {
  TypeParam from = {1, 3, 5};
  TypeParam to;

  // Noop if predicate is always false.
  EXPECT_FALSE(ExtractIf(&from, &to, [](const auto& v) { return false; }));
  EXPECT_EQ(from, (TypeParam{1, 3, 5}));
  EXPECT_TRUE(to.empty());
}

TYPED_TEST_P(ContainerOpsTest, TestExtractIf_AlwaysTrue) {
  TypeParam from = {1, 2, 3, 4, 5, 6};
  TypeParam to;

  // Just because. Isn't testing anything new.
  EXPECT_TRUE(ExtractIf(&from, &to, [](const auto& v) { return true; }));
  EXPECT_EQ(6u, to.size());
  EXPECT_TRUE(from.empty());
}

TYPED_TEST_P(ContainerOpsTest, TestExtractIf_EmptySource) {
  TypeParam from;
  TypeParam to = {1, 2, 3, 4, 5, 6};

  // Noop if |from| is empty.
  EXPECT_FALSE(ExtractIf(&from, &to, [](const auto& v) { return true; }));
  EXPECT_EQ(6u, to.size());
  EXPECT_TRUE(from.empty());
}

TYPED_TEST_P(ContainerOpsTest, TestSubtractLeftFromRight_WithIntersection) {
  TypeParam a = {1, 2, 3, 4, 5, 6, 7};
  TypeParam b = {2, 3, 4};

  EXPECT_TRUE(SubtractLeftFromRight(b, &a));
  EXPECT_EQ(a, (TypeParam{1, 5, 6, 7}));
}

TYPED_TEST_P(ContainerOpsTest, TestSubtractLeftFromRight_NoIntersection) {
  TypeParam a = {1, 5, 6, 7};
  TypeParam c = {10};

  // Nothing happens if there's no intersection.
  EXPECT_FALSE(SubtractLeftFromRight(c, &a));
  EXPECT_EQ(a, (TypeParam{1, 5, 6, 7}));
}

TYPED_TEST_P(ContainerOpsTest, TestSubtractLeftFromRight_LeftEmpty) {
  TypeParam a = {1, 5, 6, 7};
  TypeParam d;

  // Nothing happens when the left side is empty.
  EXPECT_FALSE(SubtractLeftFromRight(d, &a));
  EXPECT_EQ(a, (TypeParam{1, 5, 6, 7}));
}

REGISTER_TYPED_TEST_SUITE_P(ContainerOpsTest,
                            TestIntersects,
                            TestExtractIf_Basic,
                            TestExtractIf_AlwaysFalse,
                            TestExtractIf_AlwaysTrue,
                            TestExtractIf_EmptySource,
                            TestExtractRandomSubset,
                            TestExtractRandomSubset_Zero,
                            TestExtractRandomSubset_Empty,
                            TestExtractRandomSubset_Overflow,
                            TestExtractRandomSubset_Merge,
                            TestSubtractLeftFromRight_WithIntersection,
                            TestSubtractLeftFromRight_NoIntersection,
                            TestSubtractLeftFromRight_LeftEmpty);

INSTANTIATE_TYPED_TEST_SUITE_P(UnorderedAndOrderedSets,
                               ContainerOpsTest,
                               ContainerTypes);

}  // namespace internal
