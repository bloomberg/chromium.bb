// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/layout/compositing/RenderLayerCompositor.h"
#include "core/paint/LayerClipRecorder.h"
#include "core/paint/LayerPainter.h"
#include "core/paint/RenderDrawingRecorder.h"
#include "core/rendering/InlineTextBox.h"
#include "core/rendering/RenderText.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/RenderingTestHelper.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "third_party/skia/include/core/SkCanvas.h"
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

    virtual void TearDown() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(false);
        RuntimeEnabledFeatures::setSlimmingPaintDisplayItemCacheEnabled(false);
    }

    RenderView* m_renderView;
};

class TestDisplayItem : public DisplayItem {
public:
    TestDisplayItem(const RenderObject* renderer, Type type) : DisplayItem(renderer->displayItemClient(), type) { }
    TestDisplayItem(DisplayItemClient displayItemClient, Type type) : DisplayItem(displayItemClient, type) { }

    virtual void replay(GraphicsContext*) override final { ASSERT_NOT_REACHED(); }
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override final { ASSERT_NOT_REACHED(); }
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
    for (size_t index = 0; index < std::min<size_t>(actual.size(), expectedSize); index++) { \
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
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(renderView(), DisplayItem::ClipLayerForeground),
        TestDisplayItem(renderView(), DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(renderView(), DisplayItem::clipTypeToEndClipType(DisplayItem::ClipLayerForeground)));
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
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseChildBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));

    rootDisplayItemList().invalidate(second->displayItemClient());
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 300, 300));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 300, 300));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));
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
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(unaffected, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)));

    rootDisplayItemList().invalidate(second->displayItemClient());
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(&context, unaffected, PaintPhaseBlockBackground, FloatRect(300, 300, 10, 10));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(unaffected, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)));
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
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)));

    rootDisplayItemList().invalidate(third->displayItemClient());
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(&context, third, PaintPhaseBlockBackground, FloatRect(125, 100, 200, 50));
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(third, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)));
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

    rootDisplayItemList().invalidate(second->displayItemClient());
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(&context, third, PaintPhaseBlockBackground, FloatRect(300, 100, 50, 50));
    drawRect(&context, first, PaintPhaseForeground, FloatRect(100, 100, 100, 100));
    drawRect(&context, second, PaintPhaseForeground, FloatRect(100, 100, 50, 200));
    drawRect(&context, third, PaintPhaseForeground, FloatRect(300, 100, 50, 50));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 100, 100));
    drawRect(&context, second, PaintPhaseOutline, FloatRect(100, 100, 50, 200));
    drawRect(&context, third, PaintPhaseOutline, FloatRect(300, 100, 50, 50));
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

    // The following is only applicable when we support incremental paint.
#if 0
    rootDisplayItemList().invalidate(second->displayItemClient());

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 6,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(third, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(third, DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)),
        TestDisplayItem(third, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));
#endif
}

// This test is only applicable when we support incremental paint.
TEST_F(ViewDisplayListTest, DISABLED_ViewDisplayListTest_UpdateAddFirstNoOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(&context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));

    rootDisplayItemList().invalidate(first->displayItemClient());
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 50));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));

    rootDisplayItemList().invalidate(first->displayItemClient());

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));
}

// This test is only applicable when we support incremental paint.
TEST_F(ViewDisplayListTest, DISABLED_ViewDisplayListTest_UpdateAddFirstOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(&context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));

    rootDisplayItemList().invalidate(first->displayItemClient());
    rootDisplayItemList().invalidate(second->displayItemClient());
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(&context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));

    rootDisplayItemList().invalidate(first->displayItemClient());
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(&context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));
}

// This test is only applicable when we support incremental paint.
TEST_F(ViewDisplayListTest, DISABLED_ViewDisplayListTest_UpdateAddLastNoOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 50));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));

    rootDisplayItemList().invalidate(second->displayItemClient());
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(&context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));

    rootDisplayItemList().invalidate(second->displayItemClient());

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));
}

// This test is only applicable when we support incremental paint.
TEST_F(ViewDisplayListTest, DISABLED_ViewDisplayListTest_UpdateAddLastOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));

    rootDisplayItemList().invalidate(first->displayItemClient());
    rootDisplayItemList().invalidate(second->displayItemClient());
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));
    drawRect(&context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(&context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(second, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));

    rootDisplayItemList().invalidate(first->displayItemClient());
    rootDisplayItemList().invalidate(second->displayItemClient());
    drawRect(&context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(&context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(first, DisplayItem::paintPhaseToDrawingType(PaintPhaseOutline)));
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
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(firstRenderer, DisplayItem::ClipLayerForeground),
        TestDisplayItem(firstRenderer, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(secondRenderer, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(firstRenderer, DisplayItem::clipTypeToEndClipType(DisplayItem::ClipLayerForeground)));

    rootDisplayItemList().invalidate(firstRenderer->displayItemClient());
    drawRect(&context, firstRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(&context, secondRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(firstRenderer, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(secondRenderer, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)));

    rootDisplayItemList().invalidate(secondRenderer->displayItemClient());
    drawRect(&context, firstRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    ClipRect secondClipRect(IntRect(1, 1, 2, 2));
    {
        LayerClipRecorder layerClipRecorder(secondRenderer, &context, DisplayItem::ClipLayerForeground, secondClipRect, 0, LayoutPoint(), PaintLayerFlags());
        drawRect(&context, secondRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    }
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 4,
        TestDisplayItem(firstRenderer, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(secondRenderer, DisplayItem::ClipLayerForeground),
        TestDisplayItem(secondRenderer, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(secondRenderer, DisplayItem::clipTypeToEndClipType(DisplayItem::ClipLayerForeground)));
}

TEST_F(ViewDisplayListTest, CachedDisplayItems)
{
    RuntimeEnabledFeatures::setSlimmingPaintDisplayItemCacheEnabled(true);

    setBodyInnerHTML("<div id='first'><div id='second'></div></div>");
    RenderLayerModelObject* firstRenderer = toRenderLayerModelObject(document().body()->firstChild()->renderer());
    RenderLayerModelObject* secondRenderer = toRenderLayerModelObject(document().body()->firstChild()->firstChild()->renderer());
    GraphicsContext context(nullptr, &rootDisplayItemList());

    drawRect(&context, firstRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(&context, secondRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(firstRenderer, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(secondRenderer, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)));
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(firstRenderer->displayItemClient()));
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(secondRenderer->displayItemClient()));
    DisplayItem* firstDisplayItem = rootDisplayItemList().paintList()[0].get();
    DisplayItem* secondDisplayItem = rootDisplayItemList().paintList()[1].get();

    rootDisplayItemList().invalidate(firstRenderer->displayItemClient());
    EXPECT_FALSE(rootDisplayItemList().clientCacheIsValid(firstRenderer->displayItemClient()));
    EXPECT_TRUE(rootDisplayItemList().clientCacheIsValid(secondRenderer->displayItemClient()));

    drawRect(&context, firstRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(&context, secondRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(firstRenderer, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)),
        TestDisplayItem(secondRenderer, DisplayItem::paintPhaseToDrawingType(PaintPhaseBlockBackground)));
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
    InlineTextBox* textInlineBox = toRenderText(div->firstChild()->renderer())->firstTextBox();

    SkCanvas canvas(800, 600);
    GraphicsContext context(&canvas, &rootDisplayItemList());
    LayerPaintingInfo paintingInfo(rootLayer, LayoutRect(0, 0, 800, 600), PaintBehaviorNormal, LayoutSize());
    LayerPainter(*rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 2,
        TestDisplayItem(htmlRenderer, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(textInlineBox->displayItemClient(), DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)));

    div->focus();
    document().view()->updateLayoutAndStyleForPainting();
    LayerPainter(*rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().endNewPaints();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().paintList(), 3,
        TestDisplayItem(htmlRenderer, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(textInlineBox->displayItemClient(), DisplayItem::paintPhaseToDrawingType(PaintPhaseForeground)),
        TestDisplayItem(divRenderer, DisplayItem::paintPhaseToDrawingType(PaintPhaseCaret)));
}

} // anonymous namespace
} // namespace blink
