// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/line/InlineBox.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutTestHelper.h"

namespace blink {

using InlineBoxTest = RenderingTest;

TEST_F(InlineBoxTest, LogicalRectToPhysicalRectNormal) {
  SetBodyInnerHTML(
      "<div id='div' style='width: 80px; height: 50px'>Test</div>");
  LayoutBlockFlow* div = ToLayoutBlockFlow(GetLayoutObjectByElementId("div"));
  InlineBox* inline_box = div->FirstLineBox();
  LayoutRect rect(11, 22, 33, 44);
  inline_box->LogicalRectToPhysicalRect(rect);
  EXPECT_EQ(LayoutRect(11, 22, 33, 44), rect);
}

TEST_F(InlineBoxTest, LogicalRectToPhysicalRectVerticalRL) {
  SetBodyInnerHTML(
      "<div id='div' "
      "style='writing-mode:vertical-rl; width: 80px; height: 50px'>Test</div>");
  LayoutBlockFlow* div = ToLayoutBlockFlow(GetLayoutObjectByElementId("div"));
  InlineBox* inline_box = div->FirstLineBox();
  LayoutRect rect(11, 22, 33, 44);
  inline_box->LogicalRectToPhysicalRect(rect);
  EXPECT_EQ(LayoutRect(14, 11, 44, 33), rect);
}

TEST_F(InlineBoxTest, LogicalRectToPhysicalRectVerticalLR) {
  SetBodyInnerHTML(
      "<div id='div' "
      "style='writing-mode:vertical-lr; width: 80px; height: 50px'>Test</div>");
  LayoutBlockFlow* div = ToLayoutBlockFlow(GetLayoutObjectByElementId("div"));
  InlineBox* inline_box = div->FirstLineBox();
  LayoutRect rect(11, 22, 33, 44);
  inline_box->LogicalRectToPhysicalRect(rect);
  EXPECT_EQ(LayoutRect(22, 11, 44, 33), rect);
}

}  // namespace blink
