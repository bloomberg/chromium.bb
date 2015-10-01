// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/paint/DisplayItemListPaintTest.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

using DeprecatedPaintLayerPainterTest = DisplayItemListPaintTest;
using DeprecatedPaintLayerPainterTestForSlimmingPaintV2 = DisplayItemListPaintTestForSlimmingPaintV2;

TEST_F(DeprecatedPaintLayerPainterTest, CachedSubsequence)
{
    RuntimeEnabledFeatures::setSlimmingPaintSubsequenceCachingEnabled(true);

    setBodyInnerHTML(
        "<div id='container1' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content1' style='position: absolute; width: 100px; height: 100px; background-color: red'></div>"
        "</div>"
        "<div id='container2' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content2' style='position: absolute; width: 100px; height: 100px; background-color: green'></div>"
        "</div>");
    document().view()->updateAllLifecyclePhases();

    DeprecatedPaintLayer& rootLayer = *layoutView().layer();
    DeprecatedPaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    LayoutObject& container1 = *document().getElementById("container1")->layoutObject();
    DeprecatedPaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
    LayoutObject& content1 = *document().getElementById("content1")->layoutObject();
    LayoutObject& container2 = *document().getElementById("container2")->layoutObject();
    DeprecatedPaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
    LayoutObject& content2 = *document().getElementById("content2")->layoutObject();

    GraphicsContext context(&rootDisplayItemList());
    DeprecatedPaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 800, 600), GlobalPaintNormalPhase, LayoutSize());
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 13,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(htmlLayer, subsequenceType),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(container1Layer, subsequenceType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, endSubsequenceType),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(container2Layer, subsequenceType),
        TestDisplayItem(content2, backgroundType),
        TestDisplayItem(container2Layer, endSubsequenceType),
        TestDisplayItem(htmlLayer, endSubsequenceType),
        TestDisplayItem(rootLayer, endSubsequenceType));

    toHTMLElement(content1.node())->setAttribute(HTMLNames::styleAttr, "position: absolute; width: 100px; height: 100px; background-color: green");
    document().view()->updateAllLifecyclePhases();
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);

    EXPECT_DISPLAY_LIST(rootDisplayItemList().newDisplayItems(), 11,
        TestDisplayItem(layoutView(), cachedBackgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(htmlLayer, subsequenceType),
        TestDisplayItem(container1, cachedBackgroundType),
        TestDisplayItem(container1Layer, subsequenceType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, endSubsequenceType),
        TestDisplayItem(container2, cachedBackgroundType),
        TestDisplayItem(container2Layer, cachedSubsequenceType),
        TestDisplayItem(htmlLayer, endSubsequenceType),
        TestDisplayItem(rootLayer, endSubsequenceType));

    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 13,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(htmlLayer, subsequenceType),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(container1Layer, subsequenceType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, endSubsequenceType),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(container2Layer, subsequenceType),
        TestDisplayItem(content2, backgroundType),
        TestDisplayItem(container2Layer, endSubsequenceType),
        TestDisplayItem(htmlLayer, endSubsequenceType),
        TestDisplayItem(rootLayer, endSubsequenceType));

    // Repeated painting should just generate the root cached subsequence.
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    EXPECT_DISPLAY_LIST(rootDisplayItemList().newDisplayItems(), 2,
        TestDisplayItem(layoutView(), cachedBackgroundType),
        TestDisplayItem(rootLayer, cachedSubsequenceType));

    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 13,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(htmlLayer, subsequenceType),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(container1Layer, subsequenceType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, endSubsequenceType),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(container2Layer, subsequenceType),
        TestDisplayItem(content2, backgroundType),
        TestDisplayItem(container2Layer, endSubsequenceType),
        TestDisplayItem(htmlLayer, endSubsequenceType),
        TestDisplayItem(rootLayer, endSubsequenceType));
}

TEST_F(DeprecatedPaintLayerPainterTest, CachedSubsequenceOnInterestRectChange)
{
    RuntimeEnabledFeatures::setSlimmingPaintSubsequenceCachingEnabled(true);

    setBodyInnerHTML(
        "<div id='container1' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content1' style='position: absolute; width: 100px; height: 100px; background-color: green'></div>"
        "</div>"
        "<div id='container2' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content2a' style='position: absolute; width: 100px; height: 100px; background-color: green'></div>"
        "  <div id='content2b' style='position: absolute; top: 200px; width: 100px; height: 100px; background-color: green'></div>"
        "</div>"
        "<div id='container3' style='position: absolute; z-index: 2; left: 300px; top: 0; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content3' style='position: absolute; width: 200px; height: 200px; background-color: green'></div>"
        "</div>");
    rootDisplayItemList().invalidateAll();

    DeprecatedPaintLayer& rootLayer = *layoutView().layer();
    DeprecatedPaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    LayoutObject& container1 = *document().getElementById("container1")->layoutObject();
    DeprecatedPaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
    LayoutObject& content1 = *document().getElementById("content1")->layoutObject();
    LayoutObject& container2 = *document().getElementById("container2")->layoutObject();
    DeprecatedPaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
    LayoutObject& content2a = *document().getElementById("content2a")->layoutObject();
    LayoutObject& container3 = *document().getElementById("container3")->layoutObject();
    DeprecatedPaintLayer& container3Layer = *toLayoutBoxModelObject(container3).layer();
    LayoutObject& content3 = *document().getElementById("content3")->layoutObject();

    document().view()->updateAllLifecyclePhases();
    GraphicsContext context(&rootDisplayItemList());
    DeprecatedPaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 400, 300), GlobalPaintNormalPhase, LayoutSize());
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    // Container1 is fully in the interest rect;
    // Container2 is partly (including its stacking chidren) in the interest rect;
    // Content2b is out of the interest rect and output nothing;
    // Container3 is partly in the interest rect.
    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 17,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(htmlLayer, subsequenceType),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(container1Layer, subsequenceType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, endSubsequenceType),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(container2Layer, subsequenceType),
        TestDisplayItem(content2a, backgroundType),
        TestDisplayItem(container2Layer, endSubsequenceType),
        TestDisplayItem(container3, backgroundType),
        TestDisplayItem(container3Layer, subsequenceType),
        TestDisplayItem(content3, backgroundType),
        TestDisplayItem(container3Layer, endSubsequenceType),
        TestDisplayItem(htmlLayer, endSubsequenceType),
        TestDisplayItem(rootLayer, endSubsequenceType));

    // Container1 becomes partly in the interest rect, but uses cached subsequence
    // because it was fully painted before;
    // Container2's intersection with the interest rect changes;
    // Content2b is out of the interest rect and outputs nothing;
    // Container3 becomes out of the interest rect and outputs nothing.
    DeprecatedPaintLayerPaintingInfo paintingInfo1(&rootLayer, LayoutRect(0, 100, 300, 300), GlobalPaintNormalPhase, LayoutSize());
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo1, PaintLayerPaintingCompositingAllPhases);

    EXPECT_DISPLAY_LIST(rootDisplayItemList().newDisplayItems(), 11,
        TestDisplayItem(layoutView(), cachedBackgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(htmlLayer, subsequenceType),
        TestDisplayItem(container1, cachedBackgroundType),
        TestDisplayItem(container1Layer, cachedSubsequenceType),
        TestDisplayItem(container2, cachedBackgroundType),
        TestDisplayItem(container2Layer, subsequenceType),
        TestDisplayItem(content2a, cachedBackgroundType),
        TestDisplayItem(container2Layer, endSubsequenceType),
        TestDisplayItem(htmlLayer, endSubsequenceType),
        TestDisplayItem(rootLayer, endSubsequenceType));

    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 13,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(htmlLayer, subsequenceType),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(container1Layer, subsequenceType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, endSubsequenceType),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(container2Layer, subsequenceType),
        TestDisplayItem(content2a, backgroundType),
        TestDisplayItem(container2Layer, endSubsequenceType),
        TestDisplayItem(htmlLayer, endSubsequenceType),
        TestDisplayItem(rootLayer, endSubsequenceType));
}

TEST_F(DeprecatedPaintLayerPainterTestForSlimmingPaintV2, CachedSubsequence)
{
    setBodyInnerHTML(
        "<div id='container1' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content1' style='position: absolute; width: 100px; height: 100px; background-color: red'></div>"
        "</div>"
        "<div id='container2' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content2' style='position: absolute; width: 100px; height: 100px; background-color: green'></div>"
        "</div>");
    document().view()->updateAllLifecyclePhases();

    DeprecatedPaintLayer& rootLayer = *layoutView().layer();
    DeprecatedPaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    LayoutObject& container1 = *document().getElementById("container1")->layoutObject();
    DeprecatedPaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
    LayoutObject& content1 = *document().getElementById("content1")->layoutObject();
    LayoutObject& container2 = *document().getElementById("container2")->layoutObject();
    DeprecatedPaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
    LayoutObject& content2 = *document().getElementById("content2")->layoutObject();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 13,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(htmlLayer, subsequenceType),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(container1Layer, subsequenceType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, endSubsequenceType),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(container2Layer, subsequenceType),
        TestDisplayItem(content2, backgroundType),
        TestDisplayItem(container2Layer, endSubsequenceType),
        TestDisplayItem(htmlLayer, endSubsequenceType),
        TestDisplayItem(rootLayer, endSubsequenceType));

    toHTMLElement(content1.node())->setAttribute(HTMLNames::styleAttr, "position: absolute; width: 100px; height: 100px; background-color: green");
    updateLifecyclePhasesToPaintClean(LayoutRect::infiniteRect());

    EXPECT_DISPLAY_LIST(rootDisplayItemList().newDisplayItems(), 11,
        TestDisplayItem(layoutView(), cachedBackgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(htmlLayer, subsequenceType),
        TestDisplayItem(container1, cachedBackgroundType),
        TestDisplayItem(container1Layer, subsequenceType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, endSubsequenceType),
        TestDisplayItem(container2, cachedBackgroundType),
        TestDisplayItem(container2Layer, cachedSubsequenceType),
        TestDisplayItem(htmlLayer, endSubsequenceType),
        TestDisplayItem(rootLayer, endSubsequenceType));

    compositeForSlimmingPaintV2();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 13,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(htmlLayer, subsequenceType),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(container1Layer, subsequenceType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, endSubsequenceType),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(container2Layer, subsequenceType),
        TestDisplayItem(content2, backgroundType),
        TestDisplayItem(container2Layer, endSubsequenceType),
        TestDisplayItem(htmlLayer, endSubsequenceType),
        TestDisplayItem(rootLayer, endSubsequenceType));

    // Repeated painting should just generate the root cached subsequence.
    setNeedsDisplayWithoutInvalidationForRoot();
    updateLifecyclePhasesToPaintClean();
    EXPECT_DISPLAY_LIST(rootDisplayItemList().newDisplayItems(), 2,
        TestDisplayItem(layoutView(), cachedBackgroundType),
        TestDisplayItem(rootLayer, cachedSubsequenceType));

    compositeForSlimmingPaintV2();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 13,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(htmlLayer, subsequenceType),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(container1Layer, subsequenceType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, endSubsequenceType),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(container2Layer, subsequenceType),
        TestDisplayItem(content2, backgroundType),
        TestDisplayItem(container2Layer, endSubsequenceType),
        TestDisplayItem(htmlLayer, endSubsequenceType),
        TestDisplayItem(rootLayer, endSubsequenceType));
}

TEST_F(DeprecatedPaintLayerPainterTestForSlimmingPaintV2, CachedSubsequenceOnInterestRectChange)
{
    setBodyInnerHTML(
        "<div id='container1' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content1' style='position: absolute; width: 100px; height: 100px; background-color: green'></div>"
        "</div>"
        "<div id='container2' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content2a' style='position: absolute; width: 100px; height: 100px; background-color: green'></div>"
        "  <div id='content2b' style='position: absolute; top: 200px; width: 100px; height: 100px; background-color: green'></div>"
        "</div>"
        "<div id='container3' style='position: absolute; z-index: 2; left: 300px; top: 0; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content3' style='position: absolute; width: 200px; height: 200px; background-color: green'></div>"
        "</div>");
    setNeedsDisplayForRoot();

    DeprecatedPaintLayer& rootLayer = *layoutView().layer();
    DeprecatedPaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    LayoutObject& container1 = *document().getElementById("container1")->layoutObject();
    DeprecatedPaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
    LayoutObject& content1 = *document().getElementById("content1")->layoutObject();
    LayoutObject& container2 = *document().getElementById("container2")->layoutObject();
    DeprecatedPaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
    LayoutObject& content2a = *document().getElementById("content2a")->layoutObject();
    LayoutObject& container3 = *document().getElementById("container3")->layoutObject();
    DeprecatedPaintLayer& container3Layer = *toLayoutBoxModelObject(container3).layer();
    LayoutObject& content3 = *document().getElementById("content3")->layoutObject();

    document().view()->updateAllLifecyclePhases(LayoutRect(0, 0, 400, 300));

    // Container1 is fully in the interest rect;
    // Container2 is partly (including its stacking chidren) in the interest rect;
    // Content2b is out of the interest rect and output nothing;
    // Container3 is partly in the interest rect.
    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 17,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(htmlLayer, subsequenceType),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(container1Layer, subsequenceType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, endSubsequenceType),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(container2Layer, subsequenceType),
        TestDisplayItem(content2a, backgroundType),
        TestDisplayItem(container2Layer, endSubsequenceType),
        TestDisplayItem(container3, backgroundType),
        TestDisplayItem(container3Layer, subsequenceType),
        TestDisplayItem(content3, backgroundType),
        TestDisplayItem(container3Layer, endSubsequenceType),
        TestDisplayItem(htmlLayer, endSubsequenceType),
        TestDisplayItem(rootLayer, endSubsequenceType));

    setNeedsDisplayWithoutInvalidationForRoot();
    
    // Container1 becomes partly in the interest rect, but uses cached subsequence
    // because it was fully painted before;
    // Container2's intersection with the interest rect changes;
    // Content2b is out of the interest rect and outputs nothing;
    // Container3 becomes out of the interest rect and outputs nothing.
    updateLifecyclePhasesToPaintClean(LayoutRect(0, 100, 300, 300));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().newDisplayItems(), 11,
        TestDisplayItem(layoutView(), cachedBackgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(htmlLayer, subsequenceType),
        TestDisplayItem(container1, cachedBackgroundType),
        TestDisplayItem(container1Layer, cachedSubsequenceType),
        TestDisplayItem(container2, cachedBackgroundType),
        TestDisplayItem(container2Layer, subsequenceType),
        TestDisplayItem(content2a, cachedBackgroundType),
        TestDisplayItem(container2Layer, endSubsequenceType),
        TestDisplayItem(htmlLayer, endSubsequenceType),
        TestDisplayItem(rootLayer, endSubsequenceType));

    compositeForSlimmingPaintV2();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 13,
        TestDisplayItem(layoutView(), backgroundType),
        TestDisplayItem(rootLayer, subsequenceType),
        TestDisplayItem(htmlLayer, subsequenceType),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(container1Layer, subsequenceType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, endSubsequenceType),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(container2Layer, subsequenceType),
        TestDisplayItem(content2a, backgroundType),
        TestDisplayItem(container2Layer, endSubsequenceType),
        TestDisplayItem(htmlLayer, endSubsequenceType),
        TestDisplayItem(rootLayer, endSubsequenceType));
}

} // namespace blink
