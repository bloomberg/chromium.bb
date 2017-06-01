// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/SpaceSplitString.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(SpaceSplitStringTest, Set) {
  SpaceSplitString tokens;

  tokens.Set("foo");
  EXPECT_EQ(1u, tokens.size());
  EXPECT_EQ(AtomicString("foo"), tokens[0]);

  tokens.Set(" foo\t");
  EXPECT_EQ(1u, tokens.size());
  EXPECT_EQ(AtomicString("foo"), tokens[0]);

  tokens.Set("foo foo\t");
  EXPECT_EQ(1u, tokens.size());
  EXPECT_EQ(AtomicString("foo"), tokens[0]);

  tokens.Set("foo foo  foo");
  EXPECT_EQ(1u, tokens.size());
  EXPECT_EQ(AtomicString("foo"), tokens[0]);

  tokens.Set("foo foo bar foo");
  EXPECT_EQ(2u, tokens.size());
  EXPECT_EQ(AtomicString("foo"), tokens[0]);
  EXPECT_EQ(AtomicString("bar"), tokens[1]);
}
}
