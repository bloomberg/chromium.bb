// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class RotationViewportAnchorTest : public ::testing::WithParamInterface<bool>,
                                   private ScopedRootLayerScrollingForTest,
                                   public SimTest {
 public:
  RotationViewportAnchorTest() : ScopedRootLayerScrollingForTest(GetParam()) {}

  void SetUp() override {
    SimTest::SetUp();
    WebView().GetSettings()->SetViewportEnabled(true);
    WebView().GetSettings()->SetMainFrameResizesAreOrientationChanges(true);
  }
};

INSTANTIATE_TEST_CASE_P(All, RotationViewportAnchorTest, ::testing::Bool());

TEST_P(RotationViewportAnchorTest, SimpleAbsolutePosition) {
  WebView().Resize(WebSize(400, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        body {
          width: 10000px;
          height: 10000px;
          margin: 0px;
        }

        #target {
          width: 100px;
          height: 100px;
          position: absolute;
          left: 3000px;
          top: 4000px;
        }
      </style>
      <div id="target"></div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  ScrollableArea* layout_viewport =
      document.View()->LayoutViewportScrollableArea();

  // Place the target at the top-center of the viewport. This is where the
  // rotation anchor finds the node to anchor to.
  layout_viewport->SetScrollOffset(ScrollOffset(3050 - 200, 4050),
                                   kProgrammaticScroll);

  WebView().Resize(WebSize(600, 400));
  Compositor().BeginFrame();

  EXPECT_EQ(3050 - 200, layout_viewport->GetScrollOffset().Width());
  EXPECT_EQ(4050, layout_viewport->GetScrollOffset().Height());
}

TEST_P(RotationViewportAnchorTest, PositionRelativeToViewportSize) {
  WebView().Resize(WebSize(100, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        body {
          width: 10000px;
          height: 10000px;
          margin: 0px;
        }

        #target {
          width: 50px;
          height: 50px;
          position: absolute;
          left: 500%;
          top: 500%;
        }
      </style>
      <div id="target"></div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  ScrollableArea* layout_viewport =
      document.View()->LayoutViewportScrollableArea();

  IntPoint target_position(5 * WebView().Size().width,
                           5 * WebView().Size().height);

  // Place the target at the top-center of the viewport. This is where the
  // rotation anchor finds the node to anchor to.
  layout_viewport->SetScrollOffset(
      ScrollOffset(target_position.X() - WebView().Size().width / 2 + 25,
                   target_position.Y()),
      kProgrammaticScroll);

  WebView().Resize(WebSize(600, 100));
  Compositor().BeginFrame();

  target_position =
      IntPoint(5 * WebView().Size().width, 5 * WebView().Size().height);

  IntPoint expected_offset(
      target_position.X() - WebView().Size().width / 2 + 25,
      target_position.Y());

  EXPECT_EQ(expected_offset.X(), layout_viewport->GetScrollOffset().Width());
  EXPECT_EQ(expected_offset.Y(), layout_viewport->GetScrollOffset().Height());
}

}  // namespace

}  // namespace blink
