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

void drawRect(GraphicsContext* context, RenderView* renderer, PaintPhase phase, const FloatRect& bound)
{
    DrawingRecorder drawingRecorder(context, renderer, phase, bound);
    IntRect rect(0, 0, 10, 10);
    context->drawRect(rect);
}

void drawClippedRect(GraphicsContext* context, RenderView* renderer, PaintPhase phase, const FloatRect& bound)
{
    IntRect rect(1, 1, 9, 9);
    ClipRect clipRect(rect);
    ClipRecorder clipRecorder(renderer->compositor()->rootRenderLayer(), context, DisplayItem::ClipLayerForeground, clipRect);
    drawRect(context, renderer, phase, bound);
}

TEST_F(ViewDisplayListTest, ViewDisplayListTest_NestedRecorders)
{
    GraphicsContext* context = new GraphicsContext(nullptr);
    FloatRect bound = renderView()->viewRect();

    drawClippedRect(context, renderView(), PaintPhaseForeground, bound);
    EXPECT_EQ((size_t)3, renderView()->viewDisplayList().paintList().size());

    // TODO(schenney): Check that the IDs are what we expect.
}

}
}
