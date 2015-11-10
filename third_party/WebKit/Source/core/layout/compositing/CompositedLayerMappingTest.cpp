// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/layout/compositing/CompositedLayerMapping.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/paint/PaintLayer.h"
#include <gtest/gtest.h>

namespace blink {

class CompositedLayerMappingTest : public RenderingTest {
public:
    CompositedLayerMappingTest()
        : m_originalSlimmingPaintSynchronizedPaintingEnabled(RuntimeEnabledFeatures::slimmingPaintSynchronizedPaintingEnabled()) { }

protected:
    IntRect computeInterestRect(const GraphicsLayer* graphicsLayer, LayoutObject* owningLayoutObject)
    {
        return CompositedLayerMapping::computeInterestRect(graphicsLayer, owningLayoutObject);
    }

    bool interestRectChangedEnoughToRepaint(const IntRect& previousInterestRect, const IntRect& newInterestRect, const IntSize& layerSize)
    {
        return CompositedLayerMapping::interestRectChangedEnoughToRepaint(previousInterestRect, newInterestRect, layerSize);
    }

private:
    void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintSynchronizedPaintingEnabled(true);

        RenderingTest::SetUp();
        enableCompositing();
        GraphicsLayer::setDrawDebugRedFillForTesting(false);
    }

    void TearDown() override
    {
        GraphicsLayer::setDrawDebugRedFillForTesting(true);
        RuntimeEnabledFeatures::setSlimmingPaintSynchronizedPaintingEnabled(m_originalSlimmingPaintSynchronizedPaintingEnabled);
    }

    bool m_originalSlimmingPaintSynchronizedPaintingEnabled;
};

static void printRect(IntRect rect)
{
    fprintf(stderr, "[x=%d y=%d maxX=%d maxY=%d]\n", rect.x(), rect.y(), rect.maxX(), rect.maxY());
}

static bool checkRectsEqual(const IntRect& expected, const IntRect& actual)
{
    if (expected != actual) {
        fprintf(stderr, "Expected: ");
        printRect(expected);
        fprintf(stderr, "Actual: ");
        printRect(actual);
        return false;
    }
    return true;
}

TEST_F(CompositedLayerMappingTest, SimpleInterestRect)
{
    setBodyInnerHTML("<div id='target' style='width: 200px; height: 200px; will-change: transform'></div>");

    document().view()->updateAllLifecyclePhases();
    Element* element = document().getElementById("target");
    PaintLayer* paintLayer = toLayoutBoxModelObject(element->layoutObject())->layer();
    ASSERT_TRUE(!!paintLayer->graphicsLayerBacking());
    EXPECT_TRUE(checkRectsEqual(IntRect(0, 0, 200, 200), computeInterestRect(paintLayer->graphicsLayerBacking(), paintLayer->layoutObject())));
}

TEST_F(CompositedLayerMappingTest, TallLayerInterestRect)
{
    setBodyInnerHTML("<div id='target' style='width: 200px; height: 10000px; will-change: transform'></div>");

    document().view()->updateAllLifecyclePhases();
    Element* element = document().getElementById("target");
    PaintLayer* paintLayer = toLayoutBoxModelObject(element->layoutObject())->layer();
    ASSERT_TRUE(paintLayer->graphicsLayerBacking());
    // Screen-space visible content rect is [8, 8, 200, 600]. Mapping back to local, adding 4000px in all directions, then
    // clipping, yields this rect.
    EXPECT_TRUE(checkRectsEqual(IntRect(0, 0, 200, 4592), computeInterestRect(paintLayer->graphicsLayerBacking(), paintLayer->layoutObject())));
}

TEST_F(CompositedLayerMappingTest, RotatedInterestRect)
{
    setBodyInnerHTML(
        "<div id='target' style='width: 200px; height: 200px; will-change: transform; transform: rotateZ(45deg)'></div>");

    document().view()->updateAllLifecyclePhases();
    Element* element = document().getElementById("target");
    PaintLayer* paintLayer = toLayoutBoxModelObject(element->layoutObject())->layer();
    ASSERT_TRUE(!!paintLayer->graphicsLayerBacking());
    EXPECT_TRUE(checkRectsEqual(IntRect(0, 0, 200, 200), computeInterestRect(paintLayer->graphicsLayerBacking(), paintLayer->layoutObject())));
}

TEST_F(CompositedLayerMappingTest, 3D90DegRotatedTallInterestRect)
{
    // It's rotated 90 degrees about the X axis, which means its visual content rect is empty, and so the interest rect is the
    // default (0, 0, 4000, 4000) intersected with the layer bounds.
    setBodyInnerHTML(
        "<div id='target' style='width: 200px; height: 10000px; will-change: transform; transform: rotateY(90deg)'></div>");

    document().view()->updateAllLifecyclePhases();
    Element* element = document().getElementById("target");
    PaintLayer* paintLayer = toLayoutBoxModelObject(element->layoutObject())->layer();
    ASSERT_TRUE(!!paintLayer->graphicsLayerBacking());
    EXPECT_TRUE(checkRectsEqual(IntRect(0, 0, 200, 4000), computeInterestRect(paintLayer->graphicsLayerBacking(), paintLayer->layoutObject())));
}

TEST_F(CompositedLayerMappingTest, 3D45DegRotatedTallInterestRect)
{
    setBodyInnerHTML(
        "<div id='target' style='width: 200px; height: 10000px; will-change: transform; transform: rotateY(45deg)'></div>");

    document().view()->updateAllLifecyclePhases();
    Element* element = document().getElementById("target");
    PaintLayer* paintLayer = toLayoutBoxModelObject(element->layoutObject())->layer();
    ASSERT_TRUE(!!paintLayer->graphicsLayerBacking());
    EXPECT_TRUE(checkRectsEqual(IntRect(0, 0, 200, 4592), computeInterestRect(paintLayer->graphicsLayerBacking(), paintLayer->layoutObject())));
}

TEST_F(CompositedLayerMappingTest, RotatedTallInterestRect)
{
    setBodyInnerHTML(
        "<div id='target' style='width: 200px; height: 10000px; will-change: transform; transform: rotateZ(45deg)'></div>");

    document().view()->updateAllLifecyclePhases();
    Element* element = document().getElementById("target");
    PaintLayer* paintLayer = toLayoutBoxModelObject(element->layoutObject())->layer();
    ASSERT_TRUE(!!paintLayer->graphicsLayerBacking());
    EXPECT_TRUE(checkRectsEqual(IntRect(0, 0, 200, 4000), computeInterestRect(paintLayer->graphicsLayerBacking(), paintLayer->layoutObject())));
}

TEST_F(CompositedLayerMappingTest, WideLayerInterestRect)
{
    setBodyInnerHTML("<div id='target' style='width: 10000px; height: 200px; will-change: transform'></div>");

    document().view()->updateAllLifecyclePhases();
    Element* element = document().getElementById("target");
    PaintLayer* paintLayer = toLayoutBoxModelObject(element->layoutObject())->layer();
    ASSERT_TRUE(!!paintLayer->graphicsLayerBacking());
    // Screen-space visible content rect is [8, 8, 800, 200] (the screen is 800x600).
    // Mapping back to local, adding 4000px in all directions, then clipping, yields this rect.
    EXPECT_TRUE(checkRectsEqual(IntRect(0, 0, 4792, 200), computeInterestRect(paintLayer->graphicsLayerBacking(), paintLayer->layoutObject())));
}

TEST_F(CompositedLayerMappingTest, FixedPositionInterestRect)
{
    setBodyInnerHTML(
        "<div id='target' style='width: 300px; height: 400px; will-change: transform; position: fixed; top: 100px; left: 200px;'></div>");

    document().view()->updateAllLifecyclePhases();
    Element* element = document().getElementById("target");
    PaintLayer* paintLayer = toLayoutBoxModelObject(element->layoutObject())->layer();
    ASSERT_TRUE(!!paintLayer->graphicsLayerBacking());
    EXPECT_TRUE(checkRectsEqual(IntRect(0, 0, 300, 400), computeInterestRect(paintLayer->graphicsLayerBacking(), paintLayer->layoutObject())));
}

TEST_F(CompositedLayerMappingTest, LayerOffscreenInterestRect)
{
    setBodyInnerHTML(
        "<div id='target' style='width: 200px; height: 200px; will-change: transform; position: absolute; top: 9000px; left: 0px;'>"
        "</div>");

    document().view()->updateAllLifecyclePhases();
    Element* element = document().getElementById("target");
    PaintLayer* paintLayer = toLayoutBoxModelObject(element->layoutObject())->layer();
    ASSERT_TRUE(!!paintLayer->graphicsLayerBacking());
    // Offscreen layers are painted as usual.
    EXPECT_TRUE(checkRectsEqual(IntRect(0, 0, 200, 200), computeInterestRect(paintLayer->graphicsLayerBacking(), paintLayer->layoutObject())));
}

TEST_F(CompositedLayerMappingTest, ScrollingLayerInterestRect)
{
    setBodyInnerHTML(
        "<style>div::-webkit-scrollbar{ width: 5px; }</style>"
        "<div id='target' style='width: 200px; height: 200px; will-change: transform; overflow: scroll'>"
        "<div style='width: 100px; height: 10000px'></div></div>");

    document().view()->updateAllLifecyclePhases();
    Element* element = document().getElementById("target");
    PaintLayer* paintLayer = toLayoutBoxModelObject(element->layoutObject())->layer();
    ASSERT_TRUE(paintLayer->graphicsLayerBacking());
    // Offscreen layers are painted as usual.
    ASSERT_TRUE(paintLayer->compositedLayerMapping()->scrollingLayer());
    EXPECT_TRUE(checkRectsEqual(IntRect(0, 0, 195, 4592), computeInterestRect(paintLayer->graphicsLayerBackingForScrolling(), paintLayer->layoutObject())));
}

TEST_F(CompositedLayerMappingTest, ClippedBigLayer)
{
    setBodyInnerHTML(
        "<div style='width: 1px; height: 1px; overflow: hidden'>"
        "<div id='target' style='width: 10000px; height: 10000px; will-change: transform'></div></div>");

    document().view()->updateAllLifecyclePhases();
    Element* element = document().getElementById("target");
    PaintLayer* paintLayer = toLayoutBoxModelObject(element->layoutObject())->layer();
    ASSERT_TRUE(paintLayer->graphicsLayerBacking());
    // Offscreen layers are painted as usual.
    EXPECT_TRUE(checkRectsEqual(IntRect(0, 0, 4001, 4001), computeInterestRect(paintLayer->graphicsLayerBacking(), paintLayer->layoutObject())));
}

TEST_F(CompositedLayerMappingTest, ClippingMaskLayer)
{
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        return;

    const AtomicString styleWithoutClipping = "backface-visibility: hidden; width: 200px; height: 200px";
    const AtomicString styleWithBorderRadius = styleWithoutClipping + "; border-radius: 10px";
    const AtomicString styleWithClipPath = styleWithoutClipping + "; -webkit-clip-path: inset(10px)";

    setBodyInnerHTML("<video id='video' src='x' style='" + styleWithoutClipping + "'></video>");

    document().view()->updateAllLifecyclePhases();
    Element* videoElement = document().getElementById("video");
    GraphicsLayer* graphicsLayer = toLayoutBoxModelObject(videoElement->layoutObject())->layer()->graphicsLayerBacking();
    EXPECT_FALSE(graphicsLayer->maskLayer());
    EXPECT_FALSE(graphicsLayer->contentsClippingMaskLayer());

    videoElement->setAttribute(HTMLNames::styleAttr, styleWithBorderRadius);
    document().view()->updateAllLifecyclePhases();
    EXPECT_FALSE(graphicsLayer->maskLayer());
    EXPECT_TRUE(graphicsLayer->contentsClippingMaskLayer());

    videoElement->setAttribute(HTMLNames::styleAttr, styleWithClipPath);
    document().view()->updateAllLifecyclePhases();
    EXPECT_TRUE(graphicsLayer->maskLayer());
    EXPECT_FALSE(graphicsLayer->contentsClippingMaskLayer());

    videoElement->setAttribute(HTMLNames::styleAttr, styleWithoutClipping);
    document().view()->updateAllLifecyclePhases();
    EXPECT_FALSE(graphicsLayer->maskLayer());
    EXPECT_FALSE(graphicsLayer->contentsClippingMaskLayer());
}

TEST_F(CompositedLayerMappingTest, InterestRectChangedEnoughToRepaintEmpty)
{
    IntSize layerSize(1000, 1000);
    // Both empty means there is nothing to do.
    EXPECT_FALSE(interestRectChangedEnoughToRepaint(IntRect(), IntRect(), layerSize));
    // Going from empty to non-empty means we must re-record because it could be the first frame after construction or Clear.
    EXPECT_TRUE(interestRectChangedEnoughToRepaint(IntRect(), IntRect(0, 0, 1, 1), layerSize));
    // Going from non-empty to empty is not special-cased.
    EXPECT_FALSE(interestRectChangedEnoughToRepaint(IntRect(0, 0, 1, 1), IntRect(), layerSize));
}

TEST_F(CompositedLayerMappingTest, InterestRectChangedEnoughToRepaintNotBigEnough)
{
    IntSize layerSize(1000, 1000);
    IntRect previousInterestRect(100, 100, 100, 100);
    EXPECT_FALSE(interestRectChangedEnoughToRepaint(previousInterestRect, IntRect(100, 100, 90, 90), layerSize));
    EXPECT_FALSE(interestRectChangedEnoughToRepaint(previousInterestRect, IntRect(100, 100, 100, 100), layerSize));
    EXPECT_FALSE(interestRectChangedEnoughToRepaint(previousInterestRect, IntRect(1, 1, 200, 200), layerSize));
}

TEST_F(CompositedLayerMappingTest, InterestRectChangedEnoughToRepaintNotBigEnoughButNewAreaTouchesEdge)
{
    IntSize layerSize(500, 500);
    IntRect previousInterestRect(100, 100, 100, 100);
    // Top edge.
    EXPECT_TRUE(interestRectChangedEnoughToRepaint(previousInterestRect, IntRect(100, 0, 100, 200), layerSize));
    // Left edge.
    EXPECT_TRUE(interestRectChangedEnoughToRepaint(previousInterestRect, IntRect(0, 100, 200, 100), layerSize));
    // Bottom edge.
    EXPECT_TRUE(interestRectChangedEnoughToRepaint(previousInterestRect, IntRect(100, 100, 100, 400), layerSize));
    // Right edge.
    EXPECT_TRUE(interestRectChangedEnoughToRepaint(previousInterestRect, IntRect(100, 100, 400, 100), layerSize));
}

// Verifies that having a current viewport that touches a layer edge does not
// force re-recording.
TEST_F(CompositedLayerMappingTest, InterestRectChangedEnoughToRepaintCurrentViewportTouchesEdge)
{
    IntSize layerSize(500, 500);
    IntRect newInterestRect(100, 100, 300, 300);
    // Top edge.
    EXPECT_FALSE(interestRectChangedEnoughToRepaint(IntRect(100, 0, 100, 100), newInterestRect, layerSize));
    // Left edge.
    EXPECT_FALSE(interestRectChangedEnoughToRepaint(IntRect(0, 100, 100, 100), newInterestRect, layerSize));
    // Bottom edge.
    EXPECT_FALSE(interestRectChangedEnoughToRepaint(IntRect(300, 400, 100, 100), newInterestRect, layerSize));
    // Right edge.
    EXPECT_FALSE(interestRectChangedEnoughToRepaint(IntRect(400, 300, 100, 100), newInterestRect, layerSize));
}

TEST_F(CompositedLayerMappingTest, InterestRectChangedEnoughToRepaintScrollScenarios)
{
    IntSize layerSize(1000, 1000);
    IntRect previousInterestRect(100, 100, 100, 100);
    IntRect newInterestRect(previousInterestRect);
    newInterestRect.move(512, 0);
    EXPECT_FALSE(interestRectChangedEnoughToRepaint(previousInterestRect, newInterestRect, layerSize));
    newInterestRect.move(0, 512);
    EXPECT_FALSE(interestRectChangedEnoughToRepaint(previousInterestRect, newInterestRect, layerSize));
    newInterestRect.move(1, 0);
    EXPECT_TRUE(interestRectChangedEnoughToRepaint(previousInterestRect, newInterestRect, layerSize));
    newInterestRect.move(-1, 1);
    EXPECT_TRUE(interestRectChangedEnoughToRepaint(previousInterestRect, newInterestRect, layerSize));
}

} // namespace blink
