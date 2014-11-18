// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_editing_controller.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

}

TEST(TouchEditingControllerTest, SelectionBound) {
  SelectionBound b1, b2;
  // Simple case of aligned vertical bounds of equal height
  b1.edge_top = gfx::Point(0, 20);
  b1.edge_bottom = gfx::Point(0, 25);
  b2.edge_top = gfx::Point(110, 20);
  b2.edge_bottom = gfx::Point(110, 25);
  EXPECT_EQ(gfx::Rect(b1.edge_top,
                      gfx::Size(b2.edge_top.x() - b1.edge_top.x(),
                                b2.edge_bottom.y() - b2.edge_top.y())),
            RectBetweenSelectionBounds(b1, b2));
  EXPECT_EQ(RectBetweenSelectionBounds(b1, b2),
            RectBetweenSelectionBounds(b2, b1));

  // Parallel vertical bounds of different heights
  b1.edge_top = gfx::Point(10, 20);
  b1.edge_bottom = gfx::Point(10, 25);
  b2.edge_top = gfx::Point(110, 0);
  b2.edge_bottom = gfx::Point(110, 35);
  EXPECT_EQ(gfx::Rect(gfx::Point(b1.edge_top.x(), b2.edge_top.y()),
                      gfx::Size(b2.edge_top.x() - b1.edge_top.x(),
                                b2.edge_bottom.y() - b2.edge_top.y())),
            RectBetweenSelectionBounds(b1, b2));
  EXPECT_EQ(RectBetweenSelectionBounds(b1, b2),
            RectBetweenSelectionBounds(b2, b1));

  b1.edge_top = gfx::Point(10, 20);
  b1.edge_bottom = gfx::Point(10, 30);
  b2.edge_top = gfx::Point(110, 25);
  b2.edge_bottom = gfx::Point(110, 45);
  EXPECT_EQ(gfx::Rect(b1.edge_top,
                      gfx::Size(b2.edge_top.x() - b1.edge_top.x(),
                                b2.edge_bottom.y() - b1.edge_top.y())),
            RectBetweenSelectionBounds(b1, b2));
  EXPECT_EQ(RectBetweenSelectionBounds(b1, b2),
            RectBetweenSelectionBounds(b2, b1));

  b1.edge_top = gfx::Point(10, 20);
  b1.edge_bottom = gfx::Point(10, 30);
  b2.edge_top = gfx::Point(110, 40);
  b2.edge_bottom = gfx::Point(110, 60);
  EXPECT_EQ(gfx::Rect(b1.edge_top,
                      gfx::Size(b2.edge_top.x() - b1.edge_top.x(),
                                b2.edge_bottom.y() - b1.edge_top.y())),
            RectBetweenSelectionBounds(b1, b2));
  EXPECT_EQ(RectBetweenSelectionBounds(b1, b2),
            RectBetweenSelectionBounds(b2, b1));

  // Overlapping vertical bounds
  b1.edge_top = gfx::Point(10, 20);
  b1.edge_bottom = gfx::Point(10, 30);
  b2.edge_top = gfx::Point(10, 25);
  b2.edge_bottom = gfx::Point(10, 40);
  EXPECT_EQ(gfx::Rect(b1.edge_top,
                      gfx::Size(0, b2.edge_bottom.y() - b1.edge_top.y())),
            RectBetweenSelectionBounds(b1, b2));
  EXPECT_EQ(RectBetweenSelectionBounds(b1, b2),
            RectBetweenSelectionBounds(b2, b1));

  // Non-vertical bounds: "\ \"
  b1.edge_top = gfx::Point(10, 20);
  b1.edge_bottom = gfx::Point(20, 30);
  b2.edge_top = gfx::Point(110, 40);
  b2.edge_bottom = gfx::Point(120, 60);
  EXPECT_EQ(gfx::Rect(b1.edge_top,
                      gfx::Size(b2.edge_bottom.x() - b1.edge_top.x(),
                                b2.edge_bottom.y() - b1.edge_top.y())),
            RectBetweenSelectionBounds(b1, b2));
  EXPECT_EQ(RectBetweenSelectionBounds(b1, b2),
            RectBetweenSelectionBounds(b2, b1));

  // Non-vertical bounds: "/ \"
  b1.edge_top = gfx::Point(20, 30);
  b1.edge_bottom = gfx::Point(0, 40);
  b2.edge_top = gfx::Point(110, 30);
  b2.edge_bottom = gfx::Point(120, 40);
  EXPECT_EQ(gfx::Rect(gfx::Point(b1.edge_bottom.x(), b1.edge_top.y()),
                      gfx::Size(b2.edge_bottom.x() - b1.edge_bottom.x(),
                                b2.edge_bottom.y() - b2.edge_top.y())),
            RectBetweenSelectionBounds(b1, b2));
  EXPECT_EQ(RectBetweenSelectionBounds(b1, b2),
            RectBetweenSelectionBounds(b2, b1));
}

}  // namespace ui
