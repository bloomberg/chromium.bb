// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutInline.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutInlineTest : public RenderingTest {};

TEST_F(LayoutInlineTest, SimpleContinuation) {
  SetBodyInnerHTML(
      "<span id='splitInline'><i id='before'></i><h1 id='blockChild'></h1><i "
      "id='after'></i></span>");

  LayoutInline* split_inline_part1 =
      ToLayoutInline(GetLayoutObjectByElementId("splitInline"));
  ASSERT_TRUE(split_inline_part1);
  ASSERT_TRUE(split_inline_part1->FirstChild());
  EXPECT_EQ(split_inline_part1->FirstChild(),
            GetLayoutObjectByElementId("before"));
  EXPECT_FALSE(split_inline_part1->FirstChild()->NextSibling());

  LayoutBlockFlow* block =
      ToLayoutBlockFlow(split_inline_part1->Continuation());
  ASSERT_TRUE(block);
  ASSERT_TRUE(block->FirstChild());
  EXPECT_EQ(block->FirstChild(), GetLayoutObjectByElementId("blockChild"));
  EXPECT_FALSE(block->FirstChild()->NextSibling());

  LayoutInline* split_inline_part2 = ToLayoutInline(block->Continuation());
  ASSERT_TRUE(split_inline_part2);
  ASSERT_TRUE(split_inline_part2->FirstChild());
  EXPECT_EQ(split_inline_part2->FirstChild(),
            GetLayoutObjectByElementId("after"));
  EXPECT_FALSE(split_inline_part2->FirstChild()->NextSibling());
  EXPECT_FALSE(split_inline_part2->Continuation());
}

TEST_F(LayoutInlineTest, RegionHitTest) {
  SetBodyInnerHTML(
      "<div><span id='lotsOfBoxes'>"
      "This is a test line<br>This is a test line<br>This is a test line<br>"
      "This is a test line<br>This is a test line<br>This is a test line<br>"
      "This is a test line<br>This is a test line<br>This is a test line<br>"
      "This is a test line<br>This is a test line<br>This is a test line<br>"
      "This is a test line<br>This is a test line<br>This is a test line<br>"
      "This is a test line<br>This is a test line<br>This is a test line<br>"
      "</span></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  LayoutInline* lots_of_boxes =
      ToLayoutInline(GetLayoutObjectByElementId("lotsOfBoxes"));
  ASSERT_TRUE(lots_of_boxes);

  HitTestRequest hit_request(HitTestRequest::kTouchEvent |
                             HitTestRequest::kListBased);
  LayoutPoint hit_location(2, 5);
  HitTestResult hit_result(hit_request, hit_location, 2, 1, 2, 1);
  LayoutPoint hit_offset;

  bool hit_outcome = lots_of_boxes->HitTestCulledInline(
      hit_result, hit_result.GetHitTestLocation(), hit_offset);
  // Assert checks that we both hit something and that the area covered
  // by "something" totally contains the hit region.
  EXPECT_TRUE(hit_outcome);
}

}  // namespace blink
