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
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include <gtest/gtest.h>

namespace blink {

class LayoutObjectDrawingRecorderTest : public RenderingTest {
public:
    LayoutObjectDrawingRecorderTest()
        : m_layoutView(nullptr)
        , m_originalSlimmingPaintEnabled(RuntimeEnabledFeatures::slimmingPaintEnabled()) { }

protected:
    LayoutView& layoutView() { return *m_layoutView; }
    DisplayItemList& rootDisplayItemList() { return *layoutView().layer()->graphicsLayerBacking()->displayItemList(); }
    const DisplayItems& newDisplayItemsBeforeUpdate() { return rootDisplayItemList().m_newDisplayItems; }

private:
    virtual void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(true);

        RenderingTest::SetUp();
        enableCompositing();

        m_layoutView = document().view()->layoutView();
        ASSERT_TRUE(m_layoutView);
    }

    virtual void TearDown() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(m_originalSlimmingPaintEnabled);
    }

    LayoutView* m_layoutView;
    bool m_originalSlimmingPaintEnabled;
};

namespace {

void drawNothing(GraphicsContext& context, const LayoutView& layoutView, PaintPhase phase, const FloatRect& bound)
{
    LayoutObjectDrawingRecorder drawingRecorder(context, layoutView, phase, bound);

    // Redundant when there's nothing to draw but we must always do this check.
    if (drawingRecorder.canUseCachedDrawing())
        return;
}

void drawRect(GraphicsContext& context, LayoutView& layoutView, PaintPhase phase, const FloatRect& bound)
{
    LayoutObjectDrawingRecorder drawingRecorder(context, layoutView, phase, bound);
    if (drawingRecorder.canUseCachedDrawing())
        return;
    IntRect rect(0, 0, 10, 10);
    context.drawRect(rect);
}

bool isDrawing(const DisplayItems::ItemHandle& item)
{
    return DisplayItem::isDrawingType(item.type());
}

bool isCached(const DisplayItems::ItemHandle& item)
{
    return DisplayItem::isCachedType(item.type());
}

TEST_F(LayoutObjectDrawingRecorderTest, Nothing)
{
    GraphicsContext context(&rootDisplayItemList());
    FloatRect bound = layoutView().viewRect();
    EXPECT_EQ((size_t)0, rootDisplayItemList().displayItems().size());

    drawNothing(context, layoutView(), PaintPhaseForeground, bound);
    rootDisplayItemList().commitNewDisplayItems();
    EXPECT_EQ((size_t)1, rootDisplayItemList().displayItems().size());
    const auto& item = rootDisplayItemList().displayItems()[0];
    ASSERT_TRUE(isDrawing(item));
    EXPECT_FALSE(item.picture());
}

TEST_F(LayoutObjectDrawingRecorderTest, Rect)
{
    GraphicsContext context(&rootDisplayItemList());
    FloatRect bound = layoutView().viewRect();
    drawRect(context, layoutView(), PaintPhaseForeground, bound);
    rootDisplayItemList().commitNewDisplayItems();
    EXPECT_EQ((size_t)1, rootDisplayItemList().displayItems().size());
    EXPECT_TRUE(isDrawing(rootDisplayItemList().displayItems()[0]));
}

TEST_F(LayoutObjectDrawingRecorderTest, Cached)
{
    GraphicsContext context(&rootDisplayItemList());
    FloatRect bound = layoutView().viewRect();
    drawNothing(context, layoutView(), PaintPhaseBlockBackground, bound);
    drawRect(context, layoutView(), PaintPhaseForeground, bound);
    rootDisplayItemList().commitNewDisplayItems();
    EXPECT_EQ((size_t)2, rootDisplayItemList().displayItems().size());
    EXPECT_TRUE(isDrawing(rootDisplayItemList().displayItems()[0]));
    EXPECT_TRUE(isDrawing(rootDisplayItemList().displayItems()[1]));

    drawNothing(context, layoutView(), PaintPhaseBlockBackground, bound);
    drawRect(context, layoutView(), PaintPhaseForeground, bound);
    EXPECT_EQ((size_t)2, newDisplayItemsBeforeUpdate().size());
    EXPECT_TRUE(isCached(newDisplayItemsBeforeUpdate()[0]));
    EXPECT_TRUE(isCached(newDisplayItemsBeforeUpdate()[1]));
    rootDisplayItemList().commitNewDisplayItems();
    EXPECT_EQ((size_t)2, rootDisplayItemList().displayItems().size());
    EXPECT_TRUE(isDrawing(rootDisplayItemList().displayItems()[0]));
    EXPECT_TRUE(isDrawing(rootDisplayItemList().displayItems()[1]));
}

} // namespace
} // namespace blink
