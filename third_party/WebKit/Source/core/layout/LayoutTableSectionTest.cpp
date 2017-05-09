// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTableSection.h"

#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTestHelper.h"

namespace blink {

namespace {

class LayoutTableSectionTest : public RenderingTest {
 protected:
  LayoutTableSection* GetSectionByElementId(const char* id) {
    return ToLayoutTableSection(GetLayoutObjectByElementId(id));
  }
};

TEST_F(LayoutTableSectionTest,
       BackgroundIsKnownToBeOpaqueWithLayerAndCollapsedBorder) {
  SetBodyInnerHTML(
      "<table style='border-collapse: collapse'>"
      "  <thead id='section' style='will-change: transform;"
      "       background-color: blue'>"
      "    <tr><td>Cell</td></tr>"
      "  </thead>"
      "</table>");

  auto* section = GetSectionByElementId("section");
  EXPECT_TRUE(section);
  EXPECT_FALSE(
      section->BackgroundIsKnownToBeOpaqueInRect(LayoutRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableSectionTest, BackgroundIsKnownToBeOpaqueWithBorderSpacing) {
  SetBodyInnerHTML(
      "<table style='border-spacing: 10px'>"
      "  <thead id='section' style='background-color: blue'>"
      "    <tr><td>Cell</td></tr>"
      "  </thead>"
      "</table>");

  auto* section = GetSectionByElementId("section");
  EXPECT_TRUE(section);
  EXPECT_FALSE(
      section->BackgroundIsKnownToBeOpaqueInRect(LayoutRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableSectionTest, BackgroundIsKnownToBeOpaqueWithEmptyCell) {
  SetBodyInnerHTML(
      "<table style='border-spacing: 10px'>"
      "  <thead id='section' style='background-color: blue'>"
      "    <tr><td>Cell</td></tr>"
      "    <tr><td>Cell</td><td>Cell</td></tr>"
      "  </thead>"
      "</table>");

  auto* section = GetSectionByElementId("section");
  EXPECT_TRUE(section);
  EXPECT_FALSE(
      section->BackgroundIsKnownToBeOpaqueInRect(LayoutRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableSectionTest, EmptySectionDirtiedRows) {
  SetBodyInnerHTML(
      "<table style='border: 100px solid red'>"
      "  <thead id='section'></thead>"
      "</table>");

  auto* section = GetSectionByElementId("section");
  EXPECT_TRUE(section);
  CellSpan cell_span = section->DirtiedRows(LayoutRect(50, 50, 100, 100));
  EXPECT_EQ(0u, cell_span.Start());
  EXPECT_EQ(0u, cell_span.end());
}

TEST_F(LayoutTableSectionTest, EmptySectionDirtiedEffectiveColumns) {
  SetBodyInnerHTML(
      "<table style='border: 100px solid red'>"
      "  <thead id='section'></thead>"
      "</table>");

  auto* section = GetSectionByElementId("section");
  EXPECT_TRUE(section);
  CellSpan cell_span =
      section->DirtiedEffectiveColumns(LayoutRect(50, 50, 100, 100));
  // The table has at least 1 column even if there is no cell.
  EXPECT_EQ(1u, section->Table()->NumEffectiveColumns());
  EXPECT_EQ(1u, cell_span.Start());
  EXPECT_EQ(1u, cell_span.end());
}

TEST_F(LayoutTableSectionTest, PrimaryCellAtAndOriginatingCellAt) {
  SetBodyInnerHTML(
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
  auto* section = GetSectionByElementId("section");
  auto* cell00 = GetLayoutObjectByElementId("cell00");
  auto* cell01 = GetLayoutObjectByElementId("cell01");
  auto* cell10 = GetLayoutObjectByElementId("cell10");

  EXPECT_EQ(cell00, section->PrimaryCellAt(0, 0));
  EXPECT_EQ(cell01, section->PrimaryCellAt(0, 1));
  EXPECT_EQ(cell10, section->PrimaryCellAt(1, 0));
  EXPECT_EQ(cell10, section->PrimaryCellAt(1, 1));
  EXPECT_EQ(cell00, section->OriginatingCellAt(0, 0));
  EXPECT_EQ(cell01, section->OriginatingCellAt(0, 1));
  EXPECT_EQ(cell10, section->OriginatingCellAt(1, 0));
  EXPECT_EQ(nullptr, section->OriginatingCellAt(1, 1));
}

TEST_F(LayoutTableSectionTest, DirtiedRowsAndDirtiedEffectiveColumnsWithSpans) {
  SetBodyInnerHTML(
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
  auto* section = GetSectionByElementId("section");

  // Cell 0,0 only.
  auto span = section->DirtiedRows(LayoutRect(5, 5, 90, 90));
  EXPECT_EQ(0u, span.Start());
  EXPECT_EQ(1u, span.end());
  span = section->DirtiedEffectiveColumns(LayoutRect(5, 5, 90, 90));
  EXPECT_EQ(0u, span.Start());
  EXPECT_EQ(1u, span.end());

  // Rect intersects the first row and all originating primary cells.
  span = section->DirtiedRows(LayoutRect(5, 5, 290, 90));
  EXPECT_EQ(0u, span.Start());
  EXPECT_EQ(1u, span.end());
  span = section->DirtiedEffectiveColumns(LayoutRect(5, 5, 290, 90));
  EXPECT_EQ(0u, span.Start());
  EXPECT_EQ(3u, span.end());

  // Rect intersects (1,2). Dirtied rows also cover the first row to cover the
  // primary cell's originating slot.
  span = section->DirtiedRows(LayoutRect(205, 105, 90, 90));
  EXPECT_EQ(0u, span.Start());
  EXPECT_EQ(2u, span.end());
  span = section->DirtiedEffectiveColumns(LayoutRect(205, 105, 90, 90));
  EXPECT_EQ(2u, span.Start());
  EXPECT_EQ(3u, span.end());

  // Rect intersects (1,1) which has multiple levels of cells (originating from
  // (1,0) and (0,1), in which (1,0) is the primary cell).
  // Dirtied columns also cover the first column to cover the primary cell's
  // originating grid slot.
  span = section->DirtiedRows(LayoutRect(105, 105, 90, 90));
  EXPECT_EQ(1u, span.Start());
  EXPECT_EQ(2u, span.end());
  span = section->DirtiedEffectiveColumns(LayoutRect(105, 105, 90, 90));
  EXPECT_EQ(0u, span.Start());
  EXPECT_EQ(2u, span.end());

  // Rect intersects (1,1) and (1,2). Dirtied rows also cover the first row.
  // Dirtied columns also cover the first column.
  span = section->DirtiedRows(LayoutRect(105, 105, 190, 90));
  EXPECT_EQ(0u, span.Start());
  EXPECT_EQ(2u, span.end());
  span = section->DirtiedEffectiveColumns(LayoutRect(105, 105, 190, 90));
  EXPECT_EQ(0u, span.Start());
  EXPECT_EQ(3u, span.end());

  // Rect intersects (1,2) and (2,2). Dirtied rows and dirtied columns cover all
  // rows and columns.
  span = section->DirtiedRows(LayoutRect(205, 105, 90, 190));
  EXPECT_EQ(0u, span.Start());
  EXPECT_EQ(3u, span.end());
  span = section->DirtiedEffectiveColumns(LayoutRect(205, 105, 90, 190));
  EXPECT_EQ(0u, span.Start());
  EXPECT_EQ(3u, span.end());
}

TEST_F(LayoutTableSectionTest, VisualOverflowWithCollapsedBorders) {
  SetBodyInnerHTML(
      "<style>"
      "  table { border-collapse: collapse }"
      "  td { border: 0px solid blue; padding: 0 }"
      "  div { width: 100px; height: 100px }"
      "</style>"
      "<table>"
      "  <tbody id='section'>"
      "    <tr>"
      "      <td style='border-bottom-width: 10px;"
      "          outline: 3px solid blue'><div></div></td>"
      "      <td style='border-width: 3px 15px'><div></div></td>"
      "    </tr>"
      "    <tr style='outline: 8px solid green'><td><div></div></td></tr>"
      "  </tbody>"
      "</table>");

  auto* section = GetSectionByElementId("section");

  // The section's self visual overflow covers the collapsed borders.
  LayoutRect expected_self_visual_overflow = section->BorderBoxRect();
  expected_self_visual_overflow.ExpandEdges(LayoutUnit(1), LayoutUnit(8),
                                            LayoutUnit(0), LayoutUnit(0));
  EXPECT_EQ(expected_self_visual_overflow, section->SelfVisualOverflowRect());

  // The section's visual overflow covers self visual overflow and visual
  // overflows rows.
  LayoutRect expected_visual_overflow = section->BorderBoxRect();
  expected_visual_overflow.ExpandEdges(LayoutUnit(3), LayoutUnit(8),
                                       LayoutUnit(8), LayoutUnit(8));
  EXPECT_EQ(expected_visual_overflow, section->VisualOverflowRect());
}

}  // anonymous namespace

}  // namespace blink
