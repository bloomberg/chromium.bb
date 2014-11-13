// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ClipRecorder.h"

#include "core/rendering/RenderView.h"
#include "core/rendering/RenderingTestHelper.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include <gtest/gtest.h>

namespace blink {
namespace {

class ClipRecorderTest : public RenderingTest {
public:
    ClipRecorderTest() : m_renderView(nullptr) { }

protected:
    RenderView* renderView() { return m_renderView; }
    DisplayItemList& rootDisplayItemList() { return renderView()->layer()->graphicsLayerBacking()->displayItemList(); }

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

void drawClip(GraphicsContext* context, RenderView* renderer, PaintPhase phase, const FloatRect& bound)
{
    IntRect rect(1, 1, 9, 9);
    ClipRect clipRect(rect);
    ClipRecorder clipRecorder(renderer->compositor()->rootRenderLayer(), context, DisplayItem::ClipLayerForeground, clipRect);
}

TEST_F(ClipRecorderTest, ClipRecorderTest_Single)
{
    GraphicsContext* context = new GraphicsContext(nullptr);
    FloatRect bound = renderView()->viewRect();
    EXPECT_EQ((size_t)0, rootDisplayItemList().paintList().size());

    drawClip(context, renderView(), PaintPhaseForeground, bound);
    EXPECT_EQ((size_t)2, rootDisplayItemList().paintList().size());
}

}
}
