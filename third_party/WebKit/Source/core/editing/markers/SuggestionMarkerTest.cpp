// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/SuggestionMarker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class SuggestionMarkerTest : public ::testing::Test {};

TEST_F(SuggestionMarkerTest, MarkerType) {
  DocumentMarker* marker = new SuggestionMarker(
      0, 1, Vector<String>(), Color::kTransparent, Color::kTransparent,
      StyleableMarker::Thickness::kThin, Color::kTransparent);
  EXPECT_EQ(DocumentMarker::kSuggestion, marker->GetType());
}

TEST_F(SuggestionMarkerTest, IsStyleableMarker) {
  DocumentMarker* marker = new SuggestionMarker(
      0, 1, Vector<String>(), Color::kTransparent, Color::kTransparent,
      StyleableMarker::Thickness::kThin, Color::kTransparent);
  EXPECT_TRUE(IsStyleableMarker(*marker));
}

TEST_F(SuggestionMarkerTest, ConstructorAndGetters) {
  Vector<String> suggestions = {"this", "that"};
  SuggestionMarker* marker = new SuggestionMarker(
      0, 1, suggestions, Color::kTransparent, Color::kDarkGray,
      StyleableMarker::Thickness::kThin, Color::kGray);
  EXPECT_EQ(suggestions, marker->Suggestions());
  EXPECT_EQ(Color::kTransparent, marker->SuggestionHighlightColor());
  EXPECT_EQ(Color::kDarkGray, marker->UnderlineColor());
  EXPECT_FALSE(marker->IsThick());
  EXPECT_EQ(Color::kGray, marker->BackgroundColor());

  SuggestionMarker* marker2 = new SuggestionMarker(
      0, 1, Vector<String>(), Color::kBlack, Color::kDarkGray,
      StyleableMarker::Thickness::kThick, Color::kGray);
  EXPECT_TRUE(marker2->IsThick());
  EXPECT_EQ(marker2->SuggestionHighlightColor(), Color::kBlack);
}

TEST_F(SuggestionMarkerTest, SetSuggestion) {
  Vector<String> suggestions = {"this", "that"};
  SuggestionMarker* marker = new SuggestionMarker(
      0, 1, suggestions, Color::kTransparent, Color::kDarkGray,
      StyleableMarker::Thickness::kThin, Color::kGray);

  marker->SetSuggestion(1, "these");

  EXPECT_EQ(2u, marker->Suggestions().size());

  EXPECT_EQ("this", marker->Suggestions()[0]);
  EXPECT_EQ("these", marker->Suggestions()[1]);
}

}  // namespace blink
