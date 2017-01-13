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
  setBodyInnerHTML(
      "<span id='splitInline'><i id='before'></i><h1 id='blockChild'></h1><i "
      "id='after'></i></span>");

  LayoutInline* splitInlinePart1 =
      toLayoutInline(getLayoutObjectByElementId("splitInline"));
  ASSERT_TRUE(splitInlinePart1);
  ASSERT_TRUE(splitInlinePart1->firstChild());
  EXPECT_EQ(splitInlinePart1->firstChild(),
            getLayoutObjectByElementId("before"));
  EXPECT_FALSE(splitInlinePart1->firstChild()->nextSibling());

  LayoutBlockFlow* block = toLayoutBlockFlow(splitInlinePart1->continuation());
  ASSERT_TRUE(block);
  ASSERT_TRUE(block->firstChild());
  EXPECT_EQ(block->firstChild(), getLayoutObjectByElementId("blockChild"));
  EXPECT_FALSE(block->firstChild()->nextSibling());

  LayoutInline* splitInlinePart2 = toLayoutInline(block->continuation());
  ASSERT_TRUE(splitInlinePart2);
  ASSERT_TRUE(splitInlinePart2->firstChild());
  EXPECT_EQ(splitInlinePart2->firstChild(),
            getLayoutObjectByElementId("after"));
  EXPECT_FALSE(splitInlinePart2->firstChild()->nextSibling());
  EXPECT_FALSE(splitInlinePart2->continuation());
}

TEST_F(LayoutInlineTest, RegionHitTest) {
  setBodyInnerHTML(
      "<div><span id='lotsOfBoxes'>"
      "This is a test line<br>This is a test line<br>This is a test line<br>"
      "This is a test line<br>This is a test line<br>This is a test line<br>"
      "This is a test line<br>This is a test line<br>This is a test line<br>"
      "This is a test line<br>This is a test line<br>This is a test line<br>"
      "This is a test line<br>This is a test line<br>This is a test line<br>"
      "This is a test line<br>This is a test line<br>This is a test line<br>"
      "</span></div>");

  document().view()->updateAllLifecyclePhases();

  LayoutInline* lotsOfBoxes =
      toLayoutInline(getLayoutObjectByElementId("lotsOfBoxes"));
  ASSERT_TRUE(lotsOfBoxes);

  HitTestRequest hitRequest(HitTestRequest::TouchEvent |
                            HitTestRequest::ListBased);
  LayoutPoint hitLocation(2, 5);
  HitTestResult hitResult(hitRequest, hitLocation, 2, 1, 2, 1);
  LayoutPoint hitOffset;

  bool hitOutcome = lotsOfBoxes->hitTestCulledInline(
      hitResult, hitResult.hitTestLocation(), hitOffset);
  // Assert checks that we both hit something and that the area covered
  // by "something" totally contains the hit region.
  EXPECT_TRUE(hitOutcome);
}

}  // namespace blink
