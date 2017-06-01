// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/CompositionMarker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class CompositionMarkerTest : public ::testing::Test {};

TEST_F(CompositionMarkerTest, MarkerType) {
  DocumentMarker* marker = new CompositionMarker(0, 1, Color::kTransparent,
                                                 false, Color::kTransparent);
  EXPECT_EQ(DocumentMarker::kComposition, marker->GetType());
}

TEST_F(CompositionMarkerTest, ConstructorAndGetters) {
  CompositionMarker* marker =
      new CompositionMarker(0, 1, Color::kDarkGray, false, Color::kGray);
  EXPECT_EQ(Color::kDarkGray, marker->UnderlineColor());
  EXPECT_FALSE(marker->Thick());
  EXPECT_EQ(Color::kGray, marker->BackgroundColor());

  CompositionMarker* thick_marker =
      new CompositionMarker(0, 1, Color::kDarkGray, true, Color::kGray);
  EXPECT_EQ(true, thick_marker->Thick());
}

}  // namespace blink
