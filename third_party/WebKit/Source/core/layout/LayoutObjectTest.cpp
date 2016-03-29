// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutObject.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutObjectTest : public RenderingTest {
public:
    LayoutObjectTest()
        : RenderingTest(SingleChildFrameLoaderClient::create()) {}
protected:
    LayoutView& layoutView() const { return *document().layoutView(); }
};

TEST_F(LayoutObjectTest, LayoutDecoratedNameCalledWithPositionedObject)
{
    setBodyInnerHTML("<div id='div' style='position: fixed'>test</div>");
    Element* div = document().getElementById(AtomicString("div"));
    ASSERT(div);
    LayoutObject* obj = div->layoutObject();
    ASSERT(obj);
    EXPECT_STREQ("LayoutBlockFlow (positioned)", obj->decoratedName().ascii().data());
}


// Some display checks.
TEST_F(LayoutObjectTest, DisplayNoneCreateObject)
{
    setBodyInnerHTML("<div style='display:none'></div>");
    EXPECT_EQ(nullptr, document().body()->firstChild()->layoutObject());
}

TEST_F(LayoutObjectTest, DisplayBlockCreateObject)
{
    setBodyInnerHTML("<foo style='display:block'></foo>");
    LayoutObject* layoutObject = document().body()->firstChild()->layoutObject();
    EXPECT_NE(nullptr, layoutObject);
    EXPECT_TRUE(layoutObject->isLayoutBlockFlow());
    EXPECT_FALSE(layoutObject->isInline());
}

TEST_F(LayoutObjectTest, DisplayInlineBlockCreateObject)
{
    setBodyInnerHTML("<foo style='display:inline-block'></foo>");
    LayoutObject* layoutObject = document().body()->firstChild()->layoutObject();
    EXPECT_NE(nullptr, layoutObject);
    EXPECT_TRUE(layoutObject->isLayoutBlockFlow());
    EXPECT_TRUE(layoutObject->isInline());
}



// Containing block test.
TEST_F(LayoutObjectTest, ContainingBlockLayoutViewShouldBeNull)
{
    EXPECT_EQ(nullptr, layoutView().containingBlock());
}

TEST_F(LayoutObjectTest, ContainingBlockBodyShouldBeDocumentElement)
{
    EXPECT_EQ(document().body()->layoutObject()->containingBlock(), document().documentElement()->layoutObject());
}

TEST_F(LayoutObjectTest, ContainingBlockDocumentElementShouldBeLayoutView)
{
    EXPECT_EQ(document().documentElement()->layoutObject()->containingBlock(), layoutView());
}

TEST_F(LayoutObjectTest, ContainingBlockStaticLayoutObjectShouldBeParent)
{
    setBodyInnerHTML("<foo style='position:static'></foo>");
    LayoutObject* bodyLayoutObject = document().body()->layoutObject();
    LayoutObject* layoutObject = bodyLayoutObject->slowFirstChild();
    EXPECT_EQ(layoutObject->containingBlock(), bodyLayoutObject);
}

TEST_F(LayoutObjectTest, ContainingBlockAbsoluteLayoutObjectShouldBeLayoutView)
{
    setBodyInnerHTML("<foo style='position:absolute'></foo>");
    LayoutObject* layoutObject = document().body()->layoutObject()->slowFirstChild();
    EXPECT_EQ(layoutObject->containingBlock(), layoutView());
}

TEST_F(LayoutObjectTest, ContainingBlockAbsoluteLayoutObjectShouldBeNonStaticallyPositionedBlockAncestor)
{
    setBodyInnerHTML("<div style='position:relative'><bar style='position:absolute'></bar></div>");
    LayoutObject* containingBlocklayoutObject = document().body()->layoutObject()->slowFirstChild();
    LayoutObject* layoutObject = containingBlocklayoutObject->slowFirstChild();
    EXPECT_EQ(layoutObject->containingBlock(), containingBlocklayoutObject);
}

TEST_F(LayoutObjectTest, ContainingBlockAbsoluteLayoutObjectShouldNotBeNonStaticlyPositionedInlineAncestor)
{
    setBodyInnerHTML("<span style='position:relative'><bar style='position:absolute'></bar></span>");
    LayoutObject* bodyLayoutObject = document().body()->layoutObject();
    LayoutObject* layoutObject = bodyLayoutObject->slowFirstChild()->slowFirstChild();

    // Sanity check: Make sure we don't generate anonymous objects.
    EXPECT_EQ(nullptr, bodyLayoutObject->slowFirstChild()->nextSibling());
    EXPECT_EQ(nullptr, layoutObject->slowFirstChild());
    EXPECT_EQ(nullptr, layoutObject->nextSibling());

    EXPECT_EQ(layoutObject->containingBlock(), bodyLayoutObject);
}

TEST_F(LayoutObjectTest, LayoutTextMapToVisibleRectInAncestorSpace)
{
    setBodyInnerHTML(
        "<style>body { margin: 0; }</style>"
        "<div id='container' style='overflow: scroll; width: 50px; height: 50px'>"
        "  <span><img style='width: 20px; height: 100px'></span>"
        "  text text text text text text text"
        "</div>");

    LayoutBlock* container = toLayoutBlock(getLayoutObjectByElementId("container"));
    LayoutText* text = toLayoutText(container->lastChild());

    container->setScrollTop(LayoutUnit(50));
    LayoutRect rect(0, 60, 20, 80);
    EXPECT_TRUE(text->mapToVisibleRectInAncestorSpace(container, rect));
    EXPECT_EQ(rect, LayoutRect(0, 10, 20, 80));

    rect = LayoutRect(0, 60, 80, 0);
    EXPECT_TRUE(text->mapToVisibleRectInAncestorSpace(container, rect, EdgeInclusive));
    EXPECT_EQ(rect, LayoutRect(0, 10, 80, 0));
}

TEST_F(LayoutObjectTest, LayoutInlineMapToVisibleRectInAncestorSpace)
{
    document().setBaseURLOverride(KURL(ParsedURLString, "http://test.com"));
    setBodyInnerHTML(
        "<style>body { margin: 0; }</style>"
        "<div id='container' style='overflow: scroll; width: 50px; height: 50px'>"
        "  <span><img style='width: 20px; height: 100px'></span>"
        "  <span id=leaf></span></div>");

    LayoutBlock* container = toLayoutBlock(getLayoutObjectByElementId("container"));
    LayoutObject* leaf = container->lastChild();

    container->setScrollTop(LayoutUnit(50));
    LayoutRect rect(0, 60, 20, 80);
    EXPECT_TRUE(leaf->mapToVisibleRectInAncestorSpace(container, rect));
    EXPECT_EQ(rect, LayoutRect(0, 10, 20, 80));

    rect = LayoutRect(0, 60, 80, 0);
    EXPECT_TRUE(leaf->mapToVisibleRectInAncestorSpace(container, rect, EdgeInclusive));
    EXPECT_EQ(rect, LayoutRect(0, 10, 80, 0));
}

TEST_F(LayoutObjectTest, LayoutViewMapToVisibleRectInAncestorSpace)
{
    document().setBaseURLOverride(KURL(ParsedURLString, "http://test.com"));
    setBodyInnerHTML(
        "<style>body { margin: 0; }</style>"
        "<div id=frameContainer>"
        "  <iframe id=frame src='http://test.com' width='50' height='50' frameBorder='0'></iframe>"
        "</div>");

    Document& frameDocument = setupChildIframe("frame", "<style>body { margin: 0; }</style><span><img style='width: 20px; height: 100px'></span>text text text");
    frameDocument.updateLayout();

    LayoutBlock* frameContainer = toLayoutBlock(getLayoutObjectByElementId("frameContainer"));
    LayoutBlock* frameBody = toLayoutBlock(frameDocument.body()->layoutObject());
    LayoutText* frameText = toLayoutText(frameBody->lastChild());

    // This case involves clipping: frame height is 50, y-coordinate of result rect is 13,
    // so height should be clipped to (50 - 13) == 37.
    frameDocument.view()->setScrollPosition(DoublePoint(0, 47), ProgrammaticScroll);
    LayoutRect rect(4, 60, 20, 80);
    EXPECT_TRUE(frameText->mapToVisibleRectInAncestorSpace(frameContainer, rect));
    EXPECT_EQ(rect, LayoutRect(4, 13, 20, 37));

    rect = LayoutRect(4, 60, 0, 80);
    EXPECT_TRUE(frameText->mapToVisibleRectInAncestorSpace(frameContainer, rect, EdgeInclusive));
    EXPECT_EQ(rect, LayoutRect(4, 13, 0, 37));
}

TEST_F(LayoutObjectTest, LayoutViewMapToVisibleRectInAncestorSpaceSubpixelRounding)
{
    document().setBaseURLOverride(KURL(ParsedURLString, "http://test.com"));
    setBodyInnerHTML(
        "<style>body { margin: 0; }</style>"
        "<div id=frameContainer style='position: relative; left: 0.5px'>"
        "  <iframe id=frame style='position: relative; left: 0.5px' src='http://test.com' width='200' height='200' frameBorder='0'></iframe>"
        "</div>");

    Document& frameDocument = setupChildIframe(
        "frame", "<style>body { margin: 0; }</style><div id='target' style='position: relative; width: 100px; height: 100px; left: 0.5px'>");
    frameDocument.updateLayout();

    LayoutBlock* frameContainer = toLayoutBlock(getLayoutObjectByElementId("frameContainer"));
    LayoutObject* target = frameDocument.getElementById("target")->layoutObject();
    LayoutRect rect(0, 0, 100, 100);
    EXPECT_TRUE(target->mapToVisibleRectInAncestorSpace(frameContainer, rect));
    // When passing from the iframe to the parent frame, the rect of (0.5, 0, 100, 100) is expanded to (0, 0, 100, 100), and then offset by
    // the 0.5 offset of frameContainer.
    EXPECT_EQ(LayoutRect(LayoutPoint(DoublePoint(0.5, 0)), LayoutSize(101, 100)), rect);
}

} // namespace blink
