// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutObject.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutObjectTest : public RenderingTest {
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

TEST_F(LayoutObjectTest, MapToVisibleRectInAncestorSpace)
{
    setBodyInnerHTML(
        "<div id='container' style='overflow: scroll; will-change: transform; width: 50px; height: 50px'>"
        "  <span><img style='width: 20px; height: 100px'></span>"
        "  text text text text text text text"
        "</div>");
    LayoutBlock* container = toLayoutBlock(document().getElementById("container")->layoutObject());
    LayoutText* text = toLayoutText(container->lastChild());

    container->setScrollTop(LayoutUnit(50));
    LayoutRect rect(0, 60, 20, 20);
    text->mapToVisibleRectInAncestorSpace(container, rect, nullptr);
    EXPECT_TRUE(rect == LayoutRect(0, 10, 20, 20));
}

} // namespace blink
