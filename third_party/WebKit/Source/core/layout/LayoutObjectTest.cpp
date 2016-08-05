// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutObject.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutAPIShim.h"
#include "platform/JSONValues.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutObjectTest : public RenderingTest {
public:
    LayoutObjectTest()
        : RenderingTest(SingleChildFrameLoaderClient::create()) {}
protected:
    LayoutView& layoutView() const { return *toLayoutView(LayoutAPIShim::layoutObjectFrom(document().layoutViewItem())); }
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

TEST_F(LayoutObjectTest, PaintingLayerOfOverflowClipLayerUnderColumnSpanAll)
{
    setBodyInnerHTML(
        "<div id='columns' style='columns: 3'>"
        "  <div style='column-span: all'>"
        "    <div id='overflow-clip-layer' style='height: 100px; overflow: hidden'></div>"
        "  </div>"
        "</div>");

    LayoutObject* overflowClipObject = getLayoutObjectByElementId("overflow-clip-layer");
    LayoutBlock* columns = toLayoutBlock(getLayoutObjectByElementId("columns"));
    EXPECT_EQ(columns->layer(), overflowClipObject->paintingLayer());
}

TEST_F(LayoutObjectTest, TraverseNonCompositingDescendantsInPaintOrder)
{
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        return;

    enableCompositing();
    setBodyInnerHTML(
        "<style>div { width: 10px; height: 10px; background-color: green; }</style>"
        "<div id='container' style='position: fixed'>"
        "  <div id='normal-child'></div>"
        "  <div id='stacked-child' style='position: relative'></div>"
        "  <div id='composited-stacking-context' style='will-change: transform'>"
        "    <div id='normal-child-of-composited-stacking-context'></div>"
        "    <div id='stacked-child-of-composited-stacking-context' style='position: relative'></div>"
        "  </div>"
        "  <div id='composited-non-stacking-context' style='backface-visibility: hidden'>"
        "    <div id='normal-child-of-composited-non-stacking-context'></div>"
        "    <div id='stacked-child-of-composited-non-stacking-context' style='position: relative'></div>"
        "    <div id='non-stacked-layered-child-of-composited-non-stacking-context' style='overflow: scroll'></div>"
        "  </div>"
        "</div>");

    document().view()->setTracksPaintInvalidations(true);
    getLayoutObjectByElementId("container")->invalidateDisplayItemClientsIncludingNonCompositingDescendants(PaintInvalidationSubtree);
    std::unique_ptr<JSONArray> invalidations = document().view()->trackedObjectPaintInvalidationsAsJSON();
    document().view()->setTracksPaintInvalidations(false);

    ASSERT_EQ(4u, invalidations->size());
    String s;
    JSONObject::cast(invalidations->at(0))->get("object")->asString(&s);
    EXPECT_EQ(getLayoutObjectByElementId("container")->debugName(), s);
    JSONObject::cast(invalidations->at(1))->get("object")->asString(&s);
    EXPECT_EQ(getLayoutObjectByElementId("normal-child")->debugName(), s);
    JSONObject::cast(invalidations->at(2))->get("object")->asString(&s);
    EXPECT_EQ(getLayoutObjectByElementId("stacked-child")->debugName(), s);
    JSONObject::cast(invalidations->at(3))->get("object")->asString(&s);
    EXPECT_EQ(getLayoutObjectByElementId("stacked-child-of-composited-non-stacking-context")->debugName(), s);
}

} // namespace blink
