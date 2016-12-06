// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintControllerPaintTest.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

namespace blink {

using TablePainterTest = PaintControllerPaintTest;

TEST_F(TablePainterTest, CollapsedBorderInterestRectChange) {
  setBodyInnerHTML(
      "<style>"
      "  table { border-collapse: collapse; position: absolute; }"
      "  td { width: 100px; height: 100px; border: 2px solid blue; }"
      "</style>"
      "<table id='table'>"
      "  <tr><td></td><td></td><td></td><td></td></tr>"
      "  <tr><td></td><td></td><td></td><td></td></tr>"
      "  <tr><td></td><td></td><td></td><td></td></tr>"
      "  <tr><td></td><td></td><td></td><td></td></tr>"
      "</table>");

  PaintLayer& htmlLayer =
      *toLayoutBoxModelObject(document().documentElement()->layoutObject())
           ->layer();
  LayoutObject& table = *getLayoutObjectByElementId("table");

  rootPaintController().invalidateAll();
  document().view()->updateAllLifecyclePhasesExceptPaint();
  IntRect interestRect(300, 300, 300, 300);
  paint(&interestRect);

  EXPECT_DISPLAY_LIST(
      rootPaintController().getDisplayItemList(), 4,
      TestDisplayItem(layoutView(), documentBackgroundType),
      TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
      TestDisplayItem(table, DisplayItem::kTableCollapsedBorders),
      TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence));
  // Painted collapsed borders of the central 4 cells, each 4 operations.
  EXPECT_EQ(16, static_cast<const DrawingDisplayItem&>(
                    rootPaintController().getDisplayItemList()[2])
                    .picture()
                    ->approximateOpCount());

  // Should repaint collapsed borders if the previous paint didn't fully paint
  // and interest rect changes.
  document().view()->updateAllLifecyclePhasesExceptPaint();
  interestRect = IntRect(0, 0, 1000, 1000);
  EXPECT_TRUE(paintWithoutCommit(&interestRect));
  EXPECT_EQ(1, numCachedNewItems());
  commit();
  EXPECT_DISPLAY_LIST(
      rootPaintController().getDisplayItemList(), 4,
      TestDisplayItem(layoutView(), documentBackgroundType),
      TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
      TestDisplayItem(table, DisplayItem::kTableCollapsedBorders),
      TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence));
  // Painted collapsed borders of all 16 cells, each 4 operations.
  EXPECT_EQ(64, static_cast<const DrawingDisplayItem&>(
                    rootPaintController().getDisplayItemList()[2])
                    .picture()
                    ->approximateOpCount());

  // Should not repaint collapsed borders if the previous paint fully painted
  // and interest rect changes.
  document().view()->updateAllLifecyclePhasesExceptPaint();
  interestRect = IntRect(0, 0, 400, 400);
  EXPECT_TRUE(paintWithoutCommit(&interestRect));
  EXPECT_EQ(4, numCachedNewItems());
  commit();
}

}  // namespace blink
