// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_priority.h"
#include <gtest/gtest.h>

namespace blink {

namespace {

CascadePriority AuthorPriority(uint16_t tree_order, uint32_t position) {
  return CascadePriority(CascadeOrigin::kAuthor, false, tree_order, position);
}

CascadePriority ImportantAuthorPriority(uint16_t tree_order,
                                        uint32_t position) {
  return CascadePriority(CascadeOrigin::kAuthor, true, tree_order, position);
}

}  // namespace

TEST(CascadePriorityTest, EncodeOriginImportance) {
  using Origin = CascadeOrigin;
  EXPECT_EQ(0b00001ull, EncodeOriginImportance(Origin::kUserAgent, false));
  EXPECT_EQ(0b00010ull, EncodeOriginImportance(Origin::kUser, false));
  EXPECT_EQ(0b00011ull, EncodeOriginImportance(Origin::kAuthor, false));
  EXPECT_EQ(0b00100ull, EncodeOriginImportance(Origin::kAnimation, false));
  EXPECT_EQ(0b01100ull, EncodeOriginImportance(Origin::kAuthor, true));
  EXPECT_EQ(0b01101ull, EncodeOriginImportance(Origin::kUser, true));
  EXPECT_EQ(0b01110ull, EncodeOriginImportance(Origin::kUserAgent, true));
  EXPECT_EQ(0b10000ull, EncodeOriginImportance(Origin::kTransition, false));
}

TEST(CascadePriorityTest, OriginOperators) {
  std::vector<CascadePriority> priority = {
      CascadePriority(CascadeOrigin::kTransition, false, 0, 0),
      CascadePriority(CascadeOrigin::kAnimation, false, 0, 0),
      CascadePriority(CascadeOrigin::kAuthor, false, 0, 0),
      CascadePriority(CascadeOrigin::kUser, false, 0, 0),
      CascadePriority(CascadeOrigin::kUserAgent, false, 0, 0),
      CascadePriority(CascadeOrigin::kNone, false, 0, 0)};

  for (size_t i = 0; i < priority.size(); ++i) {
    for (size_t j = i; j < priority.size(); ++j) {
      EXPECT_GE(priority[i], priority[j]);
      EXPECT_FALSE(priority[i] < priority[j]);
    }
  }

  for (size_t i = 0; i < priority.size(); ++i) {
    for (size_t j = i + 1; j < priority.size(); ++j) {
      EXPECT_LT(priority[j], priority[i]);
      EXPECT_FALSE(priority[j] >= priority[i]);
    }
  }

  for (CascadePriority priority : priority)
    EXPECT_EQ(priority, priority);

  for (size_t i = 0; i < priority.size(); ++i) {
    for (size_t j = 0; j < priority.size(); ++j) {
      if (i == j)
        continue;
      EXPECT_NE(priority[i], priority[j]);
    }
  }
}

TEST(CascadePriorityTest, OriginImportance) {
  std::vector<CascadePriority> priority = {
      CascadePriority(CascadeOrigin::kTransition, false, 0, 0),
      CascadePriority(CascadeOrigin::kUserAgent, true, 0, 0),
      CascadePriority(CascadeOrigin::kUser, true, 0, 0),
      CascadePriority(CascadeOrigin::kAuthor, true, 0, 0),
      CascadePriority(CascadeOrigin::kAnimation, false, 0, 0),
      CascadePriority(CascadeOrigin::kAuthor, false, 0, 0),
      CascadePriority(CascadeOrigin::kUser, false, 0, 0),
      CascadePriority(CascadeOrigin::kUserAgent, false, 0, 0),
      CascadePriority(CascadeOrigin::kNone, false, 0, 0)};

  for (size_t i = 0; i < priority.size(); ++i) {
    for (size_t j = i; j < priority.size(); ++j)
      EXPECT_GE(priority[i], priority[j]);
  }
}

TEST(CascadePriorityTest, IsImportant) {
  using Priority = CascadePriority;
  using Origin = CascadeOrigin;

  EXPECT_FALSE(Priority(Origin::kUserAgent, false, 0, 0).IsImportant());
  EXPECT_FALSE(Priority(Origin::kUser, false, 0, 0).IsImportant());
  EXPECT_FALSE(Priority(Origin::kAuthor, false, 0, 0).IsImportant());
  EXPECT_FALSE(Priority(Origin::kAnimation, false, 0, 0).IsImportant());
  EXPECT_FALSE(Priority(Origin::kTransition, false, 0, 0).IsImportant());
  EXPECT_FALSE(Priority(Origin::kAuthor, false, 1024, 2048).IsImportant());

  EXPECT_TRUE(Priority(Origin::kUserAgent, true, 0, 0).IsImportant());
  EXPECT_TRUE(Priority(Origin::kUser, true, 0, 0).IsImportant());
  EXPECT_TRUE(Priority(Origin::kAuthor, true, 0, 0).IsImportant());
  EXPECT_TRUE(Priority(Origin::kAnimation, true, 0, 0).IsImportant());
  EXPECT_TRUE(Priority(Origin::kTransition, true, 0, 0).IsImportant());
  EXPECT_TRUE(Priority(Origin::kAuthor, true, 1024, 2048).IsImportant());
}

static std::vector<CascadeOrigin> all_origins = {
    CascadeOrigin::kUserAgent, CascadeOrigin::kUser, CascadeOrigin::kAuthor,
    CascadeOrigin::kTransition, CascadeOrigin::kAnimation};

TEST(CascadePriorityTest, GetOrigin) {
  for (CascadeOrigin origin : all_origins)
    EXPECT_EQ(CascadePriority(origin, false, 0, 0).GetOrigin(), origin);

  for (CascadeOrigin origin : all_origins) {
    if (origin == CascadeOrigin::kAnimation)
      continue;
    if (origin == CascadeOrigin::kTransition)
      continue;
    EXPECT_EQ(CascadePriority(origin, true, 0, 0).GetOrigin(), origin);
  }
}

TEST(CascadePriorityTest, HasOrigin) {
  for (CascadeOrigin origin : all_origins) {
    if (origin != CascadeOrigin::kNone)
      EXPECT_TRUE(CascadePriority(origin).HasOrigin());
    else
      EXPECT_FALSE(CascadePriority(origin).HasOrigin());
  }
  EXPECT_FALSE(CascadePriority().HasOrigin());
}

TEST(CascadePriorityTest, EncodeTreeOrder) {
  EXPECT_EQ(0ull, EncodeTreeOrder(0, false));
  EXPECT_EQ(1ull, EncodeTreeOrder(1, false));
  EXPECT_EQ(2ull, EncodeTreeOrder(2, false));
  EXPECT_EQ(100ull, EncodeTreeOrder(100, false));
  EXPECT_EQ(0xFFFFull, EncodeTreeOrder(0xFFFF, false));

  EXPECT_EQ(0ull ^ 0xFFFF, EncodeTreeOrder(0, true));
  EXPECT_EQ(1ull ^ 0xFFFF, EncodeTreeOrder(1, true));
  EXPECT_EQ(2ull ^ 0xFFFF, EncodeTreeOrder(2, true));
  EXPECT_EQ(100ull ^ 0xFFFF, EncodeTreeOrder(100, true));
  EXPECT_EQ(0xFFFFull ^ 0xFFFF, EncodeTreeOrder(0xFFFF, true));
}

TEST(CascadePriorityTest, TreeOrder) {
  using Priority = CascadePriority;
  CascadeOrigin origin = CascadeOrigin::kAuthor;
  EXPECT_GE(Priority(origin, false, 1), Priority(origin, false, 0));
  EXPECT_GE(Priority(origin, false, 7), Priority(origin, false, 6));
  EXPECT_GE(Priority(origin, false, 42), Priority(origin, false, 42));
  EXPECT_FALSE(Priority(origin, false, 1) >= Priority(origin, false, 8));
}

TEST(CascadePriorityTest, TreeOrderImportant) {
  using Priority = CascadePriority;
  CascadeOrigin origin = CascadeOrigin::kAuthor;
  EXPECT_GE(Priority(origin, true, 0), Priority(origin, true, 1));
  EXPECT_GE(Priority(origin, true, 6), Priority(origin, true, 7));
  EXPECT_GE(Priority(origin, true, 42), Priority(origin, true, 42));
  EXPECT_FALSE(Priority(origin, true, 8) >= Priority(origin, true, 1));
}

TEST(CascadePriorityTest, TreeOrderDifferentOrigin) {
  using Priority = CascadePriority;
  // Tree order does not matter if the origin is different.
  CascadeOrigin author = CascadeOrigin::kAuthor;
  CascadeOrigin transition = CascadeOrigin::kTransition;
  EXPECT_GE(Priority(transition, 1), Priority(author, 42));
  EXPECT_GE(Priority(transition, 1), Priority(author, 1));
}

TEST(CascadePriorityTest, Position) {
  // AuthorPriority(tree_order, position)
  EXPECT_GE(AuthorPriority(0, 0), AuthorPriority(0, 0));
  EXPECT_GE(AuthorPriority(0, 1), AuthorPriority(0, 1));
  EXPECT_GE(AuthorPriority(0, 1), AuthorPriority(0, 0));
  EXPECT_GE(AuthorPriority(0, 2), AuthorPriority(0, 1));
  EXPECT_GE(AuthorPriority(0, 0xFFFFFFFF), AuthorPriority(0, 0xFFFFFFFE));
  EXPECT_FALSE(AuthorPriority(0, 2) >= AuthorPriority(0, 3));
}

TEST(CascadePriorityTest, PositionAndTreeOrder) {
  // AuthorPriority(tree_order, position)
  EXPECT_GE(AuthorPriority(1, 0), AuthorPriority(0, 0));
  EXPECT_GE(AuthorPriority(1, 1), AuthorPriority(0, 1));
  EXPECT_GE(AuthorPriority(1, 1), AuthorPriority(0, 3));
  EXPECT_GE(AuthorPriority(1, 2), AuthorPriority(0, 0xFFFFFFFF));
}

TEST(CascadePriorityTest, PositionAndOrigin) {
  // [Important]AuthorPriority(tree_order, position)
  EXPECT_GE(ImportantAuthorPriority(0, 0), AuthorPriority(0, 0));
  EXPECT_GE(ImportantAuthorPriority(0, 1), AuthorPriority(0, 1));
  EXPECT_GE(ImportantAuthorPriority(0, 1), AuthorPriority(0, 3));
  EXPECT_GE(ImportantAuthorPriority(0, 2), AuthorPriority(0, 0xFFFFFFFF));
}

TEST(CascadePriorityTest, Generation) {
  CascadePriority ua(CascadeOrigin::kUserAgent);
  CascadePriority author(CascadeOrigin::kAuthor);

  EXPECT_EQ(author, author);
  EXPECT_GE(CascadePriority(author, 1), author);
  EXPECT_GE(CascadePriority(author, 2), CascadePriority(author, 1));
  EXPECT_EQ(CascadePriority(author, 2), CascadePriority(author, 2));

  EXPECT_LT(ua, author);
  EXPECT_LT(CascadePriority(ua, 1), author);
  EXPECT_LT(CascadePriority(ua, 2), CascadePriority(author, 1));
  EXPECT_LT(CascadePriority(ua, 2), CascadePriority(author, 2));
  EXPECT_LT(CascadePriority(ua, 2), CascadePriority(author, 3));
}

TEST(CascadePriorityTest, GenerationOverwrite) {
  CascadePriority ua(CascadeOrigin::kUserAgent);

  for (int8_t g = 0; g < 16; ++g) {
    ua = CascadePriority(ua, g);
    EXPECT_EQ(g, ua.GetGeneration());
  }

  for (int8_t g = 15; g >= 0; --g) {
    ua = CascadePriority(ua, g);
    EXPECT_EQ(g, ua.GetGeneration());
  }
}

TEST(CascadePriorityTest, PositionEncoding) {
  // Test 0b0, 0b1, 0b11, 0b111, etc.
  uint32_t pos = 0;
  do {
    // AuthorPriority(tree_order, position)
    ASSERT_EQ(pos, AuthorPriority(0, pos).GetPosition());
    pos = (pos << 1) | 1;
  } while (pos != ~static_cast<uint32_t>(0));

  // Test 0b1, 0b10, 0b100, etc
  pos = 1;
  do {
    // AuthorPriority(tree_order, position)
    ASSERT_EQ(pos, AuthorPriority(0, pos).GetPosition());
    pos <<= 1;
  } while (pos != ~static_cast<uint32_t>(1) << 31);
}

}  // namespace blink
