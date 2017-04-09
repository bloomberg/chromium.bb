/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/LayoutTableCell.h"

#include "core/layout/LayoutTestHelper.h"

namespace blink {

namespace {

class LayoutTableCellDeathTest : public RenderingTest {
 protected:
  virtual void SetUp() {
    RenderingTest::SetUp();
    cell_ = LayoutTableCell::CreateAnonymous(&GetDocument());
  }

  virtual void TearDown() {
    cell_->Destroy();
    RenderingTest::TearDown();
  }

  LayoutTableCell* cell_;
};

TEST_F(LayoutTableCellDeathTest, CanSetColumn) {
  static const unsigned kColumnIndex = 10;
  cell_->SetAbsoluteColumnIndex(kColumnIndex);
  EXPECT_EQ(kColumnIndex, cell_->AbsoluteColumnIndex());
}

TEST_F(LayoutTableCellDeathTest, CanSetColumnToMaxColumnIndex) {
  cell_->SetAbsoluteColumnIndex(kMaxColumnIndex);
  EXPECT_EQ(kMaxColumnIndex, cell_->AbsoluteColumnIndex());
}

// FIXME: Re-enable these tests once ASSERT_DEATH is supported for Android.
// See: https://bugs.webkit.org/show_bug.cgi?id=74089
// TODO(dgrogan): These tests started flaking on Mac try bots around 2016-07-28.
// https://crbug.com/632816
#if !OS(ANDROID) && !OS(MACOSX)

TEST_F(LayoutTableCellDeathTest, CrashIfColumnOverflowOnSetting) {
  ASSERT_DEATH(cell_->SetAbsoluteColumnIndex(kMaxColumnIndex + 1), "");
}

TEST_F(LayoutTableCellDeathTest, CrashIfSettingUnsetColumnIndex) {
  ASSERT_DEATH(cell_->SetAbsoluteColumnIndex(kUnsetColumnIndex), "");
}

#endif

using LayoutTableCellTest = RenderingTest;

TEST_F(LayoutTableCellTest, ResetColspanIfTooBig) {
  SetBodyInnerHTML("<table><td colspan='14000'></td></table>");

  LayoutTableCell* cell = ToLayoutTableCell(GetDocument()
                                                .body()
                                                ->FirstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->GetLayoutObject());
  ASSERT_EQ(cell->ColSpan(), 8190U);
}

TEST_F(LayoutTableCellTest, DoNotResetColspanJustBelowBoundary) {
  SetBodyInnerHTML("<table><td colspan='8190'></td></table>");

  LayoutTableCell* cell = ToLayoutTableCell(GetDocument()
                                                .body()
                                                ->FirstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->GetLayoutObject());
  ASSERT_EQ(cell->ColSpan(), 8190U);
}

TEST_F(LayoutTableCellTest, ResetRowspanIfTooBig) {
  SetBodyInnerHTML("<table><td rowspan='70000'></td></table>");

  LayoutTableCell* cell = ToLayoutTableCell(GetDocument()
                                                .body()
                                                ->FirstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->GetLayoutObject());
  ASSERT_EQ(cell->RowSpan(), 65534U);
}

TEST_F(LayoutTableCellTest, DoNotResetRowspanJustBelowBoundary) {
  SetBodyInnerHTML("<table><td rowspan='65534'></td></table>");

  LayoutTableCell* cell = ToLayoutTableCell(GetDocument()
                                                .body()
                                                ->FirstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->GetLayoutObject());
  ASSERT_EQ(cell->RowSpan(), 65534U);
}

TEST_F(LayoutTableCellTest,
       BackgroundIsKnownToBeOpaqueWithLayerAndCollapsedBorder) {
  SetBodyInnerHTML(
      "<table style='border-collapse: collapse'>"
      "<td style='will-change: transform; background-color: blue'>Cell></td>"
      "</table>");

  LayoutTableCell* cell = ToLayoutTableCell(GetDocument()
                                                .body()
                                                ->FirstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->GetLayoutObject());
  EXPECT_FALSE(cell->BackgroundIsKnownToBeOpaqueInRect(LayoutRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableCellTest, RepaintContentInTableCell) {
  const char* body_content =
      "<table id='table' style='position: absolute; left: 1px;'>"
      "<tr>"
      "<td id='cell'>"
      "<div style='display: inline-block; height: 20px; width: 20px'>"
      "</td>"
      "</tr>"
      "</table>";
  SetBodyInnerHTML(body_content);

  // Create an overflow recalc.
  Element* cell = GetDocument().GetElementById(AtomicString("cell"));
  cell->setAttribute(HTMLNames::styleAttr, "outline: 1px solid black;");
  // Trigger a layout on the table that doesn't require cell layout.
  Element* table = GetDocument().GetElementById(AtomicString("table"));
  table->setAttribute(HTMLNames::styleAttr, "position: absolute; left: 2px;");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Check that overflow was calculated on the cell.
  LayoutBlock* input_block = ToLayoutBlock(GetLayoutObjectByElementId("cell"));
  LayoutRect rect = input_block->LocalVisualRect();
  EXPECT_EQ(LayoutRect(-1, -1, 24, 24), rect);
}
}  // namespace

}  // namespace blink
