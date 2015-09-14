// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/LayerClipRecorder.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/DeprecatedPaintLayerCompositor.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include <gtest/gtest.h>

namespace blink {
namespace {

class LayerClipRecorderTest : public RenderingTest {
public:
    LayerClipRecorderTest()
        : m_layoutView(nullptr) { }

protected:
    LayoutView& layoutView() { return *m_layoutView; }
    DisplayItemList& rootDisplayItemList() { return *layoutView().layer()->graphicsLayerBacking()->displayItemList(); }

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
    GraphicsContext context(&rootDisplayItemList());
    LayoutRect bound = layoutView().viewRect();
    EXPECT_EQ((size_t)0, rootDisplayItemList().displayItems().size());

    drawRectInClip(context, layoutView(), PaintPhaseForeground, bound);
    rootDisplayItemList().commitNewDisplayItems();
    EXPECT_EQ((size_t)3, rootDisplayItemList().displayItems().size());
    EXPECT_TRUE(DisplayItem::isClipType(rootDisplayItemList().displayItems()[0].type()));
    EXPECT_TRUE(DisplayItem::isDrawingType(rootDisplayItemList().displayItems()[1].type()));
    EXPECT_TRUE(DisplayItem::isEndClipType(rootDisplayItemList().displayItems()[2].type()));
}

TEST_F(LayerClipRecorderTest, Empty)
{
    GraphicsContext context(&rootDisplayItemList());
    EXPECT_EQ((size_t)0, rootDisplayItemList().displayItems().size());

    drawEmptyClip(context, layoutView(), PaintPhaseForeground);
    rootDisplayItemList().commitNewDisplayItems();
    EXPECT_EQ((size_t)0, rootDisplayItemList().displayItems().size());
}

}
}
