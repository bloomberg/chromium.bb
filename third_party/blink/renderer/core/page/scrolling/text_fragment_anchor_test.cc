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
  EXPECT_TRUE(GetDocument().Markers().Markers().IsEmpty());
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

// Ensure multiple targetTexts are highlighted and the first is scrolled into
// view.
TEST_F(TextFragmentAnchorTest, MultipleTextFragments) {
  SimRequest request(
      "https://example.com/test.html#targetText=test&targetText=more",
      "text/html");
  LoadURL("https://example.com/test.html#targetText=test&targetText=more");
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
    <p id="second">This is some more text</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& first = *GetDocument().getElementById("first");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(first)))
      << "First <p> wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());
}

// Ensure we scroll the second targetText into view if the first isn't found.
TEST_F(TextFragmentAnchorTest, FirstTextFragmentNotFound) {
  SimRequest request(
      "https://example.com/test.html#targetText=test&targetText=more",
      "text/html");
  LoadURL("https://example.com/test.html#targetText=test&targetText=more");
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
    <p id="first">This is a page</p>
    <p id="second">This is some more text</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& second = *GetDocument().getElementById("second");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(second)))
      << "Second <p> wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Ensure we still scroll the first targetText into view if the second isn't
// found.
TEST_F(TextFragmentAnchorTest, OnlyFirstTextFragmentFound) {
  SimRequest request(
      "https://example.com/test.html#targetText=test&targetText=more",
      "text/html");
  LoadURL("https://example.com/test.html#targetText=test&targetText=more");
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

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Make sure multiple non-matching strings doesn't cause scroll and the fragment
// is removed when completed.
TEST_F(TextFragmentAnchorTest, MultipleNonMatchingStrings) {
  SimRequest request(
      "https://example.com/"
      "test.html#targetText=unicorn&targetText=cookie&targetText=cat",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#targetText=unicorn&targetText=cookie&targetText=cat");
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
  EXPECT_TRUE(GetDocument().Markers().Markers().IsEmpty());
}

// Test matching a text range within the same element
TEST_F(TextFragmentAnchorTest, SameElementTextRange) {
  SimRequest request("https://example.com/test.html#targetText=this,page",
                     "text/html");
  LoadURL("https://example.com/test.html#targetText=this,page");
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

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on "This is a test page".
  Text* text = ToText(GetDocument().getElementById("text")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::TextMatch());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(19u, markers.at(0)->EndOffset());
}

// Test matching a text range across two neighboring elements
TEST_F(TextFragmentAnchorTest, NeighboringElementTextRange) {
  SimRequest request("https://example.com/test.html#targetText=test,paragraph",
                     "text/html");
  LoadURL("https://example.com/test.html#targetText=test,paragraph");
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
    <p id="text1">This is a test page</p>
    <p id="text2">with another paragraph of text</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  // Expect marker on "test page"
  Text* text1 = ToText(GetDocument().getElementById("text1")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text1, DocumentMarker::MarkerTypes::TextMatch());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers.at(0)->StartOffset());
  EXPECT_EQ(19u, markers.at(0)->EndOffset());

  // Expect marker on "with another paragraph"
  Text* text2 = ToText(GetDocument().getElementById("text2")->firstChild());
  markers = GetDocument().Markers().MarkersFor(
      *text2, DocumentMarker::MarkerTypes::TextMatch());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(22u, markers.at(0)->EndOffset());
}

// Test matching a text range from an element to a deeper nested element
TEST_F(TextFragmentAnchorTest, DifferentDepthElementTextRange) {
  SimRequest request("https://example.com/test.html#targetText=test,paragraph",
                     "text/html");
  LoadURL("https://example.com/test.html#targetText=test,paragraph");
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
    <p id="text1">This is a test page</p>
    <div>
      <p id="text2">with another paragraph of text</p>
    </div>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  // Expect marker on "test page"
  Text* text1 = ToText(GetDocument().getElementById("text1")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text1, DocumentMarker::MarkerTypes::TextMatch());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers.at(0)->StartOffset());
  EXPECT_EQ(19u, markers.at(0)->EndOffset());

  // Expect marker on "with another paragraph"
  Text* text2 = ToText(GetDocument().getElementById("text2")->firstChild());
  markers = GetDocument().Markers().MarkersFor(
      *text2, DocumentMarker::MarkerTypes::TextMatch());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(22u, markers.at(0)->EndOffset());
}

// Ensure that we don't match anything if endText is not found.
TEST_F(TextFragmentAnchorTest, TextRangeEndTextNotFound) {
  SimRequest request("https://example.com/test.html#targetText=test,cat",
                     "text/html");
  LoadURL("https://example.com/test.html#targetText=test,cat");
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

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
  EXPECT_EQ(ScrollOffset(), LayoutViewport()->GetScrollOffset());
}

// Test matching multiple text ranges
TEST_F(TextFragmentAnchorTest, MultipleTextRanges) {
  SimRequest request(
      "https://example.com/"
      "test.html#targetText=test,with&targetText=paragraph,text",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#targetText=test,with&targetText=paragraph,text");
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
    <p id="text1">This is a test page</p>
    <div>
      <p id="text2">with another paragraph of text</p>
    </div>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(3u, GetDocument().Markers().Markers().size());

  // Expect marker on "test page"
  Text* text1 = ToText(GetDocument().getElementById("text1")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text1, DocumentMarker::MarkerTypes::TextMatch());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers.at(0)->StartOffset());
  EXPECT_EQ(19u, markers.at(0)->EndOffset());

  // Expect markers on "with" and "paragraph of text"
  Text* text2 = ToText(GetDocument().getElementById("text2")->firstChild());
  markers = GetDocument().Markers().MarkersFor(
      *text2, DocumentMarker::MarkerTypes::TextMatch());
  ASSERT_EQ(2u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(4u, markers.at(0)->EndOffset());
  EXPECT_EQ(13u, markers.at(1)->StartOffset());
  EXPECT_EQ(30u, markers.at(1)->EndOffset());
}

// Ensure we scroll to the beginning of a text range larger than the viewport.
TEST_F(TextFragmentAnchorTest, DistantElementTextRange) {
  SimRequest request("https://example.com/test.html#targetText=test,paragraph",
                     "text/html");
  LoadURL("https://example.com/test.html#targetText=test,paragraph");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        margin-top: 3000px;
      }
    </style>
    <p id="text">This is a test page</p>
    <p>with another paragraph of text</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& p = *GetDocument().getElementById("text");
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)))
      << "<p> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());
}

// Test a text range with both context terms in the same element.
TEST_F(TextFragmentAnchorTest, TextRangeWithContext) {
  SimRequest request(
      "https://example.com/test.html#targetText=this-,is,test,-page",
      "text/html");
  LoadURL("https://example.com/test.html#targetText=this-,is,test,-page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on "is a test".
  Text* text = ToText(GetDocument().getElementById("text")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::TextMatch());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(5u, markers.at(0)->StartOffset());
  EXPECT_EQ(14u, markers.at(0)->EndOffset());
}

// Ensure that we do not match a text range if the prefix is not found.
TEST_F(TextFragmentAnchorTest, PrefixNotFound) {
  SimRequest request(
      "https://example.com/test.html#targetText=prefix-,is,test,-page",
      "text/html");
  LoadURL("https://example.com/test.html#targetText=prefix-,is,test,-page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

// Ensure that we do not match a text range if the suffix is not found.
TEST_F(TextFragmentAnchorTest, SuffixNotFound) {
  SimRequest request(
      "https://example.com/test.html#targetText=this-,is,test,-suffix",
      "text/html");
  LoadURL("https://example.com/test.html#targetText=this-,is,test,-suffix");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

// Test a text range with context terms in different elements
TEST_F(TextFragmentAnchorTest, TextRangeWithCrossElementContext) {
  SimRequest request(
      "https://example.com/test.html#targetText=header%202-,a,text,-footer%201",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#targetText=header%202-,a,text,-footer%201");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <h1>Header 1</h1>
    <p>A string of text</p>
    <p>Footer 1</p>
    <h1>Header 2</h1>
    <p id="expected">A string of text</p>
    <p>Footer 1</p>
    <h1>Header 2</h1>
    <p>A string of text</p>
    <p>Footer 2</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on the expected "A string of text".
  Text* text = ToText(GetDocument().getElementById("expected")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::TextMatch());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(16u, markers.at(0)->EndOffset());
}

// Test context terms separated by elements and whitespace
TEST_F(TextFragmentAnchorTest, CrossElementAndWhitespaceContext) {
  SimRequest request(
      "https://example.com/"
      "test.html#targetText=list%202-,cat,-good%20cat",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#targetText=list%202-,cat,-good%20cat");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <h1> List 1 </h1>
    <div>
      <p>Cat</p>
      <p>&nbsp;Good cat</p>
    </div>
    <h1> List 2 </h1>
    <div>
      <p id="expected">Cat</p>
      <p>&nbsp;Good cat</p>
    </div>
    <h1> List 2 </h1>
    <div>
      <p>Cat</p>
      <p>&nbsp;Bad cat</p>
    </div>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on the expected "cat".
  Text* text = ToText(GetDocument().getElementById("expected")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::TextMatch());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(3u, markers.at(0)->EndOffset());
}

// Test context terms separated by empty sibling and parent elements
TEST_F(TextFragmentAnchorTest, CrossEmptySiblingAndParentElementContext) {
  SimRequest request(
      "https://example.com/"
      "test.html#targetText=prefix-,match,-suffix",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#targetText=prefix-,match,-suffix");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>
      <p>prefix</p>
    <div>
    <p><br>&nbsp;</p>
    <div id="expected">match</div>
    <p><br>&nbsp;</p>
    <div>
      <p>suffix</p>
    <div>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on "match".
  Text* text = ToText(GetDocument().getElementById("expected")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::TextMatch());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(5u, markers.at(0)->EndOffset());
}

// Ensure we scroll to text when its prefix and suffix are out of view.
TEST_F(TextFragmentAnchorTest, DistantElementContext) {
  SimRequest request(
      "https://example.com/test.html#targetText=prefix-,cats,-suffix",
      "text/html");
  LoadURL("https://example.com/test.html#targetText=prefix-,cats,-suffix");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        margin-top: 3000px;
      }
    </style>
    <p>Cats</p>
    <p>Prefix</p>
    <p id="text">Cats</p>
    <p>Suffix</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& p = *GetDocument().getElementById("text");
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)))
      << "<p> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Test specifying just one of the prefix and suffix
TEST_F(TextFragmentAnchorTest, OneContextTerm) {
  SimRequest request(
      "https://example.com/"
      "test.html#targetText=test-,page&targetText=page,-with%20real%20content",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#targetText=test-,page&targetText=page,-with%20real%20content");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id="text1">This is a test page</p>
    <p id="text2">Not a page with real content</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  // Expect marker on the first "page"
  Text* text1 = ToText(GetDocument().getElementById("text1")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text1, DocumentMarker::MarkerTypes::TextMatch());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(15u, markers.at(0)->StartOffset());
  EXPECT_EQ(19u, markers.at(0)->EndOffset());

  // Expect marker on the second "page"
  Text* text2 = ToText(GetDocument().getElementById("text2")->firstChild());
  markers = GetDocument().Markers().MarkersFor(
      *text2, DocumentMarker::MarkerTypes::TextMatch());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(6u, markers.at(0)->StartOffset());
  EXPECT_EQ(10u, markers.at(0)->EndOffset());
}

}  // namespace

}  // namespace blink
