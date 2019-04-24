// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using test::RunPendingTasks;

class TextFragmentAnchorTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  }

  void RunAsyncMatchingTasks() {
    auto* scheduler =
        ThreadScheduler::Current()->GetWebMainThreadSchedulerForTest();
    blink::scheduler::RunIdleTasksForTesting(scheduler,
                                             base::BindOnce([]() {}));
    RunPendingTasks();
  }

  ScrollableArea* LayoutViewport() {
    return GetDocument().View()->LayoutViewport();
  }

  IntRect ViewportRect() {
    return IntRect(IntPoint(), LayoutViewport()->VisibleContentRect().Size());
  }

  IntRect BoundingRectInFrame(Node& node) {
    return node.GetLayoutObject()->AbsoluteBoundingBoxRect();
  }
};

// Basic test case, ensure we scroll the matching text into view.
TEST_F(TextFragmentAnchorTest, BasicSmokeTest) {
  SimRequest request("https://example.com/test.html#targetText=test",
                     "text/html");
  LoadURL("https://example.com/test.html#targetText=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& p = *GetDocument().getElementById("text");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)))
      << "<p> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

// Make sure the fragment is uninstalled once the match has been completed.
// This may require a layout since the anchor is uninstalled at invocation
// time.
TEST_F(TextFragmentAnchorTest, RemoveAnchorAfterMatch) {
  SimRequest request("https://example.com/test.html#targetText=test",
                     "text/html");
  LoadURL("https://example.com/test.html#targetText=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  // Force a layout
  GetDocument().body()->setAttribute(html_names::kStyleAttr, "height: 1300px");
  Compositor().BeginFrame();

  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
}

// Make sure a non-matching string doesn't cause scroll and the fragment is
// removed when completed.
TEST_F(TextFragmentAnchorTest, NonMatchingString) {
  SimRequest request("https://example.com/test.html#targetText=unicorn",
                     "text/html");
  LoadURL("https://example.com/test.html#targetText=unicorn");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  EXPECT_EQ(ScrollOffset(), LayoutViewport()->GetScrollOffset());

  // Force a layout
  GetDocument().body()->setAttribute(html_names::kStyleAttr, "height: 1300px");
  Compositor().BeginFrame();

  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
}

// If the targetText=... string matches an id, we should scroll using id
// fragment semantics rather than doing a textual match.
TEST_F(TextFragmentAnchorTest, IdFragmentTakesPrecedence) {
  SimRequest request("https://example.com/test.html#targetText=test",
                     "text/html");
  LoadURL("https://example.com/test.html#targetText=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
      div {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id="text">This is a test page</p>
    <div id="targetText=test"></div>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& div = *GetDocument().getElementById("targetText=test");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(div)))
      << "Should have scrolled <div> into view but didn't, scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

// Ensure multiple matches will scroll the first into view.
TEST_F(TextFragmentAnchorTest, MultipleMatches) {
  SimRequest request("https://example.com/test.html#targetText=test",
                     "text/html");
  LoadURL("https://example.com/test.html#targetText=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      #first {
        position: absolute;
        top: 1000px;
      }
      #second {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id="first">This is a test page</p>
    <p id="second">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& first = *GetDocument().getElementById("first");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(first)))
      << "First <p> wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  // Ensure we only report one marker.
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Ensure matching works inside nested blocks.
TEST_F(TextFragmentAnchorTest, NestedBlocks) {
  SimRequest request("https://example.com/test.html#targetText=test",
                     "text/html");
  LoadURL("https://example.com/test.html#targetText=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #spacer {
        height: 1000px;
      }
    </style>
    <body>
      <div id="spacer">
        Some non-matching text
      </div>
      <div>
        <p id="match">This is a test page</p>
      </div>
    </body>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& match = *GetDocument().getElementById("match");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(match)))
      << "<p> wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

}  // namespace

}  // namespace blink
