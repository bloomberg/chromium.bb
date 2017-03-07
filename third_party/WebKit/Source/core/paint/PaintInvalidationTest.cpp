// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/FrameView.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class PaintInvalidationTest : public ::testing::WithParamInterface<bool>,
                              private ScopedRootLayerScrollingForTest,
                              public RenderingTest {
 public:
  PaintInvalidationTest()
      : ScopedRootLayerScrollingForTest(GetParam()),
        RenderingTest(SingleChildLocalFrameClient::create()) {}
};

INSTANTIATE_TEST_CASE_P(All, PaintInvalidationTest, ::testing::Bool());

// Changing style in a way that changes overflow without layout should cause
// the layout view to possibly need a paint invalidation since we may have
// revealed additional background that can be scrolled into view.
TEST_P(PaintInvalidationTest, RecalcOverflowInvalidatesBackground) {
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

TEST_P(PaintInvalidationTest, UpdateVisualRectOnFrameBorderWidthChange) {
  // TODO(wangxianzhu): enable for SPv2.
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  setBodyInnerHTML(
      "<style>"
      "  body { margin: 10px }"
      "  iframe { width: 100px; height: 100px; border: none; }"
      "</style>"
      "<iframe id='iframe'></iframe>");

  Element* iframe = document().getElementById("iframe");
  LayoutView* childLayoutView = childDocument().layoutView();
  EXPECT_EQ(document().layoutView(),
            &childLayoutView->containerForPaintInvalidation());
  EXPECT_EQ(LayoutRect(10, 10, 100, 100), childLayoutView->visualRect());

  iframe->setAttribute(HTMLNames::styleAttr, "border: 20px solid blue");
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(document().layoutView(),
            &childLayoutView->containerForPaintInvalidation());
  EXPECT_EQ(LayoutRect(30, 30, 100, 100), childLayoutView->visualRect());
};

}  // namespace

}  // namespace blink
