// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <gtest/gtest.h>

#include "gestures/include/set.h"

namespace gestures {

class SetTest : public ::testing::Test {};

TEST(SetTest, SimpleTest) {
  const int kMax = 5;
  set<short, kMax> set_a;
  set<short, kMax> set_b;

  EXPECT_EQ(0, set_a.size());
  EXPECT_EQ(0, set_b.size());
  EXPECT_TRUE(set_a.empty());
  set<short, kMax>::iterator a_end = set_a.end();
  set<short, kMax>::iterator a_f = set_a.find(1);
  EXPECT_EQ(a_end, a_f);
  EXPECT_EQ(set_a.end(), set_a.find(1));
  set_a.insert(1);
  EXPECT_EQ(1, set_a.size());
  EXPECT_FALSE(set_a.empty());
  EXPECT_NE(set_a.end(), set_a.find(1));
  set_b.insert(3);
  EXPECT_EQ(1, set_b.size());
  set_b.insert(3);
  EXPECT_EQ(1, set_b.size());
  EXPECT_TRUE(set_a != set_b);
  set_b.erase(3);
  set_b.insert(2);
  set_b.insert(1);
  EXPECT_EQ(2, set_b.size());
  EXPECT_NE(set_b.end(), set_b.find(1));
  EXPECT_NE(set_b.end(), set_b.find(2));
  EXPECT_TRUE(set_b != set_a);
  set_a.insert(2);
  EXPECT_EQ(2, set_a.size());
  EXPECT_TRUE(set_b == set_a);
  set_a.insert(3);
  EXPECT_EQ(3, set_a.size());
  set_b.insert(3);
  EXPECT_EQ(3, set_b.size());
  EXPECT_TRUE(set_b == set_a);
  EXPECT_EQ(0, set_a.erase(4));
  EXPECT_EQ(3, set_a.size());
  EXPECT_TRUE(set_b == set_a);
  EXPECT_EQ(1, set_a.erase(1));
  EXPECT_EQ(2, set_a.size());
  EXPECT_EQ(1, set_b.erase(1));
  EXPECT_EQ(2, set_b.size());
  EXPECT_TRUE(set_b == set_a);
  set_a.clear();
  EXPECT_EQ(0, set_a.size());
  EXPECT_TRUE(set_b != set_a);
  set_b.clear();
  EXPECT_EQ(0, set_b.size());
  EXPECT_TRUE(set_b == set_a);
}

TEST(SetTest, OverflowTest) {
  const int kMax = 3;
  set<short, kMax> the_set;  // holds 3 elts
  the_set.insert(4);
  the_set.insert(5);
  the_set.insert(6);
  the_set.insert(7);
  EXPECT_EQ(kMax, the_set.size());
  EXPECT_NE(the_set.end(), the_set.find(4));
  EXPECT_NE(the_set.end(), the_set.find(5));
  EXPECT_NE(the_set.end(), the_set.find(6));
  EXPECT_EQ(the_set.end(), the_set.find(7));
}

TEST(SetTest, SizeTest) {
  set<short, 2> small;
  set<short, 3> big;
  EXPECT_TRUE(small == big);
  EXPECT_FALSE(small != big);

  small.insert(3);
  big = small;
  EXPECT_TRUE(small == big);

  big.insert(2);
  big.insert(1);
  small = big;
}

}  // namespace gestures
