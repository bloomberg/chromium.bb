// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/DrawingRecorder.h"

#include "core/rendering/RenderView.h"
#include "core/rendering/RenderingTestHelper.h"
#include "platform/graphics/GraphicsContext.h"
#include <gtest/gtest.h>

namespace blink {
namespace {

class DrawingRecorderTest : public RenderingTest {
public:
    DrawingRecorderTest() : m_renderView(nullptr) { }

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

void drawNothing(GraphicsContext* context, RenderView* renderer, PaintPhase phase, const FloatRect& bound)
{
    DrawingRecorder drawingRecorder(context, renderer, phase, bound);
}

void drawRect(GraphicsContext* context, RenderView* renderer, PaintPhase phase, const FloatRect& bound)
{
    DrawingRecorder drawingRecorder(context, renderer, phase, bound);
    IntRect rect(0, 0, 10, 10);
    context->drawRect(rect);
}


TEST_F(DrawingRecorderTest, DrawingRecorderTest_Nothing)
{
    GraphicsContext* context = new GraphicsContext(nullptr);
    FloatRect bound = renderView()->viewRect();
    EXPECT_EQ((size_t)0, renderView()->viewDisplayList().paintList().size());

    drawNothing(context, renderView(), PaintPhaseForeground, bound);
    EXPECT_EQ((size_t)0, renderView()->viewDisplayList().paintList().size());
}

TEST_F(DrawingRecorderTest, DrawingRecorderTest_Rect)
{
    GraphicsContext* context = new GraphicsContext(nullptr);
    FloatRect bound = renderView()->viewRect();
    drawRect(context, renderView(), PaintPhaseForeground, bound);
    EXPECT_EQ((size_t)1, renderView()->viewDisplayList().paintList().size());
}

}
}
