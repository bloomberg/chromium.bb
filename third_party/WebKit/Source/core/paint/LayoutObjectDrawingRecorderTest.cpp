// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include <gtest/gtest.h>

namespace blink {

class LayoutObjectDrawingRecorderTest : public RenderingTest {
public:
    LayoutObjectDrawingRecorderTest() : m_layoutView(nullptr) { }

protected:
    LayoutView* layoutView() { return m_layoutView; }
    DisplayItemList& rootDisplayItemList() { return *layoutView()->layer()->graphicsLayerBacking()->displayItemList(); }
    const Vector<OwnPtr<DisplayItem>>& newPaintListBeforeUpdate() { return rootDisplayItemList().m_newPaints; }

private:
    virtual void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(true);
        ASSERT(RuntimeEnabledFeatures::slimmingPaintDisplayItemCacheEnabled());

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

void drawNothing(GraphicsContext* context, LayoutView* renderer, PaintPhase phase, const FloatRect& bound)
{
    LayoutObjectDrawingRecorder drawingRecorder(context, *renderer, phase, bound);

    // Redundant when there's nothing to draw but we must always do this check.
    if (drawingRecorder.canUseCachedDrawing())
        return;
}

void drawRect(GraphicsContext* context, LayoutView* renderer, PaintPhase phase, const FloatRect& bound)
{
    LayoutObjectDrawingRecorder drawingRecorder(context, *renderer, phase, bound);
    if (drawingRecorder.canUseCachedDrawing())
        return;
    IntRect rect(0, 0, 10, 10);
    context->drawRect(rect);
}


TEST_F(LayoutObjectDrawingRecorderTest, Nothing)
{
    GraphicsContext context(nullptr, &rootDisplayItemList());
    FloatRect bound = layoutView()->viewRect();
    EXPECT_EQ((size_t)0, rootDisplayItemList().paintList().size());

    drawNothing(&context, layoutView(), PaintPhaseForeground, bound);
    rootDisplayItemList().endNewPaints();
    EXPECT_EQ((size_t)0, rootDisplayItemList().paintList().size());
}

TEST_F(LayoutObjectDrawingRecorderTest, Rect)
{
    GraphicsContext context(nullptr, &rootDisplayItemList());
    FloatRect bound = layoutView()->viewRect();
    drawRect(&context, layoutView(), PaintPhaseForeground, bound);
    rootDisplayItemList().endNewPaints();
    EXPECT_EQ((size_t)1, rootDisplayItemList().paintList().size());
    EXPECT_TRUE(rootDisplayItemList().paintList()[0]->isDrawing());
}

TEST_F(LayoutObjectDrawingRecorderTest, Cached)
{
    GraphicsContext context(nullptr, &rootDisplayItemList());
    FloatRect bound = layoutView()->viewRect();
    drawNothing(&context, layoutView(), PaintPhaseBlockBackground, bound);
    drawRect(&context, layoutView(), PaintPhaseForeground, bound);
    rootDisplayItemList().endNewPaints();
    EXPECT_EQ((size_t)1, rootDisplayItemList().paintList().size());
    EXPECT_TRUE(rootDisplayItemList().paintList()[0]->isDrawing());

    drawNothing(&context, layoutView(), PaintPhaseBlockBackground, bound);
    drawRect(&context, layoutView(), PaintPhaseForeground, bound);
    EXPECT_EQ((size_t)2, newPaintListBeforeUpdate().size());
    EXPECT_TRUE(newPaintListBeforeUpdate()[0]->isCached());
    EXPECT_TRUE(newPaintListBeforeUpdate()[1]->isCached());
    rootDisplayItemList().endNewPaints();
    EXPECT_EQ((size_t)1, rootDisplayItemList().paintList().size());
    EXPECT_TRUE(rootDisplayItemList().paintList()[0]->isDrawing());
}

}
