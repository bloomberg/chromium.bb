// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTableCell.h"
#include "core/paint/PaintControllerPaintTest.h"
#include "core/paint/PaintLayerPainter.h"

// This file contains tests testing TablePainter, TableSectionPainter,
// TableRowPainter and TableCellPainter. It's difficult to separate the tests
// into individual files because of dependencies among the painter classes.

namespace blink {

using TablePainterTest = PaintControllerPaintTest;
INSTANTIATE_TEST_CASE_P(All,
                        TablePainterTest,
                        ::testing::Values(0, kRootLayerScrolling));

TEST_P(TablePainterTest, Background) {
  SetBodyInnerHTML(
      "<style>"
      "  td { width: 200px; height: 200px; padding: 0; border: none; }"
      "  tr { background-color: blue; }"
      "  table { border: none; border-spacing: 0 }"
      "</style>"
      "<table>"
      "  <tr id='row1'><td></td></tr>"
      "  <tr id='row2'><td></td></tr>"
      "</table>");

  LayoutObject& row1 = *GetLayoutObjectByElementId("row1");
  LayoutObject& row2 = *GetLayoutObjectByElementId("row2");

  RootPaintController().InvalidateAll();
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  IntRect interest_rect(0, 0, 200, 200);
  Paint(&interest_rect);

  DisplayItemClient* background_client = nullptr;
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    // With SPv1 and RLS, the document background uses the scrolling contents
    // layer as its DisplayItemClient.
    background_client = GetLayoutView().Layer()->GraphicsLayerBacking();
  } else {
    background_client = &GetLayoutView();
  }

  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 2,
      TestDisplayItem(*background_client, DisplayItem::kDocumentBackground),
      TestDisplayItem(row1, DisplayItem::kBoxDecorationBackground));

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  interest_rect = IntRect(0, 300, 200, 1000);
  Paint(&interest_rect);

  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 2,
      TestDisplayItem(*background_client, DisplayItem::kDocumentBackground),
      TestDisplayItem(row2, DisplayItem::kBoxDecorationBackground));
}

TEST_P(TablePainterTest, BackgroundWithCellSpacing) {
  SetBodyInnerHTML(
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

  LayoutObject& row1 = *GetLayoutObjectByElementId("row1");
  LayoutObject& row2 = *GetLayoutObjectByElementId("row2");
  LayoutObject& cell1 = *GetLayoutObjectByElementId("cell1");
  LayoutObject& cell2 = *GetLayoutObjectByElementId("cell2");

  RootPaintController().InvalidateAll();
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // Intersects cell1 and the spacing between cell1 and cell2.
  IntRect interest_rect(0, 200, 200, 150);
  Paint(&interest_rect);

  DisplayItemClient* background_client = nullptr;
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    // With SPv1 and RLS, the document background uses the scrolling contents
    // layer as its DisplayItemClient.
    background_client = GetLayoutView().Layer()->GraphicsLayerBacking();
  } else {
    background_client = &GetLayoutView();
  }

  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 3,
      TestDisplayItem(*background_client, DisplayItem::kDocumentBackground),
      TestDisplayItem(row1, DisplayItem::kBoxDecorationBackground),
      TestDisplayItem(cell1, DisplayItem::kBoxDecorationBackground));

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // Intersects the spacing only.
  interest_rect = IntRect(0, 250, 100, 100);
  Paint(&interest_rect);

  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 2,
      TestDisplayItem(*background_client, DisplayItem::kDocumentBackground),
      TestDisplayItem(row1, DisplayItem::kBoxDecorationBackground));

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // Intersects cell2 only.
  interest_rect = IntRect(0, 350, 200, 150);
  Paint(&interest_rect);

  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 3,
      TestDisplayItem(*background_client, DisplayItem::kDocumentBackground),
      TestDisplayItem(row2, DisplayItem::kBoxDecorationBackground),
      TestDisplayItem(cell2, DisplayItem::kBoxDecorationBackground));
}

TEST_P(TablePainterTest, BackgroundInSelfPaintingRow) {
  SetBodyInnerHTML(
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

  LayoutObject& cell1 = *GetLayoutObjectByElementId("cell1");
  LayoutObject& cell2 = *GetLayoutObjectByElementId("cell2");
  LayoutObject& row = *GetLayoutObjectByElementId("row");

  RootPaintController().InvalidateAll();
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // Intersects cell1 and the spacing between cell1 and cell2.
  IntRect interest_rect(200, 0, 200, 200);
  Paint(&interest_rect);

  DisplayItemClient* background_client = nullptr;
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    // With SPv1 and RLS, the document background uses the scrolling contents
    // layer as its DisplayItemClient.
    background_client = GetLayoutView().Layer()->GraphicsLayerBacking();
  } else {
    background_client = &GetLayoutView();
  }

  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 5,
      TestDisplayItem(*background_client, DisplayItem::kDocumentBackground),
      TestDisplayItem(row, DisplayItem::kBeginCompositing),
      TestDisplayItem(row, DisplayItem::kBoxDecorationBackground),
      TestDisplayItem(cell1, DisplayItem::kBoxDecorationBackground),
      TestDisplayItem(row, DisplayItem::kEndCompositing));

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // Intersects the spacing only.
  interest_rect = IntRect(300, 0, 100, 100);
  Paint(&interest_rect);

  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 1,
      TestDisplayItem(*background_client, DisplayItem::kDocumentBackground));

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // Intersects cell2 only.
  interest_rect = IntRect(450, 0, 200, 200);
  Paint(&interest_rect);

  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 5,
      TestDisplayItem(*background_client, DisplayItem::kDocumentBackground),
      TestDisplayItem(row, DisplayItem::kBeginCompositing),
      TestDisplayItem(row, DisplayItem::kBoxDecorationBackground),
      TestDisplayItem(cell2, DisplayItem::kBoxDecorationBackground),
      TestDisplayItem(row, DisplayItem::kEndCompositing));
}

TEST_P(TablePainterTest, CollapsedBorderAndOverflow) {
  SetBodyInnerHTML(
      "<style>"
      "  body { margin: 0 }"
      "  td { width: 100px; height: 100px; border: 100px solid blue; outline: "
      "100px solid yellow; background: green; }"
      "  table { margin: 100px; border-collapse: collapse; }"
      "</style>"
      "<table>"
      "  <tr><td id='cell'></td></tr>"
      "</table>");

  auto& cell = *ToLayoutTableCell(GetLayoutObjectByElementId("cell"));

  RootPaintController().InvalidateAll();
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // Intersects the overflowing part of cell but not border box.
  IntRect interest_rect(0, 0, 100, 100);
  Paint(&interest_rect);

  DisplayItemClient* background_client = nullptr;
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    // With SPv1 and RLS, the document background uses the scrolling contents
    // layer as its DisplayItemClient.
    background_client = GetLayoutView().Layer()->GraphicsLayerBacking();
  } else {
    background_client = &GetLayoutView();
  }

  // We should paint all display items of cell.
  EXPECT_DISPLAY_LIST(
      RootPaintController().GetDisplayItemList(), 4,
      TestDisplayItem(*background_client, DisplayItem::kDocumentBackground),
      TestDisplayItem(cell, DisplayItem::kBoxDecorationBackground),
      TestDisplayItem(*cell.Row(), DisplayItem::kTableCollapsedBorders),
      TestDisplayItem(cell, DisplayItem::PaintPhaseToDrawingType(
                                kPaintPhaseSelfOutlineOnly)));
}

}  // namespace blink
