// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ViewDisplayList.h"

#include "core/paint/ClipRecorder.h"
#include "core/paint/DrawingRecorder.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/RenderingTestHelper.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "platform/graphics/GraphicsContext.h"
#include <gtest/gtest.h>

namespace blink {
namespace {

class ViewDisplayListTest : public RenderingTest {
public:
    ViewDisplayListTest() : m_renderView(nullptr) { }

protected:
    RenderView* renderView() { return m_renderView; }

private:
    virtual void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(true);

        RenderingTest::SetUp();

        m_renderView = document().view()->renderView();
        ASSERT_TRUE(m_renderView);
    }

    RenderView* m_renderView;
};

class TestDisplayItem : public DisplayItem {
public:
    TestDisplayItem(const RenderObject* renderer, Type type) : DisplayItem(renderer, type) { }

    virtual void replay(GraphicsContext*) override final { ASSERT_NOT_REACHED(); }
};

#define EXPECT_DISPLAY_LIST(actual, expectedSize, ...) { \
    EXPECT_EQ((size_t)expectedSize, actual.size()); \
    const TestDisplayItem expected[] = { __VA_ARGS__ }; \
    for (size_t index = 0; index < expectedSize; index++) { \
        EXPECT_EQ(expected[index].renderer(), actual[index]->renderer()); \
        EXPECT_EQ(expected[index].type(), actual[index]->type()); \
    } \
}

void drawRect(GraphicsContext* context, RenderObject* renderer, PaintPhase phase, const FloatRect& bound)
{
    DrawingRecorder drawingRecorder(context, renderer, phase, bound);
    IntRect rect(0, 0, 10, 10);
    context->drawRect(rect);
}

void drawClippedRect(GraphicsContext* context, RenderLayerModelObject* renderer, PaintPhase phase, const FloatRect& bound)
{
    IntRect rect(1, 1, 9, 9);
    ClipRect clipRect(rect);
    ClipRecorder clipRecorder(renderer, context, DisplayItem::ClipLayerForeground, clipRect, 0, LayoutPoint(), PaintLayerFlags());
    drawRect(context, renderer, phase, bound);
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_NestedRecorders)
{
    GraphicsContext* context = new GraphicsContext(nullptr);
    FloatRect bound = renderView()->viewRect();

    drawClippedRect(context, renderView(), PaintPhaseForeground, bound);

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 3,
        TestDisplayItem(renderView(), DisplayItem::ClipLayerForeground),
        TestDisplayItem(renderView(), DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(renderView(), DisplayItem::EndClip));
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateBasic)
{
    setBodyInnerHTML("<div id='first'><div id='second'></div></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->firstChild()->renderer();
    GraphicsContext* context = new GraphicsContext(nullptr);

    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 300, 300));
    drawRect(context, second, PaintPhaseChildBlockBackground, FloatRect(100, 100, 200, 200));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 300, 300));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 3,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseChildBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline));

    renderView()->viewDisplayList().invalidate(second);
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 300, 300));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 300, 300));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline));
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateSwapOrder)
{
    setBodyInnerHTML("<div id='first'><div id='second'></div></div><div id='unaffected'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->firstChild()->renderer();
    RenderObject* unaffected = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext* context = new GraphicsContext(nullptr);

    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(context, unaffected, PaintPhaseBlockBackground, FloatRect(300, 300, 10, 10));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 3,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(unaffected, DisplayItem::DrawingPaintPhaseBlockBackground));

    renderView()->viewDisplayList().invalidate(second);
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 3,
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
    GraphicsContext* context = new GraphicsContext(nullptr);

    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground));

    renderView()->viewDisplayList().invalidate(third);
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(context, third, PaintPhaseBlockBackground, FloatRect(125, 100, 200, 50));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 3,
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
    GraphicsContext* context = new GraphicsContext(nullptr);

    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(context, third, PaintPhaseBlockBackground, FloatRect(300, 100, 50, 50));
    drawRect(context, first, PaintPhaseForeground, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseForeground, FloatRect(100, 100, 50, 200));
    drawRect(context, third, PaintPhaseForeground, FloatRect(300, 100, 50, 50));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseOutline, FloatRect(100, 100, 50, 200));
    drawRect(context, third, PaintPhaseOutline, FloatRect(300, 100, 50, 50));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 9,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseOutline));

    renderView()->viewDisplayList().invalidate(second);
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(context, first, PaintPhaseForeground, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseForeground, FloatRect(100, 100, 50, 200));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseOutline, FloatRect(100, 100, 50, 200));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 9,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseForeground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(third, DisplayItem::DrawingPaintPhaseOutline));

    renderView()->viewDisplayList().invalidate(second);

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 6,
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
    GraphicsContext* context = new GraphicsContext(nullptr);

    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 2,
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline));

    renderView()->viewDisplayList().invalidate(first);
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 50));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 50, 50));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 4,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline));

    renderView()->viewDisplayList().invalidate(first);

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 2,
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline));
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateAddFirstOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext* context = new GraphicsContext(nullptr);

    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 2,
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline));

    renderView()->viewDisplayList().invalidate(first);
    renderView()->viewDisplayList().invalidate(second);
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 4,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline));

    renderView()->viewDisplayList().invalidate(first);
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 2,
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline));
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateAddLastNoOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext* context = new GraphicsContext(nullptr);

    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 50));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 50, 50));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline));

    renderView()->viewDisplayList().invalidate(second);
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 4,
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline));

    renderView()->viewDisplayList().invalidate(second);

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline));
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateAddLastOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext* context = new GraphicsContext(nullptr);

    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline));

    renderView()->viewDisplayList().invalidate(first);
    renderView()->viewDisplayList().invalidate(second);
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 4,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(second, DisplayItem::DrawingPaintPhaseOutline));

    renderView()->viewDisplayList().invalidate(first);
    renderView()->viewDisplayList().invalidate(second);
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 2,
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(first, DisplayItem::DrawingPaintPhaseOutline));
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateClip)
{
    setBodyInnerHTML("<div id='first'><div id='second'></div></div>");
    RenderLayerModelObject* firstRenderer = toRenderLayerModelObject(document().body()->firstChild()->renderer());
    RenderLayerModelObject* secondRenderer = toRenderLayerModelObject(document().body()->firstChild()->firstChild()->renderer());
    GraphicsContext* context = new GraphicsContext(nullptr);

    ClipRect firstClipRect(IntRect(1, 1, 2, 2));
    {
        ClipRecorder clipRecorder(firstRenderer, context, DisplayItem::ClipLayerForeground, firstClipRect, 0, LayoutPoint(), PaintLayerFlags());
        drawRect(context, firstRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
        drawRect(context, secondRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    }

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 4,
        TestDisplayItem(firstRenderer, DisplayItem::ClipLayerForeground),
        TestDisplayItem(firstRenderer, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(secondRenderer, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(firstRenderer, DisplayItem::EndClip));

    renderView()->viewDisplayList().invalidate(firstRenderer);
    drawRect(context, firstRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, secondRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 2,
        TestDisplayItem(firstRenderer, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(secondRenderer, DisplayItem::DrawingPaintPhaseBlockBackground));

    renderView()->viewDisplayList().invalidate(secondRenderer);
    drawRect(context, firstRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    ClipRect secondClipRect(IntRect(1, 1, 2, 2));
    {
        ClipRecorder clipRecorder(secondRenderer, context, DisplayItem::ClipLayerForeground, secondClipRect, 0, LayoutPoint(), PaintLayerFlags());
        drawRect(context, secondRenderer, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    }

    EXPECT_DISPLAY_LIST(renderView()->viewDisplayList().paintList(), 4,
        TestDisplayItem(firstRenderer, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(secondRenderer, DisplayItem::ClipLayerForeground),
        TestDisplayItem(secondRenderer, DisplayItem::DrawingPaintPhaseBlockBackground),
        TestDisplayItem(secondRenderer, DisplayItem::EndClip));
}

} // anonymous namespace
} // namespace blink
