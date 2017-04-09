/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "core/layout/LayoutTableRow.h"

#include "core/layout/LayoutTestHelper.h"

namespace blink {

namespace {

class LayoutTableRowDeathTest : public RenderingTest {
 protected:
  virtual void SetUp() {
    RenderingTest::SetUp();
    row_ = LayoutTableRow::CreateAnonymous(&GetDocument());
  }

  virtual void TearDown() { row_->Destroy(); }

  LayoutTableRow* row_;
};

TEST_F(LayoutTableRowDeathTest, CanSetRow) {
  static const unsigned kRowIndex = 10;
  row_->SetRowIndex(kRowIndex);
  EXPECT_EQ(kRowIndex, row_->RowIndex());
}

TEST_F(LayoutTableRowDeathTest, CanSetRowToMaxRowIndex) {
  row_->SetRowIndex(kMaxRowIndex);
  EXPECT_EQ(kMaxRowIndex, row_->RowIndex());
}

// FIXME: Re-enable these tests once ASSERT_DEATH is supported for Android.
// See: https://bugs.webkit.org/show_bug.cgi?id=74089
// TODO(dgrogan): These tests started flaking on Mac try bots around 2016-07-28.
// https://crbug.com/632816
#if !OS(ANDROID) && !OS(MACOSX)

TEST_F(LayoutTableRowDeathTest, CrashIfRowOverflowOnSetting) {
  ASSERT_DEATH(row_->SetRowIndex(kMaxRowIndex + 1), "");
}

TEST_F(LayoutTableRowDeathTest, CrashIfSettingUnsetRowIndex) {
  ASSERT_DEATH(row_->SetRowIndex(kUnsetRowIndex), "");
}

#endif

using LayoutTableRowTest = RenderingTest;

TEST_F(LayoutTableRowTest,
       BackgroundIsKnownToBeOpaqueWithLayerAndCollapsedBorder) {
  SetBodyInnerHTML(
      "<table style='border-collapse: collapse'>"
      "<tr style='will-change: transform; background-color: "
      "blue'><td>Cell</td></tr>"
      "</table>");

  LayoutTableRow* row = ToLayoutTableRow(GetDocument()
                                             .body()
                                             ->FirstChild()
                                             ->firstChild()
                                             ->firstChild()
                                             ->GetLayoutObject());
  EXPECT_FALSE(row->BackgroundIsKnownToBeOpaqueInRect(LayoutRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableRowTest, BackgroundIsKnownToBeOpaqueWithBorderSpacing) {
  SetBodyInnerHTML(
      "<table style='border-spacing: 10px'>"
      "<tr style='background-color: blue'><td>Cell</td></tr>"
      "</table>");

  LayoutTableRow* row = ToLayoutTableRow(GetDocument()
                                             .body()
                                             ->FirstChild()
                                             ->firstChild()
                                             ->firstChild()
                                             ->GetLayoutObject());
  EXPECT_FALSE(row->BackgroundIsKnownToBeOpaqueInRect(LayoutRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableRowTest, BackgroundIsKnownToBeOpaqueWithEmptyCell) {
  SetBodyInnerHTML(
      "<table style='border-spacing: 10px'>"
      "<tr style='background-color: blue'><td>Cell</td></tr>"
      "<tr style='background-color: blue'><td>Cell</td><td>Cell</td></tr>"
      "</table>");

  LayoutTableRow* row = ToLayoutTableRow(GetDocument()
                                             .body()
                                             ->FirstChild()
                                             ->firstChild()
                                             ->firstChild()
                                             ->GetLayoutObject());
  EXPECT_FALSE(row->BackgroundIsKnownToBeOpaqueInRect(LayoutRect(0, 0, 1, 1)));
}

}  // anonymous namespace

}  // namespace blink
