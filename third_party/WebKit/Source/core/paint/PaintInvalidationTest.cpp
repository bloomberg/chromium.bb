// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/FrameView.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class PaintInvalidationTest : public RenderingTest {
 public:
  PaintInvalidationTest()
      : RenderingTest(SingleChildLocalFrameClient::create()) {}
};

// Changing style in a way that changes overflow without layout should cause
// the layout view to possibly need a paint invalidation since we may have
// revealed additional background that can be scrolled into view.
TEST_F(PaintInvalidationTest, RecalcOverflowInvalidatesBackground) {
  document().page()->settings().setViewportEnabled(true);
  setBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style type='text/css'>"
      "  body, html {"
      "    width: 100%;"
      "    height: 100%;"
      "    margin: 0px;"
      "  }"
      "  #container {"
      "    width: 100%;"
      "    height: 100%;"
      "  }"
      "</style>"
      "<div id='container'></div>");

  document().view()->updateAllLifecyclePhases();

  ScrollableArea* scrollableArea = document().view();
  ASSERT_EQ(scrollableArea->maximumScrollOffset().height(), 0);
  EXPECT_FALSE(document().layoutView()->mayNeedPaintInvalidation());

  Element* container = document().getElementById("container");
  container->setAttribute(HTMLNames::styleAttr,
                          "transform: translateY(1000px);");
  document().updateStyleAndLayoutTree();

  EXPECT_EQ(scrollableArea->maximumScrollOffset().height(), 1000);
  EXPECT_TRUE(document().layoutView()->mayNeedPaintInvalidation());
}

}  // namespace

}  // namespace blink
