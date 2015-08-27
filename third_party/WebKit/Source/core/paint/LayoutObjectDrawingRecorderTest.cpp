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
    void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(true);

        RenderingTest::SetUp();
        enableCompositing();

        m_layoutView = document().view()->layoutView();
        ASSERT_TRUE(m_layoutView);
    }

    void TearDown() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(m_originalSlimmingPaintEnabled);
    }

    LayoutView* m_layoutView;
    bool m_originalSlimmingPaintEnabled;
};

namespace {

void drawNothing(GraphicsContext& context, const LayoutView& layoutView, PaintPhase phase, const LayoutRect& bound)
{
    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(context, layoutView, phase))
        return;

    LayoutObjectDrawingRecorder drawingRecorder(context, layoutView, phase, bound);
}

void drawRect(GraphicsContext& context, LayoutView& layoutView, PaintPhase phase, const LayoutRect& bound)
{
    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(context, layoutView, phase))
        return;
    LayoutObjectDrawingRecorder drawingRecorder(context, layoutView, phase, bound);
    IntRect rect(0, 0, 10, 10);
    context.drawRect(rect);
}

bool isDrawing(const DisplayItem& item)
{
    return DisplayItem::isDrawingType(item.type());
}

bool isCached(const DisplayItem& item)
{
    return DisplayItem::isCachedType(item.type());
}

TEST_F(LayoutObjectDrawingRecorderTest, Nothing)
{
    GraphicsContext context(&rootDisplayItemList());
    LayoutRect bound = layoutView().viewRect();
    EXPECT_EQ((size_t)0, rootDisplayItemList().displayItems().size());

    drawNothing(context, layoutView(), PaintPhaseForeground, bound);
    rootDisplayItemList().commitNewDisplayItems();
    EXPECT_EQ((size_t)1, rootDisplayItemList().displayItems().size());
    const auto& item = rootDisplayItemList().displayItems()[0];
    ASSERT_TRUE(isDrawing(item));
    EXPECT_FALSE(static_cast<const DrawingDisplayItem&>(item).picture());
}

TEST_F(LayoutObjectDrawingRecorderTest, Rect)
{
    GraphicsContext context(&rootDisplayItemList());
    LayoutRect bound = layoutView().viewRect();
    drawRect(context, layoutView(), PaintPhaseForeground, bound);
    rootDisplayItemList().commitNewDisplayItems();
    EXPECT_EQ((size_t)1, rootDisplayItemList().displayItems().size());
    EXPECT_TRUE(isDrawing(rootDisplayItemList().displayItems()[0]));
}

TEST_F(LayoutObjectDrawingRecorderTest, Cached)
{
    GraphicsContext context(&rootDisplayItemList());
    LayoutRect bound = layoutView().viewRect();
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

template <typename T>
FloatRect drawAndGetCullRect(DisplayItemList& list, const LayoutObject& layoutObject, const T& bounds)
{
    list.invalidateAll();
    {
        // Draw some things which will produce a non-null picture.
        GraphicsContext context(&list);
        LayoutObjectDrawingRecorder recorder(
            context, layoutObject, DisplayItem::BoxDecorationBackground, bounds);
        context.drawRect(enclosedIntRect(FloatRect(bounds)));
    }
    list.commitNewDisplayItems();
    const auto& drawing = static_cast<const DrawingDisplayItem&>(list.displayItems()[0]);
    return drawing.picture()->cullRect();
}

TEST_F(LayoutObjectDrawingRecorderTest, CullRectMatchesProvidedClip)
{
    // It's safe for the picture's cull rect to be expanded (though doing so
    // excessively may harm performance), but it cannot be contracted.
    // For now, this test expects the two rects to match completely.
    //
    // This rect is chosen so that in the x direction, pixel snapping rounds in
    // the opposite direction to enclosing, and in the y direction, the edges
    // are exactly on a half-pixel boundary. The numbers chosen map nicely to
    // both float and LayoutUnit, to make equality checking reliable.
    FloatRect rect(20.75, -5.5, 5.375, 10);
    EXPECT_EQ(rect, drawAndGetCullRect(rootDisplayItemList(), layoutView(), rect));
    EXPECT_EQ(rect, drawAndGetCullRect(rootDisplayItemList(), layoutView(), LayoutRect(rect)));
}

} // namespace
} // namespace blink
