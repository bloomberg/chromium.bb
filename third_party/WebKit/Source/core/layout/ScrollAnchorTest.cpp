// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ScrollAnchor.h"

#include "core/dom/ClientRect.h"
#include "core/frame/VisualViewport.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/testing/HistogramTester.h"

namespace blink {

using Corner = ScrollAnchor::Corner;

class ScrollAnchorTest : public RenderingTest {
public:
    ScrollAnchorTest() { RuntimeEnabledFeatures::setScrollAnchoringEnabled(true); }
    ~ScrollAnchorTest() { RuntimeEnabledFeatures::setScrollAnchoringEnabled(false); }

protected:
    void update()
    {
        // TODO(skobes): Use SimTest instead of RenderingTest and move into Source/web?
        document().view()->updateAllLifecyclePhases();
    }

    ScrollableArea* layoutViewport()
    {
        return document().view()->layoutViewportScrollableArea();
    }

    VisualViewport& visualViewport()
    {
        return document().view()->page()->frameHost().visualViewport();
    }

    ScrollableArea* scrollerForElement(Element* element)
    {
        return toLayoutBox(element->layoutObject())->getScrollableArea();
    }

    ScrollAnchor& scrollAnchor(ScrollableArea* scroller)
    {
        if (scroller->isFrameView())
            return toFrameView(scroller)->scrollAnchor();
        ASSERT(scroller->isPaintLayerScrollableArea());
        return toPaintLayerScrollableArea(scroller)->scrollAnchor();
    }

    void setHeight(Element* element, int height)
    {
        element->setAttribute(HTMLNames::styleAttr,
            AtomicString(String::format("height: %dpx", height)));
        update();
    }

    void scrollLayoutViewport(DoubleSize delta)
    {
        Element* scrollingElement = document().scrollingElement();
        if (delta.width())
            scrollingElement->setScrollTop(scrollingElement->scrollLeft() + delta.width());
        if (delta.height())
            scrollingElement->setScrollTop(scrollingElement->scrollTop() + delta.height());
    }
};

// TODO(ymalik): Currently, this should be the first test in the file to avoid
// failure when running with other tests. Dig into this more and fix.
TEST_F(ScrollAnchorTest, UMAMetricUpdated)
{
    HistogramTester histogramTester;
    setBodyInnerHTML(
        "<style> body { height: 1000px } div { height: 100px } </style>"
        "<div id='block1'>abc</div>"
        "<div id='block2'>def</div>");

    ScrollableArea* viewport = layoutViewport();

    // Scroll position not adjusted, metric not updated.
    scrollLayoutViewport(DoubleSize(0, 150));
    histogramTester.expectTotalCount(
        "Layout.ScrollAnchor.AdjustedScrollOffset", 0);

    // Height changed, verify metric updated once.
    setHeight(document().getElementById("block1"), 200);
    histogramTester.expectUniqueSample(
        "Layout.ScrollAnchor.AdjustedScrollOffset", 1, 1);

    EXPECT_EQ(250, viewport->scrollPosition().y());
    EXPECT_EQ(document().getElementById("block2")->layoutObject(),
        scrollAnchor(viewport).anchorObject());
}

TEST_F(ScrollAnchorTest, Basic)
{
    setBodyInnerHTML(
        "<style> body { height: 1000px } div { height: 100px } </style>"
        "<div id='block1'>abc</div>"
        "<div id='block2'>def</div>");

    ScrollableArea* viewport = layoutViewport();

    // No anchor at origin (0,0).
    EXPECT_EQ(nullptr, scrollAnchor(viewport).anchorObject());

    scrollLayoutViewport(DoubleSize(0, 150));
    setHeight(document().getElementById("block1"), 200);

    EXPECT_EQ(250, viewport->scrollPosition().y());
    EXPECT_EQ(document().getElementById("block2")->layoutObject(),
        scrollAnchor(viewport).anchorObject());

    // ScrollableArea::userScroll should clear the anchor.
    viewport->userScroll(ScrollByPrecisePixel, FloatSize(0, 100));
    EXPECT_EQ(nullptr, scrollAnchor(viewport).anchorObject());
}

TEST_F(ScrollAnchorTest, VisualViewportAnchors)
{
    setBodyInnerHTML(
        "<style>"
        "    * { font-size: 1.2em; font-family: sans-serif; }"
        "    div { height: 100px; width: 20px; background-color: pink; }"
        "</style>"
        "<div id='div'></div>"
        "<div id='text'><b>This is a scroll anchoring test</div>");

    ScrollableArea* lViewport = layoutViewport();
    VisualViewport& vViewport = visualViewport();

    vViewport.setScale(2.0);

    // No anchor at origin (0,0).
    EXPECT_EQ(nullptr, scrollAnchor(lViewport).anchorObject());

    // Scroll the visual viewport to bring #text to the top.
    int top = document().getElementById("text")->getBoundingClientRect()->top();
    vViewport.setLocation(FloatPoint(0, top));

    setHeight(document().getElementById("div"), 10);
    EXPECT_EQ(document().getElementById("text")->layoutObject(),
        scrollAnchor(lViewport).anchorObject());
    EXPECT_EQ(top - 90, vViewport.scrollPosition().y());

    setHeight(document().getElementById("div"), 100);
    EXPECT_EQ(document().getElementById("text")->layoutObject(),
        scrollAnchor(lViewport).anchorObject());
    EXPECT_EQ(top, vViewport.scrollPosition().y());

    // Scrolling the visual viewport should clear the anchor.
    vViewport.setLocation(FloatPoint(0, 0));
    EXPECT_EQ(nullptr, scrollAnchor(lViewport).anchorObject());
}

// Test that we ignore the clipped content when computing visibility otherwise
// we may end up with an anchor that we think is in the viewport but is not.
TEST_F(ScrollAnchorTest, ClippedScrollersSkipped)
{
    setBodyInnerHTML(
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

    ScrollableArea* scroller = scrollerForElement(document().getElementById("scroller"));
    ScrollableArea* viewport = layoutViewport();

    document().getElementById("scroller")->setScrollTop(100);
    scrollLayoutViewport(DoubleSize(0, 350));

    setHeight(document().getElementById("innerChanger"), 200);
    setHeight(document().getElementById("outerChanger"), 150);

    EXPECT_EQ(300, scroller->scrollPosition().y());
    EXPECT_EQ(document().getElementById("innerAnchor")->layoutObject(),
        scrollAnchor(scroller).anchorObject());
    EXPECT_EQ(500, viewport->scrollPosition().y());
    EXPECT_EQ(document().getElementById("outerAnchor")->layoutObject(),
        scrollAnchor(viewport).anchorObject());
}

// Test that scroll anchoring causes no visible jump when a layout change
// (such as removal of a DOM element) changes the scroll bounds.
TEST_F(ScrollAnchorTest, AnchoringWhenContentRemoved)
{
    setBodyInnerHTML(
        "<style>"
        "    #changer { height: 1500px; }"
        "    #anchor {"
        "        width: 150px; height: 1000px; background-color: pink;"
        "    }"
        "</style>"
        "<div id='changer'></div>"
        "<div id='anchor'></div>");

    ScrollableArea* viewport = layoutViewport();
    scrollLayoutViewport(DoubleSize(0, 1600));

    setHeight(document().getElementById("changer"), 0);

    EXPECT_EQ(100, viewport->scrollPosition().y());
    EXPECT_EQ(document().getElementById("anchor")->layoutObject(),
        scrollAnchor(viewport).anchorObject());
}

// Test that scroll anchoring causes no visible jump when a layout change
// (such as removal of a DOM element) changes the scroll bounds of a scrolling
// div.
TEST_F(ScrollAnchorTest, AnchoringWhenContentRemovedFromScrollingDiv)
{
    setBodyInnerHTML(
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

    ScrollableArea* scroller = scrollerForElement(document().getElementById("scroller"));

    document().getElementById("scroller")->setScrollTop(1600);

    setHeight(document().getElementById("changer"), 0);

    EXPECT_EQ(100, scroller->scrollPosition().y());
    EXPECT_EQ(document().getElementById("anchor")->layoutObject(),
        scrollAnchor(scroller).anchorObject());
}

TEST_F(ScrollAnchorTest, FractionalOffsetsAreRoundedBeforeComparing)
{
    setBodyInnerHTML(
        "<style> body { height: 1000px } </style>"
        "<div id='block1' style='height: 50.4px'>abc</div>"
        "<div id='block2' style='height: 100px'>def</div>");

    ScrollableArea* viewport = layoutViewport();
    scrollLayoutViewport(DoubleSize(0, 100));

    document().getElementById("block1")->setAttribute(
        HTMLNames::styleAttr, "height: 50.6px");
    update();

    EXPECT_EQ(101, viewport->scrollPosition().y());
}

TEST_F(ScrollAnchorTest, AnchorWithLayerInScrollingDiv)
{
    setBodyInnerHTML(
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

    ScrollableArea* scroller = scrollerForElement(document().getElementById("scroller"));
    Element* block1 = document().getElementById("block1");
    Element* block2 = document().getElementById("block2");

    scroller->scrollBy(DoubleSize(0, 150), UserScroll);

    // In this layout pass we will anchor to #block2 which has its own PaintLayer.
    setHeight(block1, 200);
    EXPECT_EQ(250, scroller->scrollPosition().y());
    EXPECT_EQ(block2->layoutObject(), scrollAnchor(scroller).anchorObject());

    // Test that the anchor object can be destroyed without affecting the scroll position.
    block2->remove();
    update();
    EXPECT_EQ(250, scroller->scrollPosition().y());
}

TEST_F(ScrollAnchorTest, ExcludeAnonymousCandidates)
{
    setBodyInnerHTML(
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
        "</div>");

    ScrollableArea* viewport = layoutViewport();
    Element* inlineElem = document().getElementById("inline");
    EXPECT_TRUE(inlineElem->layoutObject()->parent()->isAnonymous());

    // Scroll #div into view, making anonymous block a viable candidate.
    document().getElementById("div")->scrollIntoView();

    // Trigger layout and verify that we don't anchor to the anonymous block.
    document().getElementById("div")->setAttribute(HTMLNames::styleAttr,
        AtomicString("top: 6px"));
    update();
    EXPECT_EQ(inlineElem->layoutObject()->slowFirstChild(),
        scrollAnchor(viewport).anchorObject());
}

TEST_F(ScrollAnchorTest, FullyContainedInlineBlock)
{
    // Exercises every WalkStatus value:
    // html, body -> Constrain
    // #outer -> Continue
    // #ib1, br -> Skip
    // #ib2 -> Return
    setBodyInnerHTML(
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

    scrollLayoutViewport(DoubleSize(0, 150));

    Element* ib2 = document().getElementById("ib2");
    ib2->setAttribute(HTMLNames::styleAttr, "line-height: 150px");
    update();
    EXPECT_EQ(ib2->layoutObject(), scrollAnchor(layoutViewport()).anchorObject());
}

TEST_F(ScrollAnchorTest, TextBounds)
{
    setBodyInnerHTML(
        "<style>"
        "    body {"
        "        position: absolute;"
        "        font-size: 100px;"
        "        width: 200px;"
        "        height: 1000px;"
        "        line-height: 100px;"
        "    }"
        "</style>"
        "abc <b id=b>def</b> ghi");

    scrollLayoutViewport(DoubleSize(0, 150));

    setHeight(document().body(), 1100);
    EXPECT_EQ(document().getElementById("b")->layoutObject()->slowFirstChild(),
        scrollAnchor(layoutViewport()).anchorObject());
}

TEST_F(ScrollAnchorTest, ExcludeFixedPosition)
{
    setBodyInnerHTML(
        "<style>"
        "    body { height: 1000px; padding: 20px; }"
        "    div { position: relative; top: 100px; }"
        "    #f { position: fixed }"
        "</style>"
        "<div id=f>fixed</div>"
        "<div id=c>content</div>");

    scrollLayoutViewport(DoubleSize(0, 50));

    setHeight(document().body(), 1100);
    EXPECT_EQ(document().getElementById("c")->layoutObject(),
        scrollAnchor(layoutViewport()).anchorObject());
}

// This test verifies that position:absolute elements that stick to the viewport
// are not selected as anchors.
TEST_F(ScrollAnchorTest, ExcludeAbsolutePositionThatSticksToViewport)
{
    setBodyInnerHTML(
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
        "</div></div>");

    Element* scrollerElement = document().getElementById("scroller");
    ScrollableArea* scroller = scrollerForElement(scrollerElement);
    Element* absPos = document().getElementById("abs");
    Element* relPos = document().getElementById("rel");

    scroller->scrollBy(DoubleSize(0, 25), UserScroll);
    setHeight(relPos, 100);

    // When the scroller is position:static, the anchor cannot be position:absolute.
    EXPECT_EQ(relPos->layoutObject(), scrollAnchor(scroller).anchorObject());

    scrollerElement->setAttribute(HTMLNames::styleAttr, "position: relative");
    scroller->scrollBy(DoubleSize(0, 25), UserScroll);
    setHeight(relPos, 125);

    // When the scroller is position:relative, the anchor may be position:absolute.
    EXPECT_EQ(absPos->layoutObject(), scrollAnchor(scroller).anchorObject());
}

// This test verifies that position:absolute elements with top/right/bottom/left
// set are not selected as anchors.
TEST_F(ScrollAnchorTest, ExcludeOffsettedAbsolutePosition)
{
    setBodyInnerHTML(
        "<style>"
        "    body { margin: 0; }"
        "    #scroller { overflow: scroll; width: 500px; height: 400px; position:relative; }"
        "    #space { height: 1000px; }"
        "    #abs {"
        "        position: absolute; background-color: red;"
        "        width: 100px; height: 100px;"
        "    }"
        "    #rel {"
        "        position: relative; background-color: green;"
        "        left: 50px; top: 100px; width: 100px; height: 75px;"
        "    }"
        "    .top { top: 10px }"
        "    .left { left: 10px }"
        "    .right { right: 10px }"
        "    .bottom { bottom: 10px }"
        "</style>"
        "<div id='scroller'><div id='space'>"
        "    <div id='abs'></div>"
        "    <div id='rel'></div>"
        "</div></div>");

    Element* scrollerElement = document().getElementById("scroller");
    ScrollableArea* scroller = scrollerForElement(scrollerElement);
    Element* relPos = document().getElementById("rel");
    Element* absPos = document().getElementById("abs");

    scroller->scrollBy(DoubleSize(0, 25), UserScroll);
    setHeight(relPos, 100);
    // Pick absolute anchor.
    EXPECT_EQ(absPos->layoutObject(), scrollAnchor(scroller).anchorObject());

    absPos->setAttribute(HTMLNames::classAttr, "top");
    scroller->scrollBy(DoubleSize(0, 25), UserScroll);
    setHeight(relPos, 125);
    // Don't pick absolute anchor since top is set.
    EXPECT_EQ(relPos->layoutObject(), scrollAnchor(scroller).anchorObject());

    absPos->removeAttribute(HTMLNames::classAttr);
    absPos->setAttribute(HTMLNames::classAttr, "right");
    scroller->scrollBy(DoubleSize(0, 25), UserScroll);
    setHeight(relPos, 150);
    // Don't pick absolute anchor since right is set.
    EXPECT_EQ(relPos->layoutObject(), scrollAnchor(scroller).anchorObject());

    absPos->removeAttribute(HTMLNames::classAttr);
    absPos->setAttribute(HTMLNames::classAttr, "bottom");
    scroller->scrollBy(DoubleSize(0, 25), UserScroll);
    setHeight(relPos, 175);
    // Don't pick absolute anchor since bottom is set.
    EXPECT_EQ(relPos->layoutObject(), scrollAnchor(scroller).anchorObject());

    absPos->removeAttribute(HTMLNames::classAttr);
    absPos->setAttribute(HTMLNames::classAttr, "left");
    scroller->scrollBy(DoubleSize(0, 25), UserScroll);
    setHeight(relPos, 200);
    // Don't pick absolute anchor since left is set.
    EXPECT_EQ(relPos->layoutObject(), scrollAnchor(scroller).anchorObject());
}

// Test that we descend into zero-height containers that have overflowing content.
TEST_F(ScrollAnchorTest, DescendsIntoContainerWithOverflow)
{
    setBodyInnerHTML(
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

    ScrollableArea* viewport = layoutViewport();

    scrollLayoutViewport(DoubleSize(0, 200));
    setHeight(document().getElementById("changer"), 200);

    EXPECT_EQ(300, viewport->scrollPosition().y());
    EXPECT_EQ(document().getElementById("bottom")->layoutObject(),
        scrollAnchor(viewport).anchorObject());
}

// Test that we descend into zero-height containers that have floating content.
TEST_F(ScrollAnchorTest, DescendsIntoContainerWithFloat)
{
    setBodyInnerHTML(
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
        "</div>");

    EXPECT_EQ(0, toLayoutBox(document().getElementById("zeroheight")->layoutObject())->size().height());

    ScrollableArea* viewport = layoutViewport();

    scrollLayoutViewport(DoubleSize(0, 200));
    setHeight(document().getElementById("float"), 600);

    EXPECT_EQ(200, viewport->scrollPosition().y());
    EXPECT_EQ(document().getElementById("float")->layoutObject(),
        scrollAnchor(viewport).anchorObject());
}

// Test then an element and its children are not selected as the anchor when
// it has the overflow-anchor property set to none.
TEST_F(ScrollAnchorTest, OptOutElement)
{
    setBodyInnerHTML(
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

    ScrollableArea* viewport = layoutViewport();
    scrollLayoutViewport(DoubleSize(0, 50));

    // No opt-out.
    setHeight(document().getElementById("changer"), 100);
    EXPECT_EQ(150, viewport->scrollPosition().y());
    EXPECT_EQ(document().getElementById("innerDiv")->layoutObject(),
        scrollAnchor(viewport).anchorObject());

    // Clear anchor and opt-out element.
    scrollLayoutViewport(DoubleSize(0, 10));
    document().getElementById("firstDiv")->setAttribute(HTMLNames::styleAttr,
        AtomicString("overflow-anchor: none"));
    update();

    // Opted out element and it's children skipped.
    setHeight(document().getElementById("changer"), 200);
    EXPECT_EQ(260, viewport->scrollPosition().y());
    EXPECT_EQ(document().getElementById("secondDiv")->layoutObject(),
        scrollAnchor(viewport).anchorObject());
}

TEST_F(ScrollAnchorTest, OptOutBody)
{
    setBodyInnerHTML(
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

    ScrollableArea* scroller = scrollerForElement(document().getElementById("scroller"));
    ScrollableArea* viewport = layoutViewport();

    document().getElementById("scroller")->setScrollTop(100);
    scrollLayoutViewport(DoubleSize(0, 100));

    setHeight(document().getElementById("innerChanger"), 200);
    setHeight(document().getElementById("outerChanger"), 150);

    // Scroll anchoring should apply within #scroller.
    EXPECT_EQ(300, scroller->scrollPosition().y());
    EXPECT_EQ(document().getElementById("innerAnchor")->layoutObject(),
        scrollAnchor(scroller).anchorObject());
    // Scroll anchoring should not apply within main frame.
    EXPECT_EQ(100, viewport->scrollPosition().y());
    EXPECT_EQ(nullptr, scrollAnchor(viewport).anchorObject());
}

TEST_F(ScrollAnchorTest, OptOutScrollingDiv)
{
    setBodyInnerHTML(
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

    ScrollableArea* scroller = scrollerForElement(document().getElementById("scroller"));
    ScrollableArea* viewport = layoutViewport();

    document().getElementById("scroller")->setScrollTop(100);
    scrollLayoutViewport(DoubleSize(0, 100));

    setHeight(document().getElementById("innerChanger"), 200);
    setHeight(document().getElementById("outerChanger"), 150);

    // Scroll anchoring should not apply within #scroller.
    EXPECT_EQ(100, scroller->scrollPosition().y());
    EXPECT_EQ(nullptr, scrollAnchor(scroller).anchorObject());
    // Scroll anchoring should apply within main frame.
    EXPECT_EQ(250, viewport->scrollPosition().y());
    EXPECT_EQ(document().getElementById("outerAnchor")->layoutObject(),
        scrollAnchor(viewport).anchorObject());
}

class ScrollAnchorCornerTest : public ScrollAnchorTest {
protected:
    void checkCorner(Corner corner, DoublePoint startPos, DoubleSize expectedAdjustment)
    {
        ScrollableArea* viewport = layoutViewport();
        Element* element = document().getElementById("changer");

        viewport->setScrollPosition(startPos, UserScroll);
        element->setAttribute(HTMLNames::classAttr, "change");
        update();

        DoublePoint endPos = startPos;
        endPos.move(expectedAdjustment);

        EXPECT_EQ(endPos, viewport->scrollPositionDouble());
        EXPECT_EQ(document().getElementById("a")->layoutObject(),
            scrollAnchor(viewport).anchorObject());
        EXPECT_EQ(corner, scrollAnchor(viewport).corner());

        element->removeAttribute(HTMLNames::classAttr);
        update();
    }
};

// Verify that we anchor to the top left corner of an element for LTR.
TEST_F(ScrollAnchorCornerTest, CornersLTR)
{
    setBodyInnerHTML(
        "<style>"
        "    body { position: relative; width: 1220px; height: 920px; }"
        "    #a { width: 400px; height: 300px; }"
        "    .change { height: 100px; }"
        "</style>"
        "<div id='changer'></div>"
        "<div id='a'></div>");

    checkCorner(Corner::TopLeft, DoublePoint(20,  20), DoubleSize(0, 100));
}

// Verify that we anchor to the top left corner of an anchor element for
// vertical-lr writing mode.
TEST_F(ScrollAnchorCornerTest, CornersVerticalLR)
{
    setBodyInnerHTML(
        "<style>"
        "    html { writing-mode: vertical-lr; }"
        "    body { position: relative; width: 1220px; height: 920px; }"
        "    #a { width: 400px; height: 300px; }"
        "    .change { width: 100px; }"
        "</style>"
        "<div id='changer'></div>"
        "<div id='a'></div>");

    checkCorner(Corner::TopLeft, DoublePoint(20,  20), DoubleSize(100, 0));
}

// Verify that we anchor to the top right corner of an anchor element for RTL.
TEST_F(ScrollAnchorCornerTest, CornersRTL)
{
    setBodyInnerHTML(
        "<style>"
        "    html { direction: rtl; }"
        "    body { position: relative; width: 1220px; height: 920px; }"
        "    #a { width: 400px; height: 300px; }"
        "    .change { height: 100px; }"
        "</style>"
        "<div id='changer'></div>"
        "<div id='a'></div>");

    checkCorner(Corner::TopRight, DoublePoint(-20,  20), DoubleSize(0, 100));
}

// Verify that we anchor to the top right corner of an anchor element for
// vertical-lr writing mode.
TEST_F(ScrollAnchorCornerTest, CornersVerticalRL)
{
    setBodyInnerHTML(
        "<style>"
        "    html { writing-mode: vertical-rl; }"
        "    body { position: relative; width: 1220px; height: 920px; }"
        "    #a { width: 400px; height: 300px; }"
        "    .change { width: 100px; }"
        "</style>"
        "<div id='changer'></div>"
        "<div id='a'></div>");

    checkCorner(Corner::TopRight, DoublePoint(-20,  20), DoubleSize(-100, 0));
}

}
