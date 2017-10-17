// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTableCol.h"

#include "core/layout/LayoutTestHelper.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"

namespace blink {

using LayoutTableColTest = RenderingTest;

TEST_F(LayoutTableColTest, LocalVisualRectSPv1) {
  ScopedSlimmingPaintV175ForTest spv175(false);
  SetBodyInnerHTML(
      "<table id='table' style='width: 200px; height: 200px'>"
      "  <col id='col1' style='visibility: hidden'>"
      "  <col id='col2' style='visibility: collapse'>"
      "  <col id='col3'>"
      "  <tr><td></td><td></td></tr>"
      "</table>");

  auto table_local_visual_rect =
      GetLayoutObjectByElementId("table")->LocalVisualRect();
  EXPECT_NE(LayoutRect(), table_local_visual_rect);
  EXPECT_EQ(table_local_visual_rect,
            GetLayoutObjectByElementId("col1")->LocalVisualRect());
  EXPECT_EQ(table_local_visual_rect,
            GetLayoutObjectByElementId("col2")->LocalVisualRect());
  EXPECT_EQ(table_local_visual_rect,
            GetLayoutObjectByElementId("col3")->LocalVisualRect());
}

TEST_F(LayoutTableColTest, LocalVisualRectSPv175) {
  ScopedSlimmingPaintV175ForTest spv175(true);
  SetBodyInnerHTML(
      "<table style='width: 200px; height: 200px'>"
      "  <col id='col1' style='visibility: hidden'>"
      "  <col id='col2' style='visibility: collapse'>"
      "  <col id='col3'>"
      "  <tr><td></td><td></td></tr>"
      "</table>");

  EXPECT_EQ(LayoutRect(),
            GetLayoutObjectByElementId("col1")->LocalVisualRect());
  EXPECT_EQ(LayoutRect(),
            GetLayoutObjectByElementId("col2")->LocalVisualRect());
  EXPECT_EQ(LayoutRect(),
            GetLayoutObjectByElementId("col3")->LocalVisualRect());
}

}  // namespace blink
