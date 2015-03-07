// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/LayerClipRecorder.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/LayerCompositor.h"
#include "core/paint/RenderDrawingRecorder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include <gtest/gtest.h>

namespace blink {
namespace {

class LayerClipRecorderTest : public RenderingTest {
public:
    LayerClipRecorderTest() : m_layoutView(nullptr) { }

protected:
    LayoutView* layoutView() { return m_layoutView; }
    DisplayItemList& rootDisplayItemList() { return *layoutView()->layer()->graphicsLayerBacking()->displayItemList(); }

private:
    virtual void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(true);

        RenderingTest::SetUp();
        enableCompositing();

        m_layoutView = document().view()->layoutView();
        ASSERT_TRUE(m_layoutView);
    }

    LayoutView* m_layoutView;
};

void drawEmptyClip(GraphicsContext* context, LayoutView* renderer, PaintPhase phase, const FloatRect& bound)
{
    LayoutRect rect(1, 1, 9, 9);
    ClipRect clipRect(rect);
    LayerClipRecorder LayerClipRecorder(renderer->compositor()->rootLayer()->layoutObject(), context, DisplayItem::ClipLayerForeground, clipRect, 0, LayoutPoint(), PaintLayerFlags());
}

void drawRectInClip(GraphicsContext* context, LayoutView* renderer, PaintPhase phase, const FloatRect& bound)
{
    IntRect rect(1, 1, 9, 9);
    ClipRect clipRect((LayoutRect(rect)));
    LayerClipRecorder LayerClipRecorder(renderer->compositor()->rootLayer()->layoutObject(), context, DisplayItem::ClipLayerForeground, clipRect, 0, LayoutPoint(), PaintLayerFlags());
    RenderDrawingRecorder drawingRecorder(context, *renderer, phase, bound);
    if (!drawingRecorder.canUseCachedDrawing())
        context->drawRect(rect);
}

TEST_F(LayerClipRecorderTest, Single)
{
    GraphicsContext context(nullptr, &rootDisplayItemList());
    FloatRect bound = layoutView()->viewRect();
    EXPECT_EQ((size_t)0, rootDisplayItemList().paintList().size());

    drawRectInClip(&context, layoutView(), PaintPhaseForeground, bound);
    rootDisplayItemList().endNewPaints();
    EXPECT_EQ((size_t)3, rootDisplayItemList().paintList().size());
    EXPECT_TRUE(rootDisplayItemList().paintList()[0]->isClip());
    EXPECT_TRUE(rootDisplayItemList().paintList()[1]->isDrawing());
    EXPECT_TRUE(rootDisplayItemList().paintList()[2]->isEndClip());
}

TEST_F(LayerClipRecorderTest, Empty)
{
    GraphicsContext context(nullptr, &rootDisplayItemList());
    FloatRect bound = layoutView()->viewRect();
    EXPECT_EQ((size_t)0, rootDisplayItemList().paintList().size());

    drawEmptyClip(&context, layoutView(), PaintPhaseForeground, bound);
    rootDisplayItemList().endNewPaints();
    EXPECT_EQ((size_t)0, rootDisplayItemList().paintList().size());
}

}
}
