// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ScrollAnchor.h"

#include "core/dom/ClientRect.h"
#include "core/frame/VisualViewport.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/page/PrintContext.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"

namespace blink {

using Corner = ScrollAnchor::Corner;

typedef bool TestParamRootLayerScrolling;
class ScrollAnchorTest
    : public testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedRootLayerScrollingForTest,
      public RenderingTest {
 public:
  ScrollAnchorTest() : ScopedRootLayerScrollingForTest(GetParam()) {
    RuntimeEnabledFeatures::setScrollAnchoringEnabled(true);
  }
  ~ScrollAnchorTest() {
    RuntimeEnabledFeatures::setScrollAnchoringEnabled(false);
  }

 protected:
  void Update() {
    // TODO(skobes): Use SimTest instead of RenderingTest and move into
    // Source/web?
    GetDocument().View()->UpdateAllLifecyclePhases();
  }

  ScrollableArea* LayoutViewport() {
    return GetDocument().View()->LayoutViewportScrollableArea();
  }

  VisualViewport& GetVisualViewport() {
    return GetDocument().View()->GetPage()->GetVisualViewport();
  }

  ScrollableArea* ScrollerForElement(Element* element) {
    return ToLayoutBox(element->GetLayoutObject())->GetScrollableArea();
  }

  ScrollAnchor& GetScrollAnchor(ScrollableArea* scroller) {
    DCHECK(scroller->IsFrameView() || scroller->IsPaintLayerScrollableArea());
    return *(scroller->GetScrollAnchor());
  }

  void SetHeight(Element* element, int height) {
    element->setAttribute(HTMLNames::styleAttr,
                          AtomicString(String::Format("height: %dpx", height)));
    Update();
  }

  void ScrollLayoutViewport(ScrollOffset delta) {
    Element* scrolling_element = GetDocument().scrollingElement();
    if (delta.Width())
      scrolling_element->setScrollLeft(scrolling_element->scrollLeft() +
                                       delta.Width());
    if (delta.Height())
      scrolling_element->setScrollTop(scrolling_element->scrollTop() +
                                      delta.Height());
  }
};

INSTANTIATE_TEST_CASE_P(All, ScrollAnchorTest, ::testing::Bool());

// TODO(ymalik): Currently, this should be the first test in the file to avoid
// failure when running with other tests. Dig into this more and fix.
TEST_P(ScrollAnchorTest, UMAMetricUpdated) {
  HistogramTester histogram_tester;
  SetBodyInnerHTML(
      "<style> body { height: 1000px } div { height: 100px } </style>"
      "<div id='block1'>abc</div>"
      "<div id='block2'>def</div>");

  ScrollableArea* viewport = LayoutViewport();

  // Scroll position not adjusted, metric not updated.
  ScrollLayoutViewport(ScrollOffset(0, 150));
  histogram_tester.ExpectTotalCount("Layout.ScrollAnchor.AdjustedScrollOffset",
                                    0);

  // Height changed, verify metric updated once.
  SetHeight(GetDocument().GetElementById("block1"), 200);
  histogram_tester.ExpectUniqueSample(
      "Layout.ScrollAnchor.AdjustedScrollOffset", 1, 1);

  EXPECT_EQ(250, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().GetElementById("block2")->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());
}

TEST_P(ScrollAnchorTest, Basic) {
  SetBodyInnerHTML(
      "<style> body { height: 1000px } div { height: 100px } </style>"
      "<div id='block1'>abc</div>"
      "<div id='block2'>def</div>");

  ScrollableArea* viewport = LayoutViewport();

  // No anchor at origin (0,0).
  EXPECT_EQ(nullptr, GetScrollAnchor(viewport).AnchorObject());

  ScrollLayoutViewport(ScrollOffset(0, 150));
  SetHeight(GetDocument().GetElementById("block1"), 200);

  EXPECT_EQ(250, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().GetElementById("block2")->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());

  // ScrollableArea::userScroll should clear the anchor.
  viewport->UserScroll(kScrollByPrecisePixel, FloatSize(0, 100));
  EXPECT_EQ(nullptr, GetScrollAnchor(viewport).AnchorObject());
}

TEST_P(ScrollAnchorTest, VisualViewportAnchors) {
  SetBodyInnerHTML(
      "<style>"
      "    * { font-size: 1.2em; font-family: sans-serif; }"
      "    div { height: 100px; width: 20px; background-color: pink; }"
      "</style>"
      "<div id='div'></div>"
      "<div id='text'><b>This is a scroll anchoring test</div>");

  ScrollableArea* l_viewport = LayoutViewport();
  VisualViewport& v_viewport = GetVisualViewport();

  v_viewport.SetScale(2.0);

  // No anchor at origin (0,0).
  EXPECT_EQ(nullptr, GetScrollAnchor(l_viewport).AnchorObject());

  // Scroll the visual viewport to bring #text to the top.
  int top =
      GetDocument().GetElementById("text")->getBoundingClientRect()->top();
  v_viewport.SetLocation(FloatPoint(0, top));

  SetHeight(GetDocument().GetElementById("div"), 10);
  EXPECT_EQ(GetDocument().GetElementById("text")->GetLayoutObject(),
            GetScrollAnchor(l_viewport).AnchorObject());
  EXPECT_EQ(top - 90, v_viewport.ScrollOffsetInt().Height());

  SetHeight(GetDocument().GetElementById("div"), 100);
  EXPECT_EQ(GetDocument().GetElementById("text")->GetLayoutObject(),
            GetScrollAnchor(l_viewport).AnchorObject());
  EXPECT_EQ(top, v_viewport.ScrollOffsetInt().Height());

  // Scrolling the visual viewport should clear the anchor.
  v_viewport.SetLocation(FloatPoint(0, 0));
  EXPECT_EQ(nullptr, GetScrollAnchor(l_viewport).AnchorObject());
}

// Test that we ignore the clipped content when computing visibility otherwise
// we may end up with an anchor that we think is in the viewport but is not.
TEST_P(ScrollAnchorTest, ClippedScrollersSkipped) {
  SetBodyInnerHTML(
      "<style>"
      "    body { height: 2000px; }"
      "    #scroller { overflow: scroll; width: 500px; height: 300px; }"
      "    .anchor {"
      "         position:relative; height: 100px; width: 150px;"
      "         background-color: #afa; border: 1px solid gray;"
      "    }"
      "    #forceScrolling { height: 500px; background-color: #fcc; }"
      "</style>"
      "<div id='scroller'>"
      "    <div id='innerChanger'></div>"
      "    <div id='innerAnchor' class='anchor'></div>"
      "    <div id='forceScrolling'></div>"
      "</div>"
      "<div id='outerChanger'></div>"
      "<div id='outerAnchor' class='anchor'></div>");

  ScrollableArea* scroller =
      ScrollerForElement(GetDocument().GetElementById("scroller"));
  ScrollableArea* viewport = LayoutViewport();

  GetDocument().GetElementById("scroller")->setScrollTop(100);
  ScrollLayoutViewport(ScrollOffset(0, 350));

  SetHeight(GetDocument().GetElementById("innerChanger"), 200);
  SetHeight(GetDocument().GetElementById("outerChanger"), 150);

  EXPECT_EQ(300, scroller->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().GetElementById("innerAnchor")->GetLayoutObject(),
            GetScrollAnchor(scroller).AnchorObject());
  EXPECT_EQ(500, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().GetElementById("outerAnchor")->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());
}

// Test that scroll anchoring causes no visible jump when a layout change
// (such as removal of a DOM element) changes the scroll bounds.
TEST_P(ScrollAnchorTest, AnchoringWhenContentRemoved) {
  SetBodyInnerHTML(
      "<style>"
      "    #changer { height: 1500px; }"
      "    #anchor {"
      "        width: 150px; height: 1000px; background-color: pink;"
      "    }"
      "</style>"
      "<div id='changer'></div>"
      "<div id='anchor'></div>");

  ScrollableArea* viewport = LayoutViewport();
  ScrollLayoutViewport(ScrollOffset(0, 1600));

  SetHeight(GetDocument().GetElementById("changer"), 0);

  EXPECT_EQ(100, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().GetElementById("anchor")->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());
}

// Test that scroll anchoring causes no visible jump when a layout change
// (such as removal of a DOM element) changes the scroll bounds of a scrolling
// div.
TEST_P(ScrollAnchorTest, AnchoringWhenContentRemovedFromScrollingDiv) {
  SetBodyInnerHTML(
      "<style>"
      "    #scroller { height: 500px; width: 200px; overflow: scroll; }"
      "    #changer { height: 1500px; }"
      "    #anchor {"
      "        width: 150px; height: 1000px; overflow: scroll;"
      "    }"
      "</style>"
      "<div id='scroller'>"
      "    <div id='changer'></div>"
      "    <div id='anchor'></div>"
      "</div>");

  ScrollableArea* scroller =
      ScrollerForElement(GetDocument().GetElementById("scroller"));

  GetDocument().GetElementById("scroller")->setScrollTop(1600);

  SetHeight(GetDocument().GetElementById("changer"), 0);

  EXPECT_EQ(100, scroller->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().GetElementById("anchor")->GetLayoutObject(),
            GetScrollAnchor(scroller).AnchorObject());
}

// Test that a non-anchoring scroll on scroller clears scroll anchors for all
// parent scrollers.
TEST_P(ScrollAnchorTest, ClearScrollAnchorsOnAncestors) {
  SetBodyInnerHTML(
      "<style>"
      "    body { height: 1000px } div { height: 200px }"
      "    #scroller { height: 100px; width: 200px; overflow: scroll; }"
      "</style>"
      "<div id='changer'>abc</div>"
      "<div id='anchor'>def</div>"
      "<div id='scroller'><div></div></div>");

  ScrollableArea* viewport = LayoutViewport();

  ScrollLayoutViewport(ScrollOffset(0, 250));
  SetHeight(GetDocument().GetElementById("changer"), 300);

  EXPECT_EQ(350, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().GetElementById("anchor")->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());

  // Scrolling the nested scroller should clear the anchor on the main frame.
  ScrollableArea* scroller =
      ScrollerForElement(GetDocument().GetElementById("scroller"));
  scroller->ScrollBy(ScrollOffset(0, 100), kUserScroll);
  EXPECT_EQ(nullptr, GetScrollAnchor(viewport).AnchorObject());
}

TEST_P(ScrollAnchorTest, AncestorClearingWithSiblingReference) {
  SetBodyInnerHTML(
      "<style>"
      ".scroller {"
      "  overflow: scroll;"
      "  width: 400px;"
      "  height: 400px;"
      "}"
      ".space {"
      "  width: 100px;"
      "  height: 600px;"
      "}"
      "</style>"
      "<div id='s1' class='scroller'>"
      "  <div id='anchor' class='space'></div>"
      "</div>"
      "<div id='s2' class='scroller'>"
      "  <div class='space'></div>"
      "</div>");
  Element* s1 = GetDocument().GetElementById("s1");
  Element* s2 = GetDocument().GetElementById("s2");
  Element* anchor = GetDocument().GetElementById("anchor");

  // Set non-zero scroll offsets for #s1 and #document
  s1->setScrollTop(100);
  ScrollLayoutViewport(ScrollOffset(0, 100));

  // Invalidate layout.
  SetHeight(anchor, 500);

  // This forces layout, during which both #s1 and #document will anchor to
  // #anchor. Then the scroll clears #s2 and #document.  Since #anchor is still
  // referenced by #s1, its IsScrollAnchorObject bit must remain set.
  s2->setScrollTop(100);

  // This should clear #s1.  If #anchor had its bit cleared already we would
  // crash in update().
  s1->RemoveChild(anchor);
  Update();
}

TEST_P(ScrollAnchorTest, FractionalOffsetsAreRoundedBeforeComparing) {
  SetBodyInnerHTML(
      "<style> body { height: 1000px } </style>"
      "<div id='block1' style='height: 50.4px'>abc</div>"
      "<div id='block2' style='height: 100px'>def</div>");

  ScrollableArea* viewport = LayoutViewport();
  ScrollLayoutViewport(ScrollOffset(0, 100));

  GetDocument().GetElementById("block1")->setAttribute(HTMLNames::styleAttr,
                                                       "height: 50.6px");
  Update();

  EXPECT_EQ(101, viewport->ScrollOffsetInt().Height());
}

TEST_P(ScrollAnchorTest, AnchorWithLayerInScrollingDiv) {
  SetBodyInnerHTML(
      "<style>"
      "    #scroller { overflow: scroll; width: 500px; height: 400px; }"
      "    div { height: 100px }"
      "    #block2 { overflow: hidden }"
      "    #space { height: 1000px; }"
      "</style>"
      "<div id='scroller'><div id='space'>"
      "<div id='block1'>abc</div>"
      "<div id='block2'>def</div>"
      "</div></div>");

  ScrollableArea* scroller =
      ScrollerForElement(GetDocument().GetElementById("scroller"));
  Element* block1 = GetDocument().GetElementById("block1");
  Element* block2 = GetDocument().GetElementById("block2");

  scroller->ScrollBy(ScrollOffset(0, 150), kUserScroll);

  // In this layout pass we will anchor to #block2 which has its own PaintLayer.
  SetHeight(block1, 200);
  EXPECT_EQ(250, scroller->ScrollOffsetInt().Height());
  EXPECT_EQ(block2->GetLayoutObject(),
            GetScrollAnchor(scroller).AnchorObject());

  // Test that the anchor object can be destroyed without affecting the scroll
  // position.
  block2->remove();
  Update();
  EXPECT_EQ(250, scroller->ScrollOffsetInt().Height());
}

// Verify that a nested scroller with a div that has its own PaintLayer can be
// removed without causing a crash. This test passes if it doesn't crash.
TEST_P(ScrollAnchorTest, RemoveScrollerWithLayerInScrollingDiv) {
  SetBodyInnerHTML(
      "<style>"
      "    body { height: 2000px }"
      "    #scroller { overflow: scroll; width: 500px; height: 400px}"
      "    #block1 { height: 100px; width: 100px; overflow: hidden}"
      "    #anchor { height: 1000px; }"
      "</style>"
      "<div id='changer1'></div>"
      "<div id='scroller'>"
      "  <div id='changer2'></div>"
      "  <div id='block1'></div>"
      "  <div id='anchor'></div>"
      "</div>");

  ScrollableArea* viewport = LayoutViewport();
  ScrollableArea* scroller =
      ScrollerForElement(GetDocument().GetElementById("scroller"));
  Element* changer1 = GetDocument().GetElementById("changer1");
  Element* changer2 = GetDocument().GetElementById("changer2");
  Element* anchor = GetDocument().GetElementById("anchor");

  scroller->ScrollBy(ScrollOffset(0, 150), kUserScroll);
  ScrollLayoutViewport(ScrollOffset(0, 50));

  // In this layout pass both the inner and outer scroller will anchor to
  // #anchor.
  SetHeight(changer1, 100);
  SetHeight(changer2, 100);
  EXPECT_EQ(250, scroller->ScrollOffsetInt().Height());
  EXPECT_EQ(anchor->GetLayoutObject(),
            GetScrollAnchor(scroller).AnchorObject());
  EXPECT_EQ(anchor->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());

  // Test that the inner scroller can be destroyed without crashing.
  GetDocument().GetElementById("scroller")->remove();
  Update();
}

TEST_P(ScrollAnchorTest, ExcludeAnonymousCandidates) {
  SetBodyInnerHTML(
      "<style>"
      "    body { height: 3500px }"
      "    #div {"
      "        position: relative; background-color: pink;"
      "        top: 5px; left: 5px; width: 100px; height: 3500px;"
      "    }"
      "    #inline { padding-left: 10px }"
      "</style>"
      "<div id='div'>"
      "    <a id='inline'>text</a>"
      "    <p id='block'>Some text</p>"
      "</div>"
      "<div id=a>after</div>");

  ScrollableArea* viewport = LayoutViewport();
  Element* inline_elem = GetDocument().GetElementById("inline");
  EXPECT_TRUE(inline_elem->GetLayoutObject()->Parent()->IsAnonymous());

  // Scroll #div into view, making anonymous block a viable candidate.
  GetDocument().GetElementById("div")->scrollIntoView();

  // Trigger layout and verify that we don't anchor to the anonymous block.
  SetHeight(GetDocument().GetElementById("a"), 100);
  Update();
  EXPECT_EQ(inline_elem->GetLayoutObject()->SlowFirstChild(),
            GetScrollAnchor(viewport).AnchorObject());
}

TEST_P(ScrollAnchorTest, FullyContainedInlineBlock) {
  // Exercises every WalkStatus value:
  // html, body -> Constrain
  // #outer -> Continue
  // #ib1, br -> Skip
  // #ib2 -> Return
  SetBodyInnerHTML(
      "<style>"
      "    body { height: 1000px }"
      "    #outer { line-height: 100px }"
      "    #ib1, #ib2 { display: inline-block }"
      "</style>"
      "<span id=outer>"
      "    <span id=ib1>abc</span>"
      "    <br><br>"
      "    <span id=ib2>def</span>"
      "</span>");

  ScrollLayoutViewport(ScrollOffset(0, 150));

  Element* ib1 = GetDocument().GetElementById("ib1");
  ib1->setAttribute(HTMLNames::styleAttr, "line-height: 150px");
  Update();
  EXPECT_EQ(GetDocument().GetElementById("ib2")->GetLayoutObject(),
            GetScrollAnchor(LayoutViewport()).AnchorObject());
}

TEST_P(ScrollAnchorTest, TextBounds) {
  SetBodyInnerHTML(
      "<style>"
      "    body {"
      "        position: absolute;"
      "        font-size: 100px;"
      "        width: 200px;"
      "        height: 1000px;"
      "        line-height: 100px;"
      "    }"
      "</style>"
      "abc <b id=b>def</b> ghi"
      "<div id=a>after</div>");

  ScrollLayoutViewport(ScrollOffset(0, 150));

  SetHeight(GetDocument().GetElementById("a"), 100);
  EXPECT_EQ(
      GetDocument().GetElementById("b")->GetLayoutObject()->SlowFirstChild(),
      GetScrollAnchor(LayoutViewport()).AnchorObject());
}

TEST_P(ScrollAnchorTest, ExcludeFixedPosition) {
  SetBodyInnerHTML(
      "<style>"
      "    body { height: 1000px; padding: 20px; }"
      "    div { position: relative; top: 100px; }"
      "    #f { position: fixed }"
      "</style>"
      "<div id=f>fixed</div>"
      "<div id=c>content</div>"
      "<div id=a>after</div>");

  ScrollLayoutViewport(ScrollOffset(0, 50));

  SetHeight(GetDocument().GetElementById("a"), 100);
  EXPECT_EQ(GetDocument().GetElementById("c")->GetLayoutObject(),
            GetScrollAnchor(LayoutViewport()).AnchorObject());
}

// This test verifies that position:absolute elements that stick to the viewport
// are not selected as anchors.
TEST_P(ScrollAnchorTest, ExcludeAbsolutePositionThatSticksToViewport) {
  SetBodyInnerHTML(
      "<style>"
      "    body { margin: 0; }"
      "    #scroller { overflow: scroll; width: 500px; height: 400px; }"
      "    #space { height: 1000px; }"
      "    #abs {"
      "        position: absolute; background-color: red;"
      "        width: 100px; height: 100px;"
      "    }"
      "    #rel {"
      "        position: relative; background-color: green;"
      "        left: 50px; top: 100px; width: 100px; height: 75px;"
      "    }"
      "</style>"
      "<div id='scroller'><div id='space'>"
      "    <div id='abs'></div>"
      "    <div id='rel'></div>"
      "    <div id=a>after</div>"
      "</div></div>");

  Element* scroller_element = GetDocument().GetElementById("scroller");
  ScrollableArea* scroller = ScrollerForElement(scroller_element);
  Element* abs_pos = GetDocument().GetElementById("abs");
  Element* rel_pos = GetDocument().GetElementById("rel");

  scroller->ScrollBy(ScrollOffset(0, 25), kUserScroll);
  SetHeight(GetDocument().GetElementById("a"), 100);

  // When the scroller is position:static, the anchor cannot be
  // position:absolute.
  EXPECT_EQ(rel_pos->GetLayoutObject(),
            GetScrollAnchor(scroller).AnchorObject());

  scroller_element->setAttribute(HTMLNames::styleAttr, "position: relative");
  Update();
  scroller->ScrollBy(ScrollOffset(0, 25), kUserScroll);
  SetHeight(GetDocument().GetElementById("a"), 125);

  // When the scroller is position:relative, the anchor may be
  // position:absolute.
  EXPECT_EQ(abs_pos->GetLayoutObject(),
            GetScrollAnchor(scroller).AnchorObject());
}

TEST_P(ScrollAnchorTest, DescendsIntoAbsPosWithOffscreenStaticParent) {
  SetBodyInnerHTML(
      "<style>"
      "  body, html { height: 0; }"
      "  #abs {"
      "    position: absolute;"
      "    left: 50px;"
      "    top: 50px;"
      "    height: 1200px;"
      "    padding: 50px;"
      "    border: 5px solid gray;"
      "  }"
      "  #anchor {"
      "    background-color: #afa;"
      "    width: 100px;"
      "    height: 100px;"
      "  }"
      "</style>"
      "<div id='abs'>"
      "  <div id='changer'></div>"
      "  <div id='anchor'></div>"
      "</div>");

  ScrollLayoutViewport(ScrollOffset(0, 120));
  SetHeight(GetDocument().GetElementById("changer"), 100);
  EXPECT_EQ(220, LayoutViewport()->ScrollOffsetInt().Height());
}

// Test that we descend into zero-height containers that have overflowing
// content.
TEST_P(ScrollAnchorTest, DescendsIntoContainerWithOverflow) {
  SetBodyInnerHTML(
      "<style>"
      "    body { height: 1000; }"
      "    #outer { width: 300px; }"
      "    #zeroheight { height: 0px; }"
      "    #changer { height: 100px; background-color: red; }"
      "    #bottom { margin-top: 600px; }"
      "</style>"
      "<div id='outer'>"
      "    <div id='zeroheight'>"
      "      <div id='changer'></div>"
      "      <div id='bottom'>bottom</div>"
      "    </div>"
      "</div>");

  ScrollableArea* viewport = LayoutViewport();

  ScrollLayoutViewport(ScrollOffset(0, 200));
  SetHeight(GetDocument().GetElementById("changer"), 200);

  EXPECT_EQ(300, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().GetElementById("bottom")->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());
}

// Test that we account for the origin of the layout overflow rect when
// computing bounds for possible descent.
TEST_P(ScrollAnchorTest, NegativeLayoutOverflow) {
  SetBodyInnerHTML(
      "<style>"
      "    body { height: 1200px; }"
      "    #header { position: relative; height: 100px; }"
      "    #evil { position: relative; "
      "      top: -900px; height: 1000px; width: 100px; }"
      "    #changer { height: 100px; }"
      "    #anchor { height: 100px; background-color: green }"
      "</style>"
      "<div id='header'>"
      "    <div id='evil'></div>"
      "</div>"
      "<div id='changer'></div>"
      "<div id='anchor'></div>");

  ScrollableArea* viewport = LayoutViewport();

  ScrollLayoutViewport(ScrollOffset(0, 250));
  SetHeight(GetDocument().GetElementById("changer"), 200);
  EXPECT_EQ(350, viewport->ScrollOffsetInt().Height());
}

// Test that we descend into zero-height containers that have floating content.
TEST_P(ScrollAnchorTest, DescendsIntoContainerWithFloat) {
  SetBodyInnerHTML(
      "<style>"
      "    body { height: 1000; }"
      "    #outer { width: 300px; }"
      "    #outer:after { content: ' '; clear:both; display: table; }"
      "    #float {"
      "         float: left; background-color: #ccc;"
      "         height: 500px; width: 100%;"
      "    }"
      "    #inner { height: 21px; background-color:#7f0; }"
      "</style>"
      "<div id='outer'>"
      "    <div id='zeroheight'>"
      "      <div id='float'>"
      "         <div id='inner'></div>"
      "      </div>"
      "    </div>"
      "</div>"
      "<div id=a>after</div>");

  EXPECT_EQ(
      0,
      ToLayoutBox(GetDocument().GetElementById("zeroheight")->GetLayoutObject())
          ->Size()
          .Height());

  ScrollableArea* viewport = LayoutViewport();

  ScrollLayoutViewport(ScrollOffset(0, 200));
  SetHeight(GetDocument().GetElementById("a"), 100);

  EXPECT_EQ(200, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().GetElementById("float")->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());
}

// This test verifies that scroll anchoring is disabled when any element within
// the main scroller changes its in-flow state.
TEST_P(ScrollAnchorTest, ChangeInFlowStateDisablesAnchoringForMainScroller) {
  SetBodyInnerHTML(
      "<style>"
      "    body { height: 1000px; }"
      "    #header { background-color: #F5B335; height: 50px; width: 100%; }"
      "    #content { background-color: #D3D3D3; height: 200px; }"
      "</style>"
      "<div id='header'></div>"
      "<div id='content'></div>");

  ScrollableArea* viewport = LayoutViewport();
  ScrollLayoutViewport(ScrollOffset(0, 200));

  GetDocument().GetElementById("header")->setAttribute(HTMLNames::styleAttr,
                                                       "position: fixed;");
  Update();

  EXPECT_EQ(200, viewport->ScrollOffsetInt().Height());
}

// This test verifies that scroll anchoring is disabled when any element within
// a scrolling div changes its in-flow state.
TEST_P(ScrollAnchorTest, ChangeInFlowStateDisablesAnchoringForScrollingDiv) {
  SetBodyInnerHTML(
      "<style>"
      "    #container { position: relative; width: 500px; }"
      "    #scroller { height: 200px; overflow: scroll; }"
      "    #changer { background-color: #F5B335; height: 50px; width: 100%; }"
      "    #anchor { background-color: #D3D3D3; height: 300px; }"
      "</style>"
      "<div id='container'>"
      "    <div id='scroller'>"
      "      <div id='changer'></div>"
      "      <div id='anchor'></div>"
      "    </div>"
      "</div>");

  ScrollableArea* scroller =
      ScrollerForElement(GetDocument().GetElementById("scroller"));
  GetDocument().GetElementById("scroller")->setScrollTop(100);

  GetDocument().GetElementById("changer")->setAttribute(HTMLNames::styleAttr,
                                                        "position: absolute;");
  Update();

  EXPECT_EQ(100, scroller->ScrollOffsetInt().Height());
}

TEST_P(ScrollAnchorTest, FlexboxDelayedClampingAlsoDelaysAdjustment) {
  SetBodyInnerHTML(
      "<style>"
      "    html { overflow: hidden; }"
      "    body {"
      "        position: absolute; display: flex;"
      "        top: 0; bottom: 0; margin: 0;"
      "    }"
      "    #scroller { overflow: auto; }"
      "    #spacer { width: 600px; height: 1200px; }"
      "    #before { height: 50px; }"
      "    #anchor {"
      "        width: 100px; height: 100px;"
      "        background-color: #8f8;"
      "    }"
      "</style>"
      "<div id='scroller'>"
      "    <div id='spacer'>"
      "        <div id='before'></div>"
      "        <div id='anchor'></div>"
      "    </div>"
      "</div>");

  Element* scroller = GetDocument().GetElementById("scroller");
  scroller->setScrollTop(100);

  SetHeight(GetDocument().GetElementById("before"), 100);
  EXPECT_EQ(150, ScrollerForElement(scroller)->ScrollOffsetInt().Height());
}

TEST_P(ScrollAnchorTest, FlexboxDelayedAdjustmentRespectsSANACLAP) {
  SetBodyInnerHTML(
      "<style>"
      "    html { overflow: hidden; }"
      "    body {"
      "        position: absolute; display: flex;"
      "        top: 0; bottom: 0; margin: 0;"
      "    }"
      "    #scroller { overflow: auto; }"
      "    #spacer { width: 600px; height: 1200px; }"
      "    #anchor {"
      "        position: relative; top: 50px;"
      "        width: 100px; height: 100px;"
      "        background-color: #8f8;"
      "    }"
      "</style>"
      "<div id='scroller'>"
      "    <div id='spacer'>"
      "        <div id='anchor'></div>"
      "    </div>"
      "</div>");

  Element* scroller = GetDocument().GetElementById("scroller");
  scroller->setScrollTop(100);

  GetDocument().GetElementById("spacer")->setAttribute(HTMLNames::styleAttr,
                                                       "margin-top: 50px");
  Update();
  EXPECT_EQ(100, ScrollerForElement(scroller)->ScrollOffsetInt().Height());
}

// Test then an element and its children are not selected as the anchor when
// it has the overflow-anchor property set to none.
TEST_P(ScrollAnchorTest, OptOutElement) {
  SetBodyInnerHTML(
      "<style>"
      "     body { height: 1000px }"
      "     .div {"
      "          height: 100px; width: 100px;"
      "          border: 1px solid gray; background-color: #afa;"
      "     }"
      "     #innerDiv {"
      "          height: 50px; width: 50px;"
      "          border: 1px solid gray; background-color: pink;"
      "     }"
      "</style>"
      "<div id='changer'></div>"
      "<div class='div' id='firstDiv'><div id='innerDiv'></div></div>"
      "<div class='div' id='secondDiv'></div>");

  ScrollableArea* viewport = LayoutViewport();
  ScrollLayoutViewport(ScrollOffset(0, 50));

  // No opt-out.
  SetHeight(GetDocument().GetElementById("changer"), 100);
  EXPECT_EQ(150, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().GetElementById("innerDiv")->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());

  // Clear anchor and opt-out element.
  ScrollLayoutViewport(ScrollOffset(0, 10));
  GetDocument()
      .GetElementById("firstDiv")
      ->setAttribute(HTMLNames::styleAttr,
                     AtomicString("overflow-anchor: none"));
  Update();

  // Opted out element and it's children skipped.
  SetHeight(GetDocument().GetElementById("changer"), 200);
  EXPECT_EQ(260, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().GetElementById("secondDiv")->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());
}

TEST_P(ScrollAnchorTest,
       SuppressAnchorNodeAncestorChangingLayoutAffectingProperty) {
  SetBodyInnerHTML(
      "<style> body { height: 1000px } div { height: 100px } </style>"
      "<div id='block1'>abc</div>");

  ScrollableArea* viewport = LayoutViewport();

  ScrollLayoutViewport(ScrollOffset(0, 50));
  GetDocument().body()->setAttribute(HTMLNames::styleAttr, "padding-top: 20px");
  Update();

  EXPECT_EQ(50, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(nullptr, GetScrollAnchor(viewport).AnchorObject());
}

TEST_P(ScrollAnchorTest, AnchorNodeAncestorChangingNonLayoutAffectingProperty) {
  SetBodyInnerHTML(
      "<style> body { height: 1000px } div { height: 100px } </style>"
      "<div id='block1'>abc</div>"
      "<div id='block2'>def</div>");

  ScrollableArea* viewport = LayoutViewport();
  ScrollLayoutViewport(ScrollOffset(0, 150));

  GetDocument().body()->setAttribute(HTMLNames::styleAttr, "color: red");
  SetHeight(GetDocument().GetElementById("block1"), 200);

  EXPECT_EQ(250, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().GetElementById("block2")->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());
}

TEST_P(ScrollAnchorTest, TransformIsLayoutAffecting) {
  SetBodyInnerHTML(
      "<style>"
      "    body { height: 1000px }"
      "    #block1 { height: 100px }"
      "</style>"
      "<div id='block1'>abc</div>"
      "<div id=a>after</div>");

  ScrollableArea* viewport = LayoutViewport();

  ScrollLayoutViewport(ScrollOffset(0, 50));
  GetDocument().GetElementById("block1")->setAttribute(
      HTMLNames::styleAttr, "transform: matrix(1, 0, 0, 1, 25, 25);");
  Update();

  GetDocument().GetElementById("block1")->setAttribute(
      HTMLNames::styleAttr, "transform: matrix(1, 0, 0, 1, 50, 50);");
  SetHeight(GetDocument().GetElementById("a"), 100);
  Update();

  EXPECT_EQ(50, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(nullptr, GetScrollAnchor(viewport).AnchorObject());
}

TEST_P(ScrollAnchorTest, OptOutBody) {
  SetBodyInnerHTML(
      "<style>"
      "    body { height: 2000px; overflow-anchor: none; }"
      "    #scroller { overflow: scroll; width: 500px; height: 300px; }"
      "    .anchor {"
      "        position:relative; height: 100px; width: 150px;"
      "        background-color: #afa; border: 1px solid gray;"
      "    }"
      "    #forceScrolling { height: 500px; background-color: #fcc; }"
      "</style>"
      "<div id='outerChanger'></div>"
      "<div id='outerAnchor' class='anchor'></div>"
      "<div id='scroller'>"
      "    <div id='innerChanger'></div>"
      "    <div id='innerAnchor' class='anchor'></div>"
      "    <div id='forceScrolling'></div>"
      "</div>");

  ScrollableArea* scroller =
      ScrollerForElement(GetDocument().GetElementById("scroller"));
  ScrollableArea* viewport = LayoutViewport();

  GetDocument().GetElementById("scroller")->setScrollTop(100);
  ScrollLayoutViewport(ScrollOffset(0, 100));

  SetHeight(GetDocument().GetElementById("innerChanger"), 200);
  SetHeight(GetDocument().GetElementById("outerChanger"), 150);

  // Scroll anchoring should apply within #scroller.
  EXPECT_EQ(300, scroller->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().GetElementById("innerAnchor")->GetLayoutObject(),
            GetScrollAnchor(scroller).AnchorObject());
  // Scroll anchoring should not apply within main frame.
  EXPECT_EQ(100, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(nullptr, GetScrollAnchor(viewport).AnchorObject());
}

TEST_P(ScrollAnchorTest, OptOutScrollingDiv) {
  SetBodyInnerHTML(
      "<style>"
      "    body { height: 2000px; }"
      "    #scroller {"
      "        overflow: scroll; width: 500px; height: 300px;"
      "        overflow-anchor: none;"
      "    }"
      "    .anchor {"
      "        position:relative; height: 100px; width: 150px;"
      "        background-color: #afa; border: 1px solid gray;"
      "    }"
      "    #forceScrolling { height: 500px; background-color: #fcc; }"
      "</style>"
      "<div id='outerChanger'></div>"
      "<div id='outerAnchor' class='anchor'></div>"
      "<div id='scroller'>"
      "    <div id='innerChanger'></div>"
      "    <div id='innerAnchor' class='anchor'></div>"
      "    <div id='forceScrolling'></div>"
      "</div>");

  ScrollableArea* scroller =
      ScrollerForElement(GetDocument().GetElementById("scroller"));
  ScrollableArea* viewport = LayoutViewport();

  GetDocument().GetElementById("scroller")->setScrollTop(100);
  ScrollLayoutViewport(ScrollOffset(0, 100));

  SetHeight(GetDocument().GetElementById("innerChanger"), 200);
  SetHeight(GetDocument().GetElementById("outerChanger"), 150);

  // Scroll anchoring should not apply within #scroller.
  EXPECT_EQ(100, scroller->ScrollOffsetInt().Height());
  EXPECT_EQ(nullptr, GetScrollAnchor(scroller).AnchorObject());
  // Scroll anchoring should apply within main frame.
  EXPECT_EQ(250, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().GetElementById("outerAnchor")->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());
}

TEST_P(ScrollAnchorTest, NonDefaultRootScroller) {
  SetBodyInnerHTML(
      "<style>"
      "    ::-webkit-scrollbar {"
      "      width: 0px; height: 0px;"
      "    }"
      "    body, html {"
      "      margin: 0px; width: 100%; height: 100%;"
      "    }"
      "    #rootscroller {"
      "      overflow: scroll; width: 100%; height: 100%;"
      "    }"
      "    .spacer {"
      "      height: 600px; width: 100px;"
      "    }"
      "    #target {"
      "      height: 100px; width: 100px; background-color: red;"
      "    }"
      "</style>"
      "<div id='rootscroller'>"
      "    <div id='firstChild' class='spacer'></div>"
      "    <div id='target'></div>"
      "    <div class='spacer'></div>"
      "</div>"
      "<div class='spacer'></div>");

  Element* root_scroller_element = GetDocument().GetElementById("rootscroller");

  NonThrowableExceptionState non_throw;
  GetDocument().setRootScroller(root_scroller_element, non_throw);
  GetDocument().View()->UpdateAllLifecyclePhases();

  ScrollableArea* scroller = ScrollerForElement(root_scroller_element);

  // By making the #rootScroller DIV the rootScroller, it should become the
  // layout viewport on the RootFrameViewport.
  ASSERT_EQ(scroller,
            &GetDocument().View()->GetRootFrameViewport()->LayoutViewport());

  // The #rootScroller DIV's anchor should have the RootFrameViewport set as
  // the scroller, rather than the FrameView's anchor.

  root_scroller_element->setScrollTop(600);

  SetHeight(GetDocument().GetElementById("firstChild"), 1000);

  // Scroll anchoring should be applied to #rootScroller.
  EXPECT_EQ(1000, scroller->GetScrollOffset().Height());
  EXPECT_EQ(GetDocument().GetElementById("target")->GetLayoutObject(),
            GetScrollAnchor(scroller).AnchorObject());
  // Scroll anchoring should not apply within main frame.
  EXPECT_EQ(0, LayoutViewport()->GetScrollOffset().Height());
  EXPECT_EQ(nullptr, GetScrollAnchor(LayoutViewport()).AnchorObject());
}

// This test verifies that scroll anchoring is disabled when the document is in
// printing mode.
TEST_P(ScrollAnchorTest, AnchoringDisabledForPrinting) {
  SetBodyInnerHTML(
      "<style> body { height: 1000px } div { height: 100px } </style>"
      "<div id='block1'>abc</div>"
      "<div id='block2'>def</div>");

  ScrollableArea* viewport = LayoutViewport();
  ScrollLayoutViewport(ScrollOffset(0, 150));

  // This will trigger printing and layout.
  PrintContext::NumberOfPages(GetDocument().GetFrame(), FloatSize(500, 500));

  EXPECT_EQ(150, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(nullptr, GetScrollAnchor(viewport).AnchorObject());
}

class ScrollAnchorCornerTest : public ScrollAnchorTest {
 protected:
  void CheckCorner(Corner corner,
                   ScrollOffset start_offset,
                   ScrollOffset expected_adjustment) {
    ScrollableArea* viewport = LayoutViewport();
    Element* element = GetDocument().GetElementById("changer");

    viewport->SetScrollOffset(start_offset, kUserScroll);
    element->setAttribute(HTMLNames::classAttr, "change");
    Update();

    ScrollOffset end_pos = start_offset;
    end_pos += expected_adjustment;

    EXPECT_EQ(end_pos, viewport->GetScrollOffset());
    EXPECT_EQ(GetDocument().GetElementById("a")->GetLayoutObject(),
              GetScrollAnchor(viewport).AnchorObject());
    EXPECT_EQ(corner, GetScrollAnchor(viewport).GetCorner());

    element->removeAttribute(HTMLNames::classAttr);
    Update();
  }
};

// Verify that we anchor to the top left corner of an element for LTR.
TEST_P(ScrollAnchorCornerTest, CornersLTR) {
  SetBodyInnerHTML(
      "<style>"
      "    body { position: relative; width: 1220px; height: 920px; }"
      "    #a { width: 400px; height: 300px; }"
      "    .change { height: 100px; }"
      "</style>"
      "<div id='changer'></div>"
      "<div id='a'></div>");

  CheckCorner(Corner::kTopLeft, ScrollOffset(20, 20), ScrollOffset(0, 100));
}

// Verify that we anchor to the top left corner of an anchor element for
// vertical-lr writing mode.
TEST_P(ScrollAnchorCornerTest, CornersVerticalLR) {
  SetBodyInnerHTML(
      "<style>"
      "    html { writing-mode: vertical-lr; }"
      "    body { position: relative; width: 1220px; height: 920px; }"
      "    #a { width: 400px; height: 300px; }"
      "    .change { width: 100px; }"
      "</style>"
      "<div id='changer'></div>"
      "<div id='a'></div>");

  CheckCorner(Corner::kTopLeft, ScrollOffset(20, 20), ScrollOffset(100, 0));
}

// Verify that we anchor to the top right corner of an anchor element for RTL.
TEST_P(ScrollAnchorCornerTest, CornersRTL) {
  SetBodyInnerHTML(
      "<style>"
      "    html { direction: rtl; }"
      "    body { position: relative; width: 1220px; height: 920px; }"
      "    #a { width: 400px; height: 300px; }"
      "    .change { height: 100px; }"
      "</style>"
      "<div id='changer'></div>"
      "<div id='a'></div>");

  CheckCorner(Corner::kTopRight, ScrollOffset(-20, 20), ScrollOffset(0, 100));
}

// Verify that we anchor to the top right corner of an anchor element for
// vertical-lr writing mode.
TEST_P(ScrollAnchorCornerTest, CornersVerticalRL) {
  SetBodyInnerHTML(
      "<style>"
      "    html { writing-mode: vertical-rl; }"
      "    body { position: relative; width: 1220px; height: 920px; }"
      "    #a { width: 400px; height: 300px; }"
      "    .change { width: 100px; }"
      "</style>"
      "<div id='changer'></div>"
      "<div id='a'></div>");

  CheckCorner(Corner::kTopRight, ScrollOffset(-20, 20), ScrollOffset(-100, 0));
}

TEST_P(ScrollAnchorTest, IgnoreNonBlockLayoutAxis) {
  SetBodyInnerHTML(
      "<style>"
      "    body {"
      "        margin: 0; line-height: 0;"
      "        width: 1200px; height: 1200px;"
      "    }"
      "    div {"
      "        width: 100px; height: 100px;"
      "        border: 5px solid gray;"
      "        display: inline-block;"
      "        box-sizing: border-box;"
      "    }"
      "</style>"
      "<div id='a'></div><br>"
      "<div id='b'></div><div id='c'></div>");

  ScrollableArea* viewport = LayoutViewport();
  ScrollLayoutViewport(ScrollOffset(150, 0));

  Element* a = GetDocument().GetElementById("a");
  Element* b = GetDocument().GetElementById("b");
  Element* c = GetDocument().GetElementById("c");

  a->setAttribute(HTMLNames::styleAttr, "height: 150px");
  Update();
  EXPECT_EQ(ScrollOffset(150, 0), viewport->GetScrollOffset());
  EXPECT_EQ(nullptr, GetScrollAnchor(viewport).AnchorObject());

  ScrollLayoutViewport(ScrollOffset(0, 50));

  a->setAttribute(HTMLNames::styleAttr, "height: 200px");
  b->setAttribute(HTMLNames::styleAttr, "width: 150px");
  Update();
  EXPECT_EQ(ScrollOffset(150, 100), viewport->GetScrollOffset());
  EXPECT_EQ(c->GetLayoutObject(), GetScrollAnchor(viewport).AnchorObject());

  a->setAttribute(HTMLNames::styleAttr, "height: 100px");
  b->setAttribute(HTMLNames::styleAttr, "width: 100px");
  GetDocument().documentElement()->setAttribute(HTMLNames::styleAttr,
                                                "writing-mode: vertical-rl");
  GetDocument().scrollingElement()->setScrollLeft(0);
  GetDocument().scrollingElement()->setScrollTop(0);
  ScrollLayoutViewport(ScrollOffset(0, 150));

  a->setAttribute(HTMLNames::styleAttr, "width: 150px");
  Update();
  EXPECT_EQ(ScrollOffset(0, 150), viewport->GetScrollOffset());
  EXPECT_EQ(nullptr, GetScrollAnchor(viewport).AnchorObject());

  ScrollLayoutViewport(ScrollOffset(-50, 0));

  a->setAttribute(HTMLNames::styleAttr, "width: 200px");
  b->setAttribute(HTMLNames::styleAttr, "height: 150px");
  Update();
  EXPECT_EQ(ScrollOffset(-100, 150), viewport->GetScrollOffset());
  EXPECT_EQ(c->GetLayoutObject(), GetScrollAnchor(viewport).AnchorObject());
}
}
