// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintControllerPaintTest.h"
#include "core/paint/PaintLayerPainter.h"

// This file contains tests testing TablePainter, TableSectionPainter,
// TableRowPainter and TableCellPainter. It's difficult to separate the tests
// into individual files because of dependencies among the painter classes.

namespace blink {

using TablePainterTest = PaintControllerPaintTest;

TEST_F(TablePainterTest, Background) {
  setBodyInnerHTML(
      "<style>"
      "  td { width: 200px; height: 200px; border: none; }"
      "  tr { background-color: blue; }"
      "  table { border: none; border-spacing: 0; border-collapse: collapse; }"
      "</style>"
      "<table>"
      "  <tr id='row1'><td></td></tr>"
      "  <tr id='row2'><td></td></tr>"
      "</table>");

  LayoutView& layoutView = *document().layoutView();
  LayoutObject& row1 = *getLayoutObjectByElementId("row1");
  LayoutObject& row2 = *getLayoutObjectByElementId("row2");

  rootPaintController().invalidateAll();
  document().view()->updateAllLifecyclePhasesExceptPaint();
  IntRect interestRect(0, 0, 200, 200);
  paint(&interestRect);

  EXPECT_DISPLAY_LIST(
      rootPaintController().getDisplayItemList(), 2,
      TestDisplayItem(layoutView, DisplayItem::kDocumentBackground),
      TestDisplayItem(row1, DisplayItem::kBoxDecorationBackground));

  document().view()->updateAllLifecyclePhasesExceptPaint();
  interestRect = IntRect(0, 300, 200, 1000);
  paint(&interestRect);

  EXPECT_DISPLAY_LIST(
      rootPaintController().getDisplayItemList(), 2,
      TestDisplayItem(layoutView, DisplayItem::kDocumentBackground),
      TestDisplayItem(row2, DisplayItem::kBoxDecorationBackground));
}

TEST_F(TablePainterTest, BackgroundWithCellSpacing) {
  setBodyInnerHTML(
      "<style>"
      "  body { margin: 0; }"
      "  td { width: 200px; height: 150px; border: 0; background-color: green; "
      "  }"
      "  tr { background-color: blue; }"
      "  table { border: none; border-spacing: 100px; border-collapse: "
      "separate; }"
      "</style>"
      "<table>"
      "  <tr id='row1'><td id='cell1'></td></tr>"
      "  <tr id='row2'><td id='cell2'></td></tr>"
      "</table>");

  LayoutView& layoutView = *document().layoutView();
  LayoutObject& row1 = *getLayoutObjectByElementId("row1");
  LayoutObject& row2 = *getLayoutObjectByElementId("row2");
  LayoutObject& cell1 = *getLayoutObjectByElementId("cell1");
  LayoutObject& cell2 = *getLayoutObjectByElementId("cell2");

  rootPaintController().invalidateAll();
  document().view()->updateAllLifecyclePhasesExceptPaint();
  // Intersects cell1 and the spacing between cell1 and cell2.
  IntRect interestRect(0, 200, 200, 150);
  paint(&interestRect);

  EXPECT_DISPLAY_LIST(
      rootPaintController().getDisplayItemList(), 3,
      TestDisplayItem(layoutView, DisplayItem::kDocumentBackground),
      TestDisplayItem(row1, DisplayItem::kBoxDecorationBackground),
      TestDisplayItem(cell1, DisplayItem::kBoxDecorationBackground));

  document().view()->updateAllLifecyclePhasesExceptPaint();
  // Intersects the spacing only.
  interestRect = IntRect(0, 250, 100, 100);
  paint(&interestRect);

  EXPECT_DISPLAY_LIST(
      rootPaintController().getDisplayItemList(), 2,
      TestDisplayItem(layoutView, DisplayItem::kDocumentBackground),
      TestDisplayItem(row1, DisplayItem::kBoxDecorationBackground));

  document().view()->updateAllLifecyclePhasesExceptPaint();
  // Intersects cell2 only.
  interestRect = IntRect(0, 350, 200, 150);
  paint(&interestRect);

  EXPECT_DISPLAY_LIST(
      rootPaintController().getDisplayItemList(), 3,
      TestDisplayItem(layoutView, DisplayItem::kDocumentBackground),
      TestDisplayItem(row2, DisplayItem::kBoxDecorationBackground),
      TestDisplayItem(cell2, DisplayItem::kBoxDecorationBackground));
}

TEST_F(TablePainterTest, BackgroundInSelfPaintingRow) {
  setBodyInnerHTML(
      "<style>"
      "  body { margin: 0 }"
      "  td { width: 200px; height: 200px; border: 0; background-color: green; "
      "}"
      "  tr { background-color: blue; opacity: 0.5; }"
      "  table { border: none; border-spacing: 100px; border-collapse: "
      "separate; }"
      "</style>"
      "<table>"
      "  <tr id='row'><td id='cell1'><td id='cell2'></td></tr>"
      "</table>");

  LayoutView& layoutView = *document().layoutView();
  LayoutObject& cell1 = *getLayoutObjectByElementId("cell1");
  LayoutObject& cell2 = *getLayoutObjectByElementId("cell2");
  LayoutObject& row = *getLayoutObjectByElementId("row");

  rootPaintController().invalidateAll();
  document().view()->updateAllLifecyclePhasesExceptPaint();
  // Intersects cell1 and the spacing between cell1 and cell2.
  IntRect interestRect(200, 0, 200, 200);
  paint(&interestRect);

  EXPECT_DISPLAY_LIST(
      rootPaintController().getDisplayItemList(), 5,
      TestDisplayItem(layoutView, DisplayItem::kDocumentBackground),
      TestDisplayItem(row, DisplayItem::kBeginCompositing),
      TestDisplayItem(row, DisplayItem::kBoxDecorationBackground),
      TestDisplayItem(cell1, DisplayItem::kBoxDecorationBackground),
      TestDisplayItem(row, DisplayItem::kEndCompositing));

  document().view()->updateAllLifecyclePhasesExceptPaint();
  // Intersects the spacing only.
  interestRect = IntRect(300, 0, 100, 100);
  paint(&interestRect);

  EXPECT_DISPLAY_LIST(
      rootPaintController().getDisplayItemList(), 1,
      TestDisplayItem(layoutView, DisplayItem::kDocumentBackground));

  document().view()->updateAllLifecyclePhasesExceptPaint();
  // Intersects cell2 only.
  interestRect = IntRect(450, 0, 200, 200);
  paint(&interestRect);

  EXPECT_DISPLAY_LIST(
      rootPaintController().getDisplayItemList(), 5,
      TestDisplayItem(layoutView, DisplayItem::kDocumentBackground),
      TestDisplayItem(row, DisplayItem::kBeginCompositing),
      TestDisplayItem(row, DisplayItem::kBoxDecorationBackground),
      TestDisplayItem(cell2, DisplayItem::kBoxDecorationBackground),
      TestDisplayItem(row, DisplayItem::kEndCompositing));
}

TEST_F(TablePainterTest, CollapsedBorderAndOverflow) {
  setBodyInnerHTML(
      "<style>"
      "  body { margin: 0 }"
      "  td { width: 100px; height: 100px; border: 100px solid blue; outline: "
      "100px solid yellow; background: green; }"
      "  table { margin: 100px; border-collapse: collapse; }"
      "</style>"
      "<table>"
      "  <tr><td id='cell'></td></tr>"
      "</table>");

  LayoutView& layoutView = *document().layoutView();
  LayoutObject& cell = *getLayoutObjectByElementId("cell");

  rootPaintController().invalidateAll();
  document().view()->updateAllLifecyclePhasesExceptPaint();
  // Intersects the overflowing part of cell but not border box.
  IntRect interestRect(0, 0, 100, 100);
  paint(&interestRect);

  // We should paint all display items of cell.
  EXPECT_DISPLAY_LIST(
      rootPaintController().getDisplayItemList(), 4,
      TestDisplayItem(layoutView, DisplayItem::kDocumentBackground),
      TestDisplayItem(cell, DisplayItem::kBoxDecorationBackground),
      TestDisplayItem(cell, DisplayItem::kTableCollapsedBorderLast),
      TestDisplayItem(cell, DisplayItem::paintPhaseToDrawingType(
                                PaintPhaseSelfOutlineOnly)));
}

}  // namespace blink
