// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using test::RunPendingTasks;

class TextFragmentAnchorMetricsTest : public SimTest {
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

  HistogramTester histogram_tester_;
};

// Test UMA metrics collection
TEST_F(TextFragmentAnchorMetricsTest, UMAMetricsCollected) {
  SimRequest request(
      "https://example.com/test.html#targetText=test&targetText=cat",
      "text/html");
  LoadURL("https://example.com/test.html#targetText=test&targetText=cat");
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
    <p>This is a test page</p>
    <p>With ambiguous test content</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.SelectorCount", 2,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.MatchRate", 50, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.AmbiguousMatch", 1,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.ScrollCancelled", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.ScrollCancelled", 0,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.DidScrollIntoView", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.DidScrollIntoView",
                                       1, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.TimeToScrollIntoView",
                                     1);
}

// Test UMA metrics collection when there is no match found
TEST_F(TextFragmentAnchorMetricsTest, NoMatchFound) {
  SimRequest request("https://example.com/test.html#targetText=cat",
                     "text/html");
  LoadURL("https://example.com/test.html#targetText=cat");
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
    <p>This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.SelectorCount", 1,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.MatchRate", 0, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.AmbiguousMatch", 0,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.ScrollCancelled", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.ScrollCancelled", 0,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.DidScrollIntoView", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.TimeToScrollIntoView",
                                     0);
}

// Test that we don't collect any metrics when there is no targetText
TEST_F(TextFragmentAnchorMetricsTest, NoTextFragmentAnchor) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.MatchRate", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.ScrollCancelled", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.DidScrollIntoView", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.TimeToScrollIntoView",
                                     0);
}

// Test that the correct metrics are collected when we found a match but didn't
// need to scroll.
TEST_F(TextFragmentAnchorMetricsTest, MatchFoundNoScroll) {
  SimRequest request("https://example.com/test.html#targetText=test",
                     "text/html");
  LoadURL("https://example.com/test.html#targetText=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.SelectorCount", 1,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.MatchRate", 100, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.AmbiguousMatch", 0,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.ScrollCancelled", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.ScrollCancelled", 0,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.DidScrollIntoView", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.DidScrollIntoView",
                                       0, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.TimeToScrollIntoView",
                                     1);
}

// Test that the ScrollCancelled metric gets reported when a user scroll cancels
// the scroll into view.
TEST_F(TextFragmentAnchorMetricsTest, ScrollCancelled) {
  SimRequest request("https://example.com/test.html#targetText=test",
                     "text/html");
  SimSubresourceRequest css_request("https://example.com/test.css", "text/css");
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
        visibility: hidden;
      }
    </style>
    <link rel=stylesheet href=test.css>
    <p>This is a test page</p>
  )HTML");

  Compositor().PaintFrame();
  GetDocument().View()->LayoutViewport()->ScrollBy(ScrollOffset(0, 100),
                                                   kUserScroll);

  // Set the target text to visible and change its position to cause a layout
  // and invoke the fragment anchor.
  css_request.Complete("p { visibility: visible; top: 1001px; }");

  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.SelectorCount", 1,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.MatchRate", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.MatchRate", 100, 1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.AmbiguousMatch", 0,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.ScrollCancelled", 1);
  histogram_tester_.ExpectUniqueSample("TextFragmentAnchor.ScrollCancelled", 1,
                                       1);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.DidScrollIntoView", 0);

  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.TimeToScrollIntoView",
                                     0);
}

}  // namespace

}  // namespace blink
