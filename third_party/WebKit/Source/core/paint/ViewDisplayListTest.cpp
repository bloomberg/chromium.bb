// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutText.h"
#include "core/layout/LayoutView.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/page/FocusController.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "core/paint/DeprecatedPaintLayerPainter.h"
#include "core/paint/LayerClipRecorder.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ScopeRecorder.h"
#include "core/paint/SubtreeRecorder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include <gtest/gtest.h>

namespace blink {

class ViewDisplayListTest : public RenderingTest {
public:
    ViewDisplayListTest()
        : m_layoutView(nullptr) { }

protected:
    LayoutView& layoutView() { return *m_layoutView; }
    DisplayItemList& rootDisplayItemList() { return *layoutView().layer()->graphicsLayerBacking()->displayItemList(); }
    const Vector<OwnPtr<DisplayItem>>& newPaintListBeforeUpdate() { return rootDisplayItemList().m_newPaints; }

private:
    virtual void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(true);

        RenderingTest::SetUp();
        enableCompositing();

        m_layoutView = document().view()->layoutView();
        ASSERT_TRUE(m_layoutView);
    }

    virtual void TearDown() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(false);
    }

    LayoutView* m_layoutView;
};

class TestDisplayItem : public DisplayItem {
public:
    TestDisplayItem(const DisplayItemClientWrapper& client, Type type) : DisplayItem(client, type) { }

    virtual void replay(GraphicsContext&) override final { ASSERT_NOT_REACHED(); }
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override final { ASSERT_NOT_REACHED(); }
};

#ifndef NDEBUG
#define TRACE_DISPLAY_ITEMS(i, expected, actual) \
    String trace = String::format("%d: ", (int)i) + "Expected: " + (expected).asDebugString() + " Actual: " + (actual).asDebugString(); \
    SCOPED_TRACE(trace.utf8().data());
#else
#define TRACE_DISPLAY_ITEMS(i, expected, actual)
#endif

#define EXPECT_DISPLAY_LIST(actual, expectedSize, ...) \
    do { \
        EXPECT_EQ((size_t)expectedSize, actual.size()); \
        if (expectedSize != actual.size()) \
            break; \
        const TestDisplayItem expected[] = { __VA_ARGS__ }; \
        for (size_t index = 0; index < std::min<size_t>(actual.size(), expectedSize); index++) { \
            TRACE_DISPLAY_ITEMS(index, expected[index], *actual[index]); \
            EXPECT_EQ(expected[index].client(), actual[index]->client()); \
            EXPECT_EQ(expected[index].type(), actual[index]->type()); \
        } \
    } while (false);

void drawRect(GraphicsContext& context, LayoutObject& layoutObject, PaintPhase phase, const FloatRect& bound)
{
    LayoutObjectDrawingRecorder drawingRecorder(context, layoutObject, phase, bound);
    if (drawingRecorder.canUseCachedDrawing())
        return;
    IntRect rect(0, 0, 10, 10);
    context.drawRect(rect);
}

void drawClippedRect(GraphicsContext& context, LayoutBoxModelObject& layoutObject, PaintPhase phase, const FloatRect& bound)
{
    LayoutRect rect(1, 1, 9, 9);
    ClipRect clipRect(rect);
    LayerClipRecorder layerClipRecorder(context, layoutObject, DisplayItem::ClipLayerForeground, clipRect, 0, LayoutPoint(), PaintLayerFlags());
    drawRect(context, layoutObject, phase, bound);
}

TEST_F(ViewDisplayListTest, NestedRecorders)
{
    GraphicsContext context(nullptr, &rootDisplayItemList());
    FloatRect bound = layoutView().viewRect();

    drawClippedRect(context, layoutView(), PaintPhaseForeground, bound);
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(layoutView(), DisplayItem::ClipLayerForeground),
        TestDisplayItem(layoutView(), DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(layoutView(), DisplayItem::clipTypeToEndClipType(DisplayItem::ClipLayerForeground)));
}

TEST_F(ViewDisplayListTest, UpdateBasic)
{
    setBodyInnerHTML("<div id='first'><div id='second'></div></div>");
    LayoutObject& first = *document().body()->firstChild()->layoutObject();
    LayoutObject& second = *document().body()->firstChild()->firstChild()->layoutObject();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 300, 300));
    drawRect(context, second, PaintPhaseChildBlockBackground, FloatRect(100, 100, 200, 200));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 300, 300));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseChildBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));

    rootDisplayItemList().invalidate(second.displayItemClient());
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 300, 300));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 300, 300));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));
}

TEST_F(ViewDisplayListTest, UpdateSwapOrder)
{
    setBodyInnerHTML("<div id='first'><div id='second'></div></div><div id='unaffected'></div>");
    LayoutObject& first = *document().body()->firstChild()->layoutObject();
    LayoutObject& second = *document().body()->firstChild()->firstChild()->layoutObject();
    LayoutObject& unaffected = *document().body()->firstChild()->nextSibling()->layoutObject();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(context, unaffected, PaintPhaseBlockBackground, FloatRect(300, 300, 10, 10));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(unaffected, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)));

    rootDisplayItemList().invalidate(second.displayItemClient());
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(context, unaffected, PaintPhaseBlockBackground, FloatRect(300, 300, 10, 10));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(unaffected, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)));
}

TEST_F(ViewDisplayListTest, UpdateNewItemInMiddle)
{
    setBodyInnerHTML("<div id='first'><div id='second'><div id='third'></div></div></div>");
    LayoutObject& first = *document().body()->firstChild()->layoutObject();
    LayoutObject& second = *document().body()->firstChild()->firstChild()->layoutObject();
    LayoutObject& third = *document().body()->firstChild()->firstChild()->firstChild()->layoutObject();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)));

    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(context, third, PaintPhaseBlockBackground, FloatRect(125, 100, 200, 50));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(third, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)));
}

TEST_F(ViewDisplayListTest, UpdateInvalidationWithPhases)
{
    setBodyInnerHTML("<div id='first'><div id='second'></div></div><div id='third'></div>");
    LayoutObject& first = *document().body()->firstChild()->layoutObject();
    LayoutObject& second = *document().body()->firstChild()->firstChild()->layoutObject();
    LayoutObject& third = *document().body()->firstChild()->nextSibling()->layoutObject();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(context, third, PaintPhaseBlockBackground, FloatRect(300, 100, 50, 50));
    drawRect(context, first, PaintPhaseForeground, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseForeground, FloatRect(100, 100, 50, 200));
    drawRect(context, third, PaintPhaseForeground, FloatRect(300, 100, 50, 50));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseOutline, FloatRect(100, 100, 50, 200));
    drawRect(context, third, PaintPhaseOutline, FloatRect(300, 100, 50, 50));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 9,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(third, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(third, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)),
        TestDisplayItem(third, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));

    rootDisplayItemList().invalidate(second.displayItemClient());
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(context, third, PaintPhaseBlockBackground, FloatRect(300, 100, 50, 50));
    drawRect(context, first, PaintPhaseForeground, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseForeground, FloatRect(100, 100, 50, 200));
    drawRect(context, third, PaintPhaseForeground, FloatRect(300, 100, 50, 50));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseOutline, FloatRect(100, 100, 50, 200));
    drawRect(context, third, PaintPhaseOutline, FloatRect(300, 100, 50, 50));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 9,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(third, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(third, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)),
        TestDisplayItem(third, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));
}

TEST_F(ViewDisplayListTest, UpdateAddFirstOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    LayoutObject& first = *document().body()->firstChild()->layoutObject();
    LayoutObject& second = *document().body()->firstChild()->nextSibling()->layoutObject();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));

    rootDisplayItemList().invalidate(first.displayItemClient());
    rootDisplayItemList().invalidate(second.displayItemClient());
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));

    rootDisplayItemList().invalidate(first.displayItemClient());
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));
}

TEST_F(ViewDisplayListTest, UpdateAddLastOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    LayoutObject& first = *document().body()->firstChild()->layoutObject();
    LayoutObject& second = *document().body()->firstChild()->nextSibling()->layoutObject();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));

    rootDisplayItemList().invalidate(first.displayItemClient());
    rootDisplayItemList().invalidate(second.displayItemClient());
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));

    rootDisplayItemList().invalidate(first.displayItemClient());
    rootDisplayItemList().invalidate(second.displayItemClient());
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));
}

TEST_F(ViewDisplayListTest, UpdateClip)
{
    setBodyInnerHTML("<div id='first'><div id='second'></div></div>");
    LayoutBoxModelObject& firstLayoutObject = *toLayoutBoxModelObject(document().body()->firstChild()->layoutObject());
    LayoutBoxModelObject& secondLayoutObject = *toLayoutBoxModelObject(document().body()->firstChild()->firstChild()->layoutObject());
    GraphicsContext context(nullptr, &rootDisplayItemList());

    ClipRect firstClipRect(LayoutRect(1, 1, 2, 2));
    {
        LayerClipRecorder layerClipRecorder(context, firstLayoutObject, DisplayItem::ClipLayerForeground, firstClipRect, 0, LayoutPoint(), PaintLayerFlags());
        drawRect(context, firstLayoutObject, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
        drawRect(context, secondLayoutObject, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    }
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(firstLayoutObject, DisplayItem::ClipLayerForeground),
        TestDisplayItem(firstLayoutObject, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(secondLayoutObject, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(firstLayoutObject, DisplayItem::clipTypeToEndClipType(DisplayItem::ClipLayerForeground)));

    rootDisplayItemList().invalidate(firstLayoutObject.displayItemClient());
    drawRect(context, firstLayoutObject, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, secondLayoutObject, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(firstLayoutObject, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(secondLayoutObject, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)));

    rootDisplayItemList().invalidate(secondLayoutObject.displayItemClient());
    drawRect(context, firstLayoutObject, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    ClipRect secondClipRect(LayoutRect(1, 1, 2, 2));
    {
        LayerClipRecorder layerClipRecorder(context, secondLayoutObject, DisplayItem::ClipLayerForeground, secondClipRect, 0, LayoutPoint(), PaintLayerFlags());
        drawRect(context, secondLayoutObject, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    }
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(firstLayoutObject, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(secondLayoutObject, DisplayItem::ClipLayerForeground),
        TestDisplayItem(secondLayoutObject, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(secondLayoutObject, DisplayItem::clipTypeToEndClipType(DisplayItem::ClipLayerForeground)));
}

TEST_F(ViewDisplayListTest, CachedDisplayItems)
{
    setBodyInnerHTML("<div id='first'><div id='second'></div></div>");
    LayoutBoxModelObject& firstLayoutObject = *toLayoutBoxModelObject(document().body()->firstChild()->layoutObject());
    LayoutBoxModelObject& secondLayoutObject = *toLayoutBoxModelObject(document().body()->firstChild()->firstChild()->layoutObject());
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(context, firstLayoutObject, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, secondLayoutObject, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(firstLayoutObject, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(secondLayoutObject, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)));
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(firstLayoutObject.displayItemClient()));
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(secondLayoutObject.displayItemClient()));
    DisplayItem* firstDisplayItem = rootDisplayItemList().paintList()[0].get();
    DisplayItem* secondDisplayItem = rootDisplayItemList().paintList()[1].get();

    rootDisplayItemList().invalidate(firstLayoutObject.displayItemClient());
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(firstLayoutObject.displayItemClient()));
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(secondLayoutObject.displayItemClient()));

    drawRect(context, firstLayoutObject, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, secondLayoutObject, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(firstLayoutObject, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(secondLayoutObject, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)));
    // The first display item should be updated.
    EXPECT_NE(firstDisplayItem, rootDisplayItemList().paintList()[0].get());
    // The second display item should be cached.
    EXPECT_EQ(secondDisplayItem, rootDisplayItemList().paintList()[1].get());
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(firstLayoutObject.displayItemClient()));
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(secondLayoutObject.displayItemClient()));

    rootDisplayItemList().invalidateAll();
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(firstLayoutObject.displayItemClient()));
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(secondLayoutObject.displayItemClient()));
}

TEST_F(ViewDisplayListTest, FullDocumentPaintingWithCaret)
{
    setBodyInnerHTML("<div id='div' contentEditable='true' style='outline:none'>XYZ</div>");
    document().page()->focusController().setActive(true);
    document().page()->focusController().setFocused(true);
    LayoutView& layoutView = *document().layoutView();
    DeprecatedPaintLayer& rootLayer = *layoutView.layer();
    LayoutObject& htmlLayoutObject = *document().documentElement()->layoutObject();
    Element& div = *toElement(document().body()->firstChild());
    LayoutObject& divLayoutObject = *document().body()->firstChild()->layoutObject();
    InlineTextBox& textInlineBox = *toLayoutText(div.firstChild()->layoutObject())->firstTextBox();

    GraphicsContext context(nullptr, &rootDisplayItemList());
    DeprecatedPaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 800, 600), PaintBehaviorNormal, LayoutSize());
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(htmlLayoutObject, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(textInlineBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)));

    div.focus();
    document().view()->updateLayoutAndStyleForPainting();
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(htmlLayoutObject.displayItemClient()));
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(divLayoutObject.displayItemClient()));
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(textInlineBox.displayItemClient()));
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(htmlLayoutObject, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(textInlineBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(divLayoutObject, DisplayItem::Caret)); // New!
}

TEST_F(ViewDisplayListTest, ComplexUpdateSwapOrder)
{
    setBodyInnerHTML("<div id='container1'><div id='content1'></div></div>"
        "<div id='container2'><div id='content2'></div></div>");
    LayoutObject& container1 = *document().body()->firstChild()->layoutObject();
    LayoutObject& content1 = *document().body()->firstChild()->firstChild()->layoutObject();
    LayoutObject& container2 = *document().body()->firstChild()->nextSibling()->layoutObject();
    LayoutObject& content2 = *document().body()->firstChild()->nextSibling()->firstChild()->layoutObject();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(context, container1, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(context, content1, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(context, content1, PaintPhaseForeground, FloatRect(100, 100, 50, 200));
    drawRect(context, container1, PaintPhaseForeground, FloatRect(100, 100, 100, 100));
    drawRect(context, container2, PaintPhaseBlockBackground, FloatRect(100, 200, 100, 100));
    drawRect(context, content2, PaintPhaseBlockBackground, FloatRect(100, 200, 50, 200));
    drawRect(context, content2, PaintPhaseForeground, FloatRect(100, 200, 50, 200));
    drawRect(context, container2, PaintPhaseForeground, FloatRect(100, 200, 100, 100));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 8,
        TestDisplayItem(container1, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(content1, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(content1, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(container1, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(container2, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(content2, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(content2, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(container2, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)));

    // Simulate the situation when container1 e.g. gets a z-index that is now greater than container2.
    rootDisplayItemList().invalidate(container1.displayItemClient());
    drawRect(context, container2, PaintPhaseBlockBackground, FloatRect(100, 200, 100, 100));
    drawRect(context, content2, PaintPhaseBlockBackground, FloatRect(100, 200, 50, 200));
    drawRect(context, content2, PaintPhaseForeground, FloatRect(100, 200, 50, 200));
    drawRect(context, container2, PaintPhaseForeground, FloatRect(100, 200, 100, 100));
    drawRect(context, container1, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(context, content1, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(context, content1, PaintPhaseForeground, FloatRect(100, 100, 50, 200));
    drawRect(context, container1, PaintPhaseForeground, FloatRect(100, 100, 100, 100));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 8,
        TestDisplayItem(container2, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(content2, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(content2, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(container2, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(container1, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(content1, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(content1, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(container1, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)));
}

// Enable this when cached subtree flags are ready.
TEST_F(ViewDisplayListTest, DISABLED_CachedSubtreeSwapOrder)
{
    setBodyInnerHTML("<div id='container1'><div id='content1'></div></div>"
        "<div id='container2'><div id='content2'></div></div>");
    LayoutObject& container1 = *document().body()->firstChild()->layoutObject();
    LayoutObject& content1 = *document().body()->firstChild()->firstChild()->layoutObject();
    LayoutObject& container2 = *document().body()->firstChild()->nextSibling()->layoutObject();
    LayoutObject& content2 = *document().body()->firstChild()->nextSibling()->firstChild()->layoutObject();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    {
        SubtreeRecorder r(context, container1, PaintPhaseBlockBackground);
        r.begin();
        drawRect(context, container1, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
        drawRect(context, content1, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    }
    {
        SubtreeRecorder r(context, container1, PaintPhaseForeground);
        r.begin();
        drawRect(context, content1, PaintPhaseForeground, FloatRect(100, 100, 50, 200));
        drawRect(context, container1, PaintPhaseForeground, FloatRect(100, 100, 100, 100));
    }
    {
        SubtreeRecorder r(context, container2, PaintPhaseBlockBackground);
        r.begin();
        drawRect(context, container2, PaintPhaseBlockBackground, FloatRect(100, 200, 100, 100));
        drawRect(context, content2, PaintPhaseBlockBackground, FloatRect(100, 200, 50, 200));
    }
    {
        SubtreeRecorder r(context, container2, PaintPhaseForeground);
        r.begin();
        drawRect(context, content2, PaintPhaseForeground, FloatRect(100, 200, 50, 200));
        drawRect(context, container2, PaintPhaseForeground, FloatRect(100, 200, 100, 100));
    }
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 16,
        TestDisplayItem(container1, DisplayItem::paintPhaseToBeginSubtreeType(PaintPhaseBlockBackground)),
        TestDisplayItem(container1, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(content1, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(container1, DisplayItem::paintPhaseToEndSubtreeType(PaintPhaseBlockBackground)),

        TestDisplayItem(container1, DisplayItem::paintPhaseToBeginSubtreeType(PaintPhaseForeground)),
        TestDisplayItem(content1, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(container1, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(container1, DisplayItem::paintPhaseToEndSubtreeType(PaintPhaseForeground)),

        TestDisplayItem(container2, DisplayItem::paintPhaseToBeginSubtreeType(PaintPhaseBlockBackground)),
        TestDisplayItem(container2, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(content2, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(container2, DisplayItem::paintPhaseToEndSubtreeType(PaintPhaseBlockBackground)),

        TestDisplayItem(container2, DisplayItem::paintPhaseToBeginSubtreeType(PaintPhaseForeground)),
        TestDisplayItem(content2, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(container2, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(container2, DisplayItem::paintPhaseToEndSubtreeType(PaintPhaseForeground)));

    // Simulate the situation when container1 e.g. gets a z-index that is now greater than container2,
    // and at the same time container2 is scrolled out of viewport and content2 is invalidated.
    rootDisplayItemList().invalidate(content2.displayItemClient());
    {
        SubtreeRecorder r(context, container2, PaintPhaseBlockBackground);
    }
    EXPECT_EQ((size_t)1, newPaintListBeforeUpdate().size());
    EXPECT_TRUE(newPaintListBeforeUpdate().last()->isSubtreeCached());
    {
        SubtreeRecorder r(context, container2, PaintPhaseForeground);
    }
    EXPECT_EQ((size_t)2, newPaintListBeforeUpdate().size());
    EXPECT_TRUE(newPaintListBeforeUpdate().last()->isSubtreeCached());
    {
        SubtreeRecorder r(context, container1, PaintPhaseBlockBackground);
        r.begin();
        drawRect(context, container1, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
        drawRect(context, content1, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    }
    {
        SubtreeRecorder r(context, container1, PaintPhaseForeground);
        r.begin();
        drawRect(context, content1, PaintPhaseForeground, FloatRect(100, 100, 50, 200));
        drawRect(context, container1, PaintPhaseForeground, FloatRect(100, 100, 100, 100));
    }
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 14,
        TestDisplayItem(container2, DisplayItem::paintPhaseToBeginSubtreeType(PaintPhaseBlockBackground)),
        TestDisplayItem(container2, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(container2, DisplayItem::paintPhaseToEndSubtreeType(PaintPhaseBlockBackground)),

        TestDisplayItem(container2, DisplayItem::paintPhaseToBeginSubtreeType(PaintPhaseForeground)),
        TestDisplayItem(container2, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(container2, DisplayItem::paintPhaseToEndSubtreeType(PaintPhaseForeground)),

        TestDisplayItem(container1, DisplayItem::paintPhaseToBeginSubtreeType(PaintPhaseBlockBackground)),
        TestDisplayItem(container1, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(content1, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(container1, DisplayItem::paintPhaseToEndSubtreeType(PaintPhaseBlockBackground)),

        TestDisplayItem(container1, DisplayItem::paintPhaseToBeginSubtreeType(PaintPhaseForeground)),
        TestDisplayItem(content1, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(container1, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(container1, DisplayItem::paintPhaseToEndSubtreeType(PaintPhaseForeground)));
}

TEST_F(ViewDisplayListTest, Scope)
{
    setBodyInnerHTML("<div id='multicol'><div id='content'></div></div>");

    LayoutObject& multicol = *document().body()->firstChild()->layoutObject();
    LayoutObject& content = *document().body()->firstChild()->firstChild()->layoutObject();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    FloatRect rect1(100, 100, 50, 50);
    FloatRect rect2(150, 100, 50, 50);
    FloatRect rect3(200, 100, 50, 50);
    drawRect(context, multicol, PaintPhaseBlockBackground, FloatRect(100, 200, 100, 100));
    {
        ScopeRecorder r(context, multicol);
        drawRect(context, content, PaintPhaseForeground, rect1);
    }
    {
        ScopeRecorder r(context, multicol);
        drawRect(context, content, PaintPhaseForeground, rect2);
    }
    rootDisplayItemList().endNewPaints();
    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(multicol, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(content, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(content, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)));
    RefPtr<const SkPicture> picture1 = static_cast<DrawingDisplayItem*>(rootDisplayItemList().paintList()[1].get())->picture();
    RefPtr<const SkPicture> picture2 = static_cast<DrawingDisplayItem*>(rootDisplayItemList().paintList()[2].get())->picture();
    EXPECT_NE(picture1, picture2);

    // Draw again with nothing invalidated.
    drawRect(context, multicol, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    {
        ScopeRecorder r(context, multicol);
        drawRect(context, content, PaintPhaseForeground, rect1);
    }
    {
        ScopeRecorder r(context, multicol);
        drawRect(context, content, PaintPhaseForeground, rect2);
    }
    EXPECT_TRUE(newPaintListBeforeUpdate()[0]->isCached());
    EXPECT_TRUE(newPaintListBeforeUpdate()[1]->isCached());
    EXPECT_TRUE(newPaintListBeforeUpdate()[2]->isCached());
    rootDisplayItemList().endNewPaints();
    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(multicol, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(content, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(content, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)));
    EXPECT_EQ(picture1, static_cast<DrawingDisplayItem*>(rootDisplayItemList().paintList()[1].get())->picture());
    EXPECT_EQ(picture2, static_cast<DrawingDisplayItem*>(rootDisplayItemList().paintList()[2].get())->picture());

    // Now the multicol becomes 3 columns and repaints.
    rootDisplayItemList().invalidate(multicol.displayItemClient());
    rootDisplayItemList().invalidate(content.displayItemClient());
    drawRect(context, multicol, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    {
        ScopeRecorder r(context, multicol);
        drawRect(context, content, PaintPhaseForeground, rect1);
    }
    {
        ScopeRecorder r(context, multicol);
        drawRect(context, content, PaintPhaseForeground, rect2);
    }
    {
        ScopeRecorder r(context, multicol);
        drawRect(context, content, PaintPhaseForeground, rect3);
    }
    // We should repaint everything on invalidation of the scope container.
    EXPECT_TRUE(newPaintListBeforeUpdate()[0]->isDrawing());
    EXPECT_TRUE(newPaintListBeforeUpdate()[1]->isDrawing());
    EXPECT_TRUE(newPaintListBeforeUpdate()[2]->isDrawing());
    EXPECT_TRUE(newPaintListBeforeUpdate()[3]->isDrawing());
    rootDisplayItemList().endNewPaints();
    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(multicol, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(content, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(content, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(content, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)));
    EXPECT_NE(picture1, static_cast<DrawingDisplayItem*>(rootDisplayItemList().paintList()[1].get())->picture());
    EXPECT_NE(picture2, static_cast<DrawingDisplayItem*>(rootDisplayItemList().paintList()[2].get())->picture());
}

TEST_F(ViewDisplayListTest, InlineRelayout)
{
    setBodyInnerHTML("<div id='div' style='width:100px; height: 200px'>AAAAAAAAAA BBBBBBBBBB</div>");
    LayoutView& layoutView = *document().layoutView();
    DeprecatedPaintLayer& rootLayer = *layoutView.layer();
    LayoutObject& htmlObject = *document().documentElement()->layoutObject();
    Element& div = *toElement(document().body()->firstChild());
    LayoutBlock& divBlock = *toLayoutBlock(document().body()->firstChild()->layoutObject());
    LayoutText& text = *toLayoutText(divBlock.firstChild());
    InlineTextBox& firstTextBox = *text.firstTextBox();
    DisplayItemClient firstTextBoxDisplayItemClient = firstTextBox.displayItemClient();

    GraphicsContext context(nullptr, &rootDisplayItemList());
    DeprecatedPaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 800, 600), PaintBehaviorNormal, LayoutSize());
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(htmlObject, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(firstTextBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)));

    div.setAttribute(HTMLNames::styleAttr, "width: 10px; height: 200px");
    document().view()->updateLayoutAndStyleForPainting();
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(htmlObject.displayItemClient()));
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(divBlock.displayItemClient()));
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(firstTextBoxDisplayItemClient));
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().endNewPaints();

    LayoutText& newText = *toLayoutText(divBlock.firstChild());
    InlineTextBox& newFirstTextBox = *newText.firstTextBox();
    InlineTextBox& secondTextBox = *newText.firstTextBox()->nextTextBox();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(htmlObject, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(newFirstTextBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(secondTextBox, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)));
}

} // namespace blink
