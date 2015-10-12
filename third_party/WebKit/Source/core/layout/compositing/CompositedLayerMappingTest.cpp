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

} // namespace blink
