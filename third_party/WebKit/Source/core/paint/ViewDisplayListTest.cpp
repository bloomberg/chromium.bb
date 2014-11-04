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

void drawRect(GraphicsContext* context, RenderObject* renderer, PaintPhase phase, const FloatRect& bound)
{
    DrawingRecorder drawingRecorder(context, renderer, phase, bound);
    IntRect rect(0, 0, 10, 10);
    context->drawRect(rect);
}

void drawClippedRect(GraphicsContext* context, RenderView* renderView, PaintPhase phase, const FloatRect& bound)
{
    IntRect rect(1, 1, 9, 9);
    ClipRect clipRect(rect);
    ClipRecorder clipRecorder(renderView->compositor()->rootRenderLayer(), context, DisplayItem::ClipLayerForeground, clipRect);
    drawRect(context, renderView, phase, bound);
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_NestedRecorders)
{
    GraphicsContext* context = new GraphicsContext(nullptr);
    FloatRect bound = renderView()->viewRect();

    drawClippedRect(context, renderView(), PaintPhaseForeground, bound);
    EXPECT_EQ((size_t)3, renderView()->viewDisplayList().paintList().size());

    // TODO(schenney): Check that the IDs are what we expect.
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

    EXPECT_EQ((size_t)3, renderView()->viewDisplayList().paintList().size());

    renderView()->viewDisplayList().invalidate(second);
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 300, 300));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 300, 300));
    EXPECT_EQ((size_t)2, renderView()->viewDisplayList().paintList().size());
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

    const PaintList& firstList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)3, firstList.size());
    EXPECT_EQ(first, firstList[0]->renderer());
    EXPECT_EQ(second, firstList[1]->renderer());
    EXPECT_EQ(unaffected, firstList[2]->renderer());

    renderView()->viewDisplayList().invalidate(second);
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));

    const PaintList& secondList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)3, secondList.size());
    EXPECT_EQ(second, secondList[0]->renderer());
    EXPECT_EQ(first, secondList[1]->renderer());
    EXPECT_EQ(unaffected, secondList[2]->renderer());
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

    const PaintList& firstList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)2, firstList.size());
    EXPECT_EQ(first, firstList[0]->renderer());
    EXPECT_EQ(second, firstList[1]->renderer());

    renderView()->viewDisplayList().invalidate(third);
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(context, third, PaintPhaseBlockBackground, FloatRect(125, 100, 200, 50));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));

    const PaintList& secondList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)3, secondList.size());
    EXPECT_EQ(first, secondList[0]->renderer());
    EXPECT_EQ(third, secondList[1]->renderer());
    EXPECT_EQ(second, secondList[2]->renderer());
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

    const PaintList& firstList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)9, firstList.size());
    for (int item = 0; item < 9; item += 3) {
        EXPECT_EQ(first, firstList[item % 3 + 0]->renderer());
        EXPECT_EQ(second, firstList[item % 3 + 1]->renderer());
        EXPECT_EQ(third, firstList[item % 3 + 2]->renderer());
    }

    renderView()->viewDisplayList().invalidate(second);
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 200));
    drawRect(context, first, PaintPhaseForeground, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseForeground, FloatRect(100, 100, 50, 200));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 100, 100));
    drawRect(context, second, PaintPhaseOutline, FloatRect(100, 100, 50, 200));

    const PaintList& secondList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)9, secondList.size());
    for (int item = 0; item < 9; item += 3) {
        EXPECT_EQ(first, secondList[item % 3 + 0]->renderer());
        EXPECT_EQ(second, secondList[item % 3 + 1]->renderer());
        EXPECT_EQ(third, secondList[item % 3 + 2]->renderer());
    }

    renderView()->viewDisplayList().invalidate(second);
    const PaintList& thirdList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)6, thirdList.size());
    for (int item = 0; item < 6; item += 2) {
        EXPECT_EQ(first, thirdList[item % 2 + 0]->renderer());
        EXPECT_EQ(third, thirdList[item % 2 + 1]->renderer());
    }
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateAddFirstNoOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext* context = new GraphicsContext(nullptr);

    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    const PaintList& firstList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)2, firstList.size());
    EXPECT_EQ(second, firstList[0]->renderer());
    EXPECT_EQ(second, firstList[1]->renderer());

    renderView()->viewDisplayList().invalidate(first);
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 50));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 50, 50));

    const PaintList& secondList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)4, secondList.size());
    EXPECT_EQ(first, secondList[0]->renderer());
    EXPECT_EQ(first, secondList[1]->renderer());
    EXPECT_EQ(second, secondList[2]->renderer());
    EXPECT_EQ(second, secondList[3]->renderer());

    renderView()->viewDisplayList().invalidate(first);
    const PaintList& thirdList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)2, thirdList.size());
    EXPECT_EQ(second, thirdList[0]->renderer());
    EXPECT_EQ(second, thirdList[1]->renderer());
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateAddFirstOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext* context = new GraphicsContext(nullptr);

    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    const PaintList& firstList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)2, firstList.size());
    EXPECT_EQ(second, firstList[0]->renderer());
    EXPECT_EQ(second, firstList[1]->renderer());

    renderView()->viewDisplayList().invalidate(first);
    renderView()->viewDisplayList().invalidate(second);
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    const PaintList& secondList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)4, secondList.size());
    EXPECT_EQ(first, secondList[0]->renderer());
    EXPECT_EQ(first, secondList[1]->renderer());
    EXPECT_EQ(second, secondList[2]->renderer());
    EXPECT_EQ(second, secondList[3]->renderer());

    renderView()->viewDisplayList().invalidate(first);
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    const PaintList& thirdList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)2, thirdList.size());
    EXPECT_EQ(second, thirdList[0]->renderer());
    EXPECT_EQ(second, thirdList[1]->renderer());
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateAddLastNoOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext* context = new GraphicsContext(nullptr);

    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 50, 50));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 50, 50));

    const PaintList& firstList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)2, firstList.size());
    EXPECT_EQ(first, firstList[0]->renderer());
    EXPECT_EQ(first, firstList[1]->renderer());

    renderView()->viewDisplayList().invalidate(second);
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    const PaintList& secondList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)4, secondList.size());
    EXPECT_EQ(second, secondList[0]->renderer());
    EXPECT_EQ(second, secondList[1]->renderer());
    EXPECT_EQ(first, secondList[2]->renderer());
    EXPECT_EQ(first, secondList[3]->renderer());

    renderView()->viewDisplayList().invalidate(second);
    const PaintList& thirdList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)2, thirdList.size());
    EXPECT_EQ(first, thirdList[0]->renderer());
    EXPECT_EQ(first, thirdList[1]->renderer());
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_UpdateAddLastOverlap)
{
    setBodyInnerHTML("<div id='first'></div><div id='second'></div>");
    RenderObject* first = document().body()->firstChild()->renderer();
    RenderObject* second = document().body()->firstChild()->nextSibling()->renderer();
    GraphicsContext* context = new GraphicsContext(nullptr);

    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));

    const PaintList& firstList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)2, firstList.size());
    EXPECT_EQ(first, firstList[0]->renderer());
    EXPECT_EQ(first, firstList[1]->renderer());

    renderView()->viewDisplayList().invalidate(first);
    renderView()->viewDisplayList().invalidate(second);
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));
    drawRect(context, second, PaintPhaseBlockBackground, FloatRect(200, 200, 50, 50));
    drawRect(context, second, PaintPhaseOutline, FloatRect(200, 200, 50, 50));

    const PaintList& secondList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)4, secondList.size());
    EXPECT_EQ(first, secondList[0]->renderer());
    EXPECT_EQ(first, secondList[1]->renderer());
    EXPECT_EQ(second, secondList[2]->renderer());
    EXPECT_EQ(second, secondList[3]->renderer());

    renderView()->viewDisplayList().invalidate(first);
    renderView()->viewDisplayList().invalidate(second);
    drawRect(context, first, PaintPhaseBlockBackground, FloatRect(100, 100, 150, 150));
    drawRect(context, first, PaintPhaseOutline, FloatRect(100, 100, 150, 150));

    const PaintList& thirdList = renderView()->viewDisplayList().paintList();
    EXPECT_EQ((size_t)2, thirdList.size());
    EXPECT_EQ(first, thirdList[0]->renderer());
    EXPECT_EQ(first, thirdList[1]->renderer());
}

}

}
