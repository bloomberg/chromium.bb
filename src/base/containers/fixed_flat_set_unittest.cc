// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/fixed_flat_set.h"

#include "base/ranges/algorithm.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(FixedFlatSetTest, MakeFixedFlatSet) {
  constexpr auto kSet = MakeFixedFlatSet({1, 2, 3, 4});
  static_assert(ranges::is_sorted(kSet), "Error: Set is not sorted.");
  static_assert(ranges::adjacent_find(kSet) == kSet.end(),
                "Error: Set contains repeated elements.");
  EXPECT_THAT(kSet, ::testing::ElementsAre(1, 2, 3, 4));
}

}  // namespace base
