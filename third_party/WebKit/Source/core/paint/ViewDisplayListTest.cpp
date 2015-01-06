// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/paint/LayerClipRecorder.h"
#include "core/paint/LayerPainter.h"
#include "core/paint/RenderDrawingRecorder.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/RenderingTestHelper.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include <gtest/gtest.h>

namespace blink {
namespace {

class ViewDisplayListTest : public RenderingTest {
public:
    ViewDisplayListTest() : m_renderView(nullptr) { }

protected:
    RenderView* renderView() { return m_renderView; }
    DisplayItemList& rootDisplayItemList() { return *renderView()->layer()->graphicsLayerBacking()->displayItemList(); }

private:
    virtual void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(true);

        RenderingTest::SetUp();
        enableCompositing();

        m_renderView = document().view()->renderView();
        ASSERT_TRUE(m_renderView);
    }

    RenderView* m_renderView;
};

class TestDisplayItem : public DisplayItem {
public:
    TestDisplayItem(const RenderObject* renderer, Type type) : DisplayItem(renderer->displayItemClient(), type) { }

    virtual void replay(GraphicsContext*) override final { ASSERT_NOT_REACHED(); }
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override final { ASSERT_NOT_REACHED(); }
#ifndef NDEBUG
    virtual const char* name() const override final { return "Test"; }
#endif
};

#ifndef NDEBUG
#define TRACE_DISPLAY_ITEMS(expected, actual) \
    String trace = "Expected: " + (expected).asDebugString() + " Actual: " + (actual).asDebugString(); \
    SCOPED_TRACE(trace.utf8().data());
#else
#define TRACE_DISPLAY_ITEMS(expected, actual)
#endif

#define EXPECT_DISPLAY_LIST(actual, expectedSize, ...) { \
    EXPECT_EQ((size_t)expectedSize, actual.size()); \
    const TestDisplayItem expected[] = { __VA_ARGS__ }; \
    for (size_t index = 0; index < expectedSize; index++) { \
        TRACE_DISPLAY_ITEMS(expected[index], *actual[index]); \
        EXPECT_EQ(expected[index].client(), actual[index]->client()); \
        EXPECT_EQ(expected[index].type(), actual[index]->type()); \
    } \
}

void drawRect(GraphicsContext* context, RenderObject* renderer, PaintPhase phase, const FloatRect& bound)
{
    RenderDrawingRecorder drawingRecorder(context, *renderer, phase, bound);
    if (drawingRecorder.canUseCachedDrawing())
        return;
    IntRect rect(0, 0, 10, 10);
    context->drawRect(rect);
}

void drawClippedRect(GraphicsContext* context, RenderLayerModelObject* renderer, PaintPhase phase, const FloatRect& bound)
{
    IntRect rect(1, 1, 9, 9);
    ClipRect clipRect(rect);
    LayerClipRecorder layerClipRecorder(renderer, context, DisplayItem::ClipLayerForeground, clipRect, 0, LayoutPoint(), PaintLayerFlags());
    drawRect(context, renderer, phase, bound);
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_NestedRecorders)
{
    GraphicsContext context(nullptr, &rootDisplayItemList());
    FloatRect bound = renderView()->viewRect();

    drawClippedRect(&context, renderView(), PaintPhaseForeground, bound);

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(renderView(), DisplayItem::ClipLayerForeground),
        TestDisplayItem(renderView(), DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(renderView(), DisplayItem::EndClip));
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateBasic)
{
    setBodyInnerHTML("<div id='first'><div id='second'></div></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->firstChild()->renderer();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 300, 300));
    drawRect(&context, second, PaintPhaseChildBlockBackground, FloatRect(100, 100, 200, 200));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 300, 300));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseChildBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline));

    rootDisplayItemList().invalidate(second->displayItemClient());
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 300, 300));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 300, 300));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline));
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateSwapOrder)
{
    setBodyInnerHTML("<div id='first'><div id='second'></div></div><div id='unaffected'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->firstChild()->renderer();
    RenderObject* unaffected = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(&context, unaffected, PaintPhaseBlockBackground, FloatRect(300, 300, 10, 10));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(unaffected, DisplayItem::DrawingPaintPhaseBlockBackground));

    rootDisplayItemList().invalidate(second->displayItemClient());
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(unaffected, DisplayItem::DrawingPaintPhaseBlockBackground));
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateNewItemInMiddle)
{
    setBodyInnerHTML("<div id='first'><div id='second'><div id='third'></div></div></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->firstChild()->renderer();
    RenderObject* third = document().body()->firstChild()->firstChild()->firstChild()->renderer();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground));

    rootDisplayItemList().invalidate(third->displayItemClient());
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(&context, third, PaintPhaseBlockBackground, FloatRect(125, 100, 200, 50));
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground));
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateInvalidationWithPhases)
{
    setBodyInnerHTML("<div id='first'><div id='second'></div></div><div id='third'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->firstChild()->renderer();
    RenderObject* third = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(&context, third, PaintPhaseBlockBackground, FloatRect(300, 100, 50, 50));
    drawRect(&context, first, PaintPhaseForeground, FloatRect(100, 100, 100, 100));
    drawRect(&context, second, PaintPhaseForeground, FloatRect(100, 100, 50, 200));
    drawRect(&context, third, PaintPhaseForeground, FloatRect(300, 100, 50, 50));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 100, 100));
    drawRect(&context, second, PaintPhaseOutline, FloatRect(100, 100, 50, 200));
    drawRect(&context, third, PaintPhaseOutline, FloatRect(300, 100, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 9,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseOutline));

    rootDisplayItemList().invalidate(second->displayItemClient());
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(&context, first, PaintPhaseForeground, FloatRect(100, 100, 100, 100));
    drawRect(&context, second, PaintPhaseForeground, FloatRect(100, 100, 50, 200));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 100, 100));
    drawRect(&context, second, PaintPhaseOutline, FloatRect(100, 100, 50, 200));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 9,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseOutline));

    rootDisplayItemList().invalidate(second->displayItemClient());

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 6,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseOutline));
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateAddFirstNoOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(&context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline));

    rootDisplayItemList().invalidate(first->displayItemClient());
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 50));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline));

    rootDisplayItemList().invalidate(first->displayItemClient());

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline));
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateAddFirstOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(&context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline));

    rootDisplayItemList().invalidate(first->displayItemClient());
    rootDisplayItemList().invalidate(second->displayItemClient());
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(&context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline));

    rootDisplayItemList().invalidate(first->displayItemClient());
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(&context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline));
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateAddLastNoOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 50));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline));

    rootDisplayItemList().invalidate(second->displayItemClient());
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(&context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline));

    rootDisplayItemList().invalidate(second->displayItemClient());

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline));
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateAddLastOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline));

    rootDisplayItemList().invalidate(first->displayItemClient());
    rootDisplayItemList().invalidate(second->displayItemClient());
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(&context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline));

    rootDisplayItemList().invalidate(first->displayItemClient());
    rootDisplayItemList().invalidate(second->displayItemClient());
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline));
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateClip)
{
    setBodyInnerHTML("<div id='first'><div id='second'></div></div>");
    RenderLayerModelObject* firstRenderer = toRenderLayerModelObject(document().body()->firstChild()->renderer());
    RenderLayerModelObject* secondRenderer = toRenderLayerModelObject(document().body()->firstChild()->firstChild()->renderer());
    GraphicsContext context(nullptr, &rootDisplayItemList());

    ClipRect firstClipRect(IntRect(1, 1, 2, 2));
    {
        LayerClipRecorder layerClipRecorder(firstRenderer, &context, DisplayItem::ClipLayerForeground, firstClipRect, 0, LayoutPoint(), PaintLayerFlags());
        drawRect(&context, firstRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
        drawRect(&context, secondRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    }

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(firstRenderer, DisplayItem::ClipLayerForeground),
        TestDisplayItem(firstRenderer, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(secondRenderer, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(firstRenderer, DisplayItem::EndClip));

    rootDisplayItemList().invalidate(firstRenderer->displayItemClient());
    drawRect(&context, firstRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(&context, secondRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(firstRenderer, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(secondRenderer, DisplayItem::DrawingPaintPhaseBlockBackground));

    rootDisplayItemList().invalidate(secondRenderer->displayItemClient());
    drawRect(&context, firstRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    ClipRect secondClipRect(IntRect(1, 1, 2, 2));
    {
        LayerClipRecorder layerClipRecorder(secondRenderer, &context, DisplayItem::ClipLayerForeground, secondClipRect, 0, LayoutPoint(), PaintLayerFlags());
        drawRect(&context, secondRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    }

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(firstRenderer, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(secondRenderer, DisplayItem::ClipLayerForeground),
        TestDisplayItem(secondRenderer, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(secondRenderer, DisplayItem::EndClip));
}

TEST_F(ViewDisplayListTest, CachedDisplayItems)
{
    setBodyInnerHTML("<div id='first'><div id='second'></div></div>");
    RenderLayerModelObject* firstRenderer = toRenderLayerModelObject(document().body()->firstChild()->renderer());
    RenderLayerModelObject* secondRenderer = toRenderLayerModelObject(document().body()->firstChild()->firstChild()->renderer());
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(&context, firstRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(&context, secondRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(firstRenderer, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(secondRenderer, DisplayItem::DrawingPaintPhaseBlockBackground));
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(firstRenderer->displayItemClient()));
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(secondRenderer->displayItemClient()));
    DisplayItem* firstDisplayItem = rootDisplayItemList().paintList()[0].get();
    DisplayItem* secondDisplayItem = rootDisplayItemList().paintList()[1].get();

    rootDisplayItemList().invalidate(firstRenderer->displayItemClient());
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(firstRenderer->displayItemClient()));
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(secondRenderer->displayItemClient()));

    drawRect(&context, firstRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(&context, secondRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(firstRenderer, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(secondRenderer, DisplayItem::DrawingPaintPhaseBlockBackground));
    // The first display item should be updated.
    EXPECT_NE(firstDisplayItem, rootDisplayItemList().paintList()[0].get());
    // The second display item should be cached.
    EXPECT_EQ(secondDisplayItem, rootDisplayItemList().paintList()[1].get());
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(firstRenderer->displayItemClient()));
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(secondRenderer->displayItemClient()));

    rootDisplayItemList().invalidateAll();
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(firstRenderer->displayItemClient()));
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(secondRenderer->displayItemClient()));
}

TEST_F(ViewDisplayListTest, FullDocumentPaintingWithCaret)
{
    setBodyInnerHTML("<div id='div' contentEditable='true'>XYZ</div>");
    RenderView* renderView = document().renderView();
    RenderLayer* rootLayer = renderView->layer();
    RenderObject* htmlRenderer = document().documentElement()->renderer();
    Element* div = toElement(document().body()->firstChild());
    RenderObject* divRenderer = document().body()->firstChild()->renderer();
    RenderObject* textRenderer = div->firstChild()->renderer();

    SkCanvas canvas(800, 600);
    GraphicsContext context(&canvas, &rootDisplayItemList());
    LayerPaintingInfo paintingInfo(rootLayer, LayoutRect(0, 0, 800, 600), PaintBehaviorNormal, LayoutSize());
    LayerPainter(*rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(htmlRenderer, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(textRenderer, DisplayItem::DrawingPaintPhaseForeground));

    div->focus();
    document().view()->updateLayoutAndStyleForPainting();
    LayerPainter(*rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(htmlRenderer, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(textRenderer, DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(divRenderer, DisplayItem::DrawingPaintPhaseCaret));
}

} // anonymous namespace
} // namespace blink
