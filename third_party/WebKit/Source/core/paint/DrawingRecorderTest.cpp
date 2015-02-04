// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/RenderDrawingRecorder.h"

#include "core/layout/Layer.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/RenderingTestHelper.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include <gtest/gtest.h>

namespace blink {
namespace {

class DrawingRecorderTest : public RenderingTest {
public:
    DrawingRecorderTest() : m_renderView(nullptr) { }

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

void drawNothing(GraphicsContext* context, RenderView* renderer, PaintPhase phase, const FloatRect& bound)
{
    RenderDrawingRecorder drawingRecorder(context, *renderer, phase, bound);
}

void drawRect(GraphicsContext* context, RenderView* renderer, PaintPhase phase, const FloatRect& bound)
{
    RenderDrawingRecorder drawingRecorder(context, *renderer, phase, bound);
    if (drawingRecorder.canUseCachedDrawing())
        return;
    IntRect rect(0, 0, 10, 10);
    context->drawRect(rect);
}


TEST_F(DrawingRecorderTest, DrawingRecorderTest_Nothing)
{
    GraphicsContext context(nullptr, &rootDisplayItemList());
    FloatRect bound = renderView()->viewRect();
    EXPECT_EQ((size_t)0, rootDisplayItemList().paintList().size());

    drawNothing(&context, renderView(), PaintPhaseForeground, bound);
    rootDisplayItemList().endNewPaints();
    EXPECT_EQ((size_t)1, rootDisplayItemList().paintList().size());
}

TEST_F(DrawingRecorderTest, DrawingRecorderTest_Rect)
{
    GraphicsContext context(nullptr, &rootDisplayItemList());
    FloatRect bound = renderView()->viewRect();
    drawRect(&context, renderView(), PaintPhaseForeground, bound);
    rootDisplayItemList().endNewPaints();
    EXPECT_EQ((size_t)1, rootDisplayItemList().paintList().size());
    EXPECT_TRUE(rootDisplayItemList().paintList()[0]->isDrawing());
}

TEST_F(DrawingRecorderTest, DrawingRecorderTest_Cached)
{
    GraphicsContext context(nullptr, &rootDisplayItemList());
    FloatRect bound = renderView()->viewRect();
    drawNothing(&context, renderView(), PaintPhaseBlockBackground, bound);
    drawRect(&context, renderView(), PaintPhaseForeground, bound);
    rootDisplayItemList().endNewPaints();
    EXPECT_EQ((size_t)2, rootDisplayItemList().paintList().size());
    EXPECT_TRUE(rootDisplayItemList().paintList()[1]->isDrawing());

    drawNothing(&context, renderView(), PaintPhaseBlockBackground, bound);
    drawRect(&context, renderView(), PaintPhaseForeground, bound);
    rootDisplayItemList().endNewPaints();
    EXPECT_EQ((size_t)2, rootDisplayItemList().paintList().size());
    EXPECT_TRUE(rootDisplayItemList().paintList()[1]->isDrawing());
}

}
}
