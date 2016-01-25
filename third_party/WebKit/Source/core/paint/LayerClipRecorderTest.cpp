// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/LayerClipRecorder.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/PaintController.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class LayerClipRecorderTest : public RenderingTest {
public:
    LayerClipRecorderTest()
        : m_layoutView(nullptr) { }

protected:
    LayoutView& layoutView() { return *m_layoutView; }
    PaintController& rootPaintController() { return layoutView().layer()->graphicsLayerBacking()->paintController(); }

private:
    void SetUp() override
    {
        RenderingTest::SetUp();
        enableCompositing();

        m_layoutView = document().view()->layoutView();
        ASSERT_TRUE(m_layoutView);
    }

    LayoutView* m_layoutView;
};

void drawEmptyClip(GraphicsContext& context, LayoutView& layoutView, PaintPhase phase)
{
    LayoutRect rect(1, 1, 9, 9);
    ClipRect clipRect(rect);
    LayerClipRecorder LayerClipRecorder(context, *layoutView.compositor()->rootLayer()->layoutObject(), DisplayItem::ClipLayerForeground, clipRect, 0, LayoutPoint(), PaintLayerFlags());
}

void drawRectInClip(GraphicsContext& context, LayoutView& layoutView, PaintPhase phase, const LayoutRect& bound)
{
    IntRect rect(1, 1, 9, 9);
    ClipRect clipRect((LayoutRect(rect)));
    LayerClipRecorder LayerClipRecorder(context, *layoutView.compositor()->rootLayer()->layoutObject(), DisplayItem::ClipLayerForeground, clipRect, 0, LayoutPoint(), PaintLayerFlags());
    if (!LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(context, layoutView, phase, LayoutPoint())) {
        LayoutObjectDrawingRecorder drawingRecorder(context, layoutView, phase, bound, LayoutPoint());
        context.drawRect(rect);
    }
}

TEST_F(LayerClipRecorderTest, Single)
{
    rootPaintController().invalidateAll();
    GraphicsContext context(rootPaintController());
    LayoutRect bound = layoutView().viewRect();
    EXPECT_EQ((size_t)0, rootPaintController().displayItemList().size());

    drawRectInClip(context, layoutView(), PaintPhaseForeground, bound);
    rootPaintController().commitNewDisplayItems();
    EXPECT_EQ((size_t)3, rootPaintController().displayItemList().size());
    EXPECT_TRUE(DisplayItem::isClipType(rootPaintController().displayItemList()[0].type()));
    EXPECT_TRUE(DisplayItem::isDrawingType(rootPaintController().displayItemList()[1].type()));
    EXPECT_TRUE(DisplayItem::isEndClipType(rootPaintController().displayItemList()[2].type()));
}

TEST_F(LayerClipRecorderTest, Empty)
{
    rootPaintController().invalidateAll();
    GraphicsContext context(rootPaintController());
    EXPECT_EQ((size_t)0, rootPaintController().displayItemList().size());

    drawEmptyClip(context, layoutView(), PaintPhaseForeground);
    rootPaintController().commitNewDisplayItems();
    EXPECT_EQ((size_t)0, rootPaintController().displayItemList().size());
}

} // namespace
} // namespace blink
