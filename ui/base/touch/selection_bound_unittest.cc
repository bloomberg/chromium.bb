// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/selection_bound.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

TEST(SelectionBoundTest, RectBetweenSelectionBounds) {
  SelectionBound b1, b2;
  // Simple case of aligned vertical bounds of equal height
  b1.SetEdge(gfx::Point(0, 20), gfx::Point(0, 25));
  b2.SetEdge(gfx::Point(110, 20), gfx::Point(110, 25));
  gfx::Rect expected_rect(
      b1.edge_top_rounded().x(),
      b1.edge_top_rounded().y(),
      b2.edge_top_rounded().x() - b1.edge_top_rounded().x(),
      b2.edge_bottom_rounded().y() - b2.edge_top_rounded().y());
  EXPECT_EQ(expected_rect, RectBetweenSelectionBounds(b1, b2));
  EXPECT_EQ(expected_rect, RectBetweenSelectionBounds(b2, b1));

  // Parallel vertical bounds of different heights
  b1.SetEdge(gfx::Point(10, 20), gfx::Point(10, 25));
  b2.SetEdge(gfx::Point(110, 0), gfx::Point(110, 35));
  expected_rect = gfx::Rect(
      b1.edge_top_rounded().x(),
      b2.edge_top_rounded().y(),
      b2.edge_top_rounded().x() - b1.edge_top_rounded().x(),
      b2.edge_bottom_rounded().y() - b2.edge_top_rounded().y());
  EXPECT_EQ(expected_rect, RectBetweenSelectionBounds(b1, b2));
  EXPECT_EQ(expected_rect, RectBetweenSelectionBounds(b2, b1));

  b1.SetEdge(gfx::Point(10, 20), gfx::Point(10, 30));
  b2.SetEdge(gfx::Point(110, 25), gfx::Point(110, 45));
  expected_rect = gfx::Rect(
      b1.edge_top_rounded().x(),
      b1.edge_top_rounded().y(),
      b2.edge_top_rounded().x() - b1.edge_top_rounded().x(),
      b2.edge_bottom_rounded().y() - b1.edge_top_rounded().y());
  EXPECT_EQ(expected_rect, RectBetweenSelectionBounds(b1, b2));
  EXPECT_EQ(expected_rect, RectBetweenSelectionBounds(b2, b1));

  b1.SetEdge(gfx::Point(10, 20), gfx::Point(10, 30));
  b2.SetEdge(gfx::Point(110, 40), gfx::Point(110, 60));
  expected_rect = gfx::Rect(
      b1.edge_top_rounded().x(),
      b1.edge_top_rounded().y(),
      b2.edge_top_rounded().x() - b1.edge_top_rounded().x(),
      b2.edge_bottom_rounded().y() - b1.edge_top_rounded().y());
  EXPECT_EQ(expected_rect, RectBetweenSelectionBounds(b1, b2));
  EXPECT_EQ(expected_rect, RectBetweenSelectionBounds(b2, b1));

  // Overlapping vertical bounds
  b1.SetEdge(gfx::Point(10, 20), gfx::Point(10, 30));
  b2.SetEdge(gfx::Point(10, 25), gfx::Point(10, 40));
  expected_rect = gfx::Rect(
      b1.edge_top_rounded().x(),
      b1.edge_top_rounded().y(),
      0,
      b2.edge_bottom_rounded().y() - b1.edge_top_rounded().y());
  EXPECT_EQ(expected_rect, RectBetweenSelectionBounds(b1, b2));
  EXPECT_EQ(expected_rect, RectBetweenSelectionBounds(b2, b1));

  // Non-vertical bounds: "\ \"
  b1.SetEdge(gfx::Point(10, 20), gfx::Point(20, 30));
  b2.SetEdge(gfx::Point(110, 40), gfx::Point(120, 60));
  expected_rect = gfx::Rect(
      b1.edge_top_rounded().x(),
      b1.edge_top_rounded().y(),
      b2.edge_bottom_rounded().x() - b1.edge_top_rounded().x(),
      b2.edge_bottom_rounded().y() - b1.edge_top_rounded().y());
  EXPECT_EQ(expected_rect, RectBetweenSelectionBounds(b1, b2));
  EXPECT_EQ(expected_rect, RectBetweenSelectionBounds(b2, b1));

  // Non-vertical bounds: "/ \"
  b1.SetEdge(gfx::Point(20, 30), gfx::Point(0, 40));
  b2.SetEdge(gfx::Point(110, 30), gfx::Point(120, 40));
  expected_rect = gfx::Rect(
      b1.edge_bottom_rounded().x(),
      b1.edge_top_rounded().y(),
      b2.edge_bottom_rounded().x() - b1.edge_bottom_rounded().x(),
      b2.edge_bottom_rounded().y() - b2.edge_top_rounded().y());
  EXPECT_EQ(expected_rect, RectBetweenSelectionBounds(b1, b2));
  EXPECT_EQ(expected_rect, RectBetweenSelectionBounds(b2, b1));
}

}  // namespace ui
