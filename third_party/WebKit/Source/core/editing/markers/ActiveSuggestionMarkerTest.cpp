// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/ActiveSuggestionMarker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ActiveSuggestionMarkerTest : public ::testing::Test {};

TEST_F(ActiveSuggestionMarkerTest, MarkerType) {
  DocumentMarker* marker = new ActiveSuggestionMarker(
      0, 1, Color::kTransparent, StyleableMarker::Thickness::kThin,
      Color::kTransparent);
  EXPECT_EQ(DocumentMarker::kActiveSuggestion, marker->GetType());
}

TEST_F(ActiveSuggestionMarkerTest, IsStyleableMarker) {
  DocumentMarker* marker = new ActiveSuggestionMarker(
      0, 1, Color::kTransparent, StyleableMarker::Thickness::kThin,
      Color::kTransparent);
  EXPECT_TRUE(IsStyleableMarker(*marker));
}

TEST_F(ActiveSuggestionMarkerTest, ConstructorAndGetters) {
  ActiveSuggestionMarker* marker = new ActiveSuggestionMarker(
      0, 1, Color::kDarkGray, StyleableMarker::Thickness::kThin, Color::kGray);
  EXPECT_EQ(Color::kDarkGray, marker->UnderlineColor());
  EXPECT_FALSE(marker->IsThick());
  EXPECT_EQ(Color::kGray, marker->BackgroundColor());

  ActiveSuggestionMarker* thick_marker = new ActiveSuggestionMarker(
      0, 1, Color::kDarkGray, StyleableMarker::Thickness::kThick, Color::kGray);
  EXPECT_EQ(true, thick_marker->IsThick());
}

}  // namespace blink
