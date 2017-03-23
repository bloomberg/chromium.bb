// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/DocumentMarker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class DocumentMarkerTest : public ::testing::Test {};

TEST_F(DocumentMarkerTest, MarkerTypeIteratorEmpty) {
  DocumentMarker::MarkerTypes types(0);
  EXPECT_TRUE(types.begin() == types.end());
}

TEST_F(DocumentMarkerTest, MarkerTypeIteratorOne) {
  DocumentMarker::MarkerTypes types(DocumentMarker::Spelling);
  ASSERT_TRUE(types.begin() != types.end());
  auto it = types.begin();
  EXPECT_EQ(DocumentMarker::Spelling, *it);
  ++it;
  EXPECT_TRUE(it == types.end());
}

TEST_F(DocumentMarkerTest, MarkerTypeIteratorConsecutive) {
  DocumentMarker::MarkerTypes types(0b11);  // Spelling | Grammar
  ASSERT_TRUE(types.begin() != types.end());
  auto it = types.begin();
  EXPECT_EQ(DocumentMarker::Spelling, *it);
  ++it;
  EXPECT_EQ(DocumentMarker::Grammar, *it);
  ++it;
  EXPECT_TRUE(it == types.end());
}

TEST_F(DocumentMarkerTest, MarkerTypeIteratorDistributed) {
  DocumentMarker::MarkerTypes types(0b101);  // Spelling | TextMatch
  ASSERT_TRUE(types.begin() != types.end());
  auto it = types.begin();
  EXPECT_EQ(DocumentMarker::Spelling, *it);
  ++it;
  EXPECT_EQ(DocumentMarker::TextMatch, *it);
  ++it;
  EXPECT_TRUE(it == types.end());
}
}
