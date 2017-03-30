// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTableSection.h"

#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTestHelper.h"

namespace blink {

namespace {

using LayoutTableSectionTest = RenderingTest;

TEST_F(LayoutTableSectionTest,
       BackgroundIsKnownToBeOpaqueWithLayerAndCollapsedBorder) {
  setBodyInnerHTML(
      "<table style='border-collapse: collapse'>"
      "  <thead id='section' style='will-change: transform;"
      "       background-color: blue'>"
      "    <tr><td>Cell</td></tr>"
      "  </thead>"
      "</table>");

  LayoutTableSection* section =
      toLayoutTableSection(getLayoutObjectByElementId("section"));
  EXPECT_TRUE(section);
  EXPECT_FALSE(
      section->backgroundIsKnownToBeOpaqueInRect(LayoutRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableSectionTest, BackgroundIsKnownToBeOpaqueWithBorderSpacing) {
  setBodyInnerHTML(
      "<table style='border-spacing: 10px'>"
      "  <thead id='section' style='background-color: blue'>"
      "    <tr><td>Cell</td></tr>"
      "  </thead>"
      "</table>");

  LayoutTableSection* section =
      toLayoutTableSection(getLayoutObjectByElementId("section"));
  EXPECT_TRUE(section);
  EXPECT_FALSE(
      section->backgroundIsKnownToBeOpaqueInRect(LayoutRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableSectionTest, BackgroundIsKnownToBeOpaqueWithEmptyCell) {
  setBodyInnerHTML(
      "<table style='border-spacing: 10px'>"
      "  <thead id='section' style='background-color: blue'>"
      "    <tr><td>Cell</td></tr>"
      "    <tr><td>Cell</td><td>Cell</td></tr>"
      "  </thead>"
      "</table>");

  LayoutTableSection* section =
      toLayoutTableSection(getLayoutObjectByElementId("section"));
  EXPECT_TRUE(section);
  EXPECT_FALSE(
      section->backgroundIsKnownToBeOpaqueInRect(LayoutRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableSectionTest, EmptySectionDirtiedRows) {
  setBodyInnerHTML(
      "<table style='border: 100px solid red'>"
      "  <thead id='section'></thead>"
      "</table>");

  LayoutTableSection* section =
      toLayoutTableSection(getLayoutObjectByElementId("section"));
  EXPECT_TRUE(section);
  CellSpan cellSpan = section->dirtiedRows(LayoutRect(50, 50, 100, 100));
  EXPECT_EQ(0u, cellSpan.start());
  EXPECT_EQ(0u, cellSpan.end());
}

TEST_F(LayoutTableSectionTest, EmptySectionDirtiedEffectiveColumns) {
  setBodyInnerHTML(
      "<table style='border: 100px solid red'>"
      "  <thead id='section'></thead>"
      "</table>");

  LayoutTableSection* section =
      toLayoutTableSection(getLayoutObjectByElementId("section"));
  EXPECT_TRUE(section);
  CellSpan cellSpan =
      section->dirtiedEffectiveColumns(LayoutRect(50, 50, 100, 100));
  // The table has at least 1 column even if there is no cell.
  EXPECT_EQ(1u, section->table()->numEffectiveColumns());
  EXPECT_EQ(1u, cellSpan.start());
  EXPECT_EQ(1u, cellSpan.end());
}

TEST_F(LayoutTableSectionTest, PrimaryCellAtAndOriginatingCellAt) {
  setBodyInnerHTML(
      "<table>"
      "  <tbody id='section'>"
      "    <tr>"
      "      <td id='cell00'></td>"
      "      <td id='cell01' rowspan='2'></td>"
      "    </tr>"
      "    <tr>"
      "      <td id='cell10' colspan='2'></td>"
      "    </tr>"
      "  </tbody>"
      "</table>");

  // x,yO: A cell originates from this grid slot.
  // x,yS: A cell originating from x,y spans into this slot.
  //       0         1
  // 0   0,0(O)    0,1(O)
  // 1   1,0(O)    1,0/0,1(S)
  auto* section = toLayoutTableSection(getLayoutObjectByElementId("section"));
  auto* cell00 = getLayoutObjectByElementId("cell00");
  auto* cell01 = getLayoutObjectByElementId("cell01");
  auto* cell10 = getLayoutObjectByElementId("cell10");

  EXPECT_EQ(cell00, section->primaryCellAt(0, 0));
  EXPECT_EQ(cell01, section->primaryCellAt(0, 1));
  EXPECT_EQ(cell10, section->primaryCellAt(1, 0));
  EXPECT_EQ(cell10, section->primaryCellAt(1, 1));
  EXPECT_EQ(cell00, section->originatingCellAt(0, 0));
  EXPECT_EQ(cell01, section->originatingCellAt(0, 1));
  EXPECT_EQ(cell10, section->originatingCellAt(1, 0));
  EXPECT_EQ(nullptr, section->originatingCellAt(1, 1));
}

TEST_F(LayoutTableSectionTest, DirtiedRowsAndDirtiedEffectiveColumnsWithSpans) {
  setBodyInnerHTML(
      "<style>"
      "  td { width: 100px; height: 100px; padding: 0 }"
      "  table { border-spacing: 0 }"
      "</style>"
      "<table>"
      "  <tbody id='section'>"
      "    <tr>"
      "      <td></td>"
      "      <td rowspan='2'></td>"
      "      <td rowspan='2'></td>"
      "    </tr>"
      "    <tr>"
      "      <td colspan='2'></td>"
      "    </tr>"
      "    <tr>"
      "      <td colspan='3'></td>"
      "    </tr>"
      "  </tbody>"
      "</table>");

  // x,yO: A cell originates from this grid slot.
  // x,yS: A cell originating from x,y spans into this slot.
  //      0          1           2
  // 0  0,0(O)    0,1(O)      0,2(O)
  // 1  1,0(O)    1,0/0,1(S)  0,2(S)
  // 2  2,0(O)    2,0(S)      2,0(S)
  auto* section = toLayoutTableSection(getLayoutObjectByElementId("section"));

  // Cell 0,0 only.
  auto span = section->dirtiedRows(LayoutRect(5, 5, 90, 90));
  EXPECT_EQ(0u, span.start());
  EXPECT_EQ(1u, span.end());
  span = section->dirtiedEffectiveColumns(LayoutRect(5, 5, 90, 90));
  EXPECT_EQ(0u, span.start());
  EXPECT_EQ(1u, span.end());

  // Rect intersects the first row and all originating primary cells.
  span = section->dirtiedRows(LayoutRect(5, 5, 290, 90));
  EXPECT_EQ(0u, span.start());
  EXPECT_EQ(1u, span.end());
  span = section->dirtiedEffectiveColumns(LayoutRect(5, 5, 290, 90));
  EXPECT_EQ(0u, span.start());
  EXPECT_EQ(3u, span.end());

  // Rect intersects (1,2). Dirtied rows also cover the first row to cover the
  // primary cell's originating slot.
  span = section->dirtiedRows(LayoutRect(205, 105, 90, 90));
  EXPECT_EQ(0u, span.start());
  EXPECT_EQ(2u, span.end());
  span = section->dirtiedEffectiveColumns(LayoutRect(205, 105, 90, 90));
  EXPECT_EQ(2u, span.start());
  EXPECT_EQ(3u, span.end());

  // Rect intersects (1,1) which has multiple levels of cells (originating from
  // (1,0) and (0,1), in which (1,0) is the primary cell).
  // Dirtied columns also cover the first column to cover the primary cell's
  // originating grid slot.
  span = section->dirtiedRows(LayoutRect(105, 105, 90, 90));
  EXPECT_EQ(1u, span.start());
  EXPECT_EQ(2u, span.end());
  span = section->dirtiedEffectiveColumns(LayoutRect(105, 105, 90, 90));
  EXPECT_EQ(0u, span.start());
  EXPECT_EQ(2u, span.end());

  // Rect intersects (1,1) and (1,2). Dirtied rows also cover the first row.
  // Dirtied columns also cover the first column.
  span = section->dirtiedRows(LayoutRect(105, 105, 190, 90));
  EXPECT_EQ(0u, span.start());
  EXPECT_EQ(2u, span.end());
  span = section->dirtiedEffectiveColumns(LayoutRect(105, 105, 190, 90));
  EXPECT_EQ(0u, span.start());
  EXPECT_EQ(3u, span.end());

  // Rect intersects (1,2) and (2,2). Dirtied rows and dirtied columns cover all
  // rows and columns.
  span = section->dirtiedRows(LayoutRect(205, 105, 90, 190));
  EXPECT_EQ(0u, span.start());
  EXPECT_EQ(3u, span.end());
  span = section->dirtiedEffectiveColumns(LayoutRect(205, 105, 90, 190));
  EXPECT_EQ(0u, span.start());
  EXPECT_EQ(3u, span.end());
}

}  // anonymous namespace

}  // namespace blink
