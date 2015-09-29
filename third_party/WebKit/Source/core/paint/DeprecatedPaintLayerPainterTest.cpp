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
        "<div id='container1' style='position: relative; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content1' style='width: 100px; height: 100px; background-color: red'></div>"
        "</div>"
        "<div id='container2' style='position: relative; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content2' style='width: 100px; height: 100px; background-color: green'></div>"
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
        TestDisplayItem(rootLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
        TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));

    toHTMLElement(content1.node())->setAttribute(HTMLNames::styleAttr, "width: 100px; height: 100px; background-color: green");
    document().view()->updateAllLifecyclePhases();
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);

    EXPECT_DISPLAY_LIST(rootDisplayItemList().newDisplayItems(), 10,
        TestDisplayItem(rootLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(layoutView(), DisplayItem::drawingTypeToCachedDrawingType(DisplayItem::BoxDecorationBackground)),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1, DisplayItem::drawingTypeToCachedDrawingType(DisplayItem::BoxDecorationBackground)),
        TestDisplayItem(content1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::CachedSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
        TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));

    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 13,
        TestDisplayItem(rootLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
        TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));

    // Repeated painting should just generate the root cached subsequence.
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    EXPECT_DISPLAY_LIST(rootDisplayItemList().newDisplayItems(), 1,
        TestDisplayItem(rootLayer, DisplayItem::CachedSubsequence));

    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 13,
        TestDisplayItem(rootLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
        TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
}

TEST_F(DeprecatedPaintLayerPainterTest, CachedSubsequenceOnInterestRectChange)
{
    RuntimeEnabledFeatures::setSlimmingPaintSubsequenceCachingEnabled(true);

    setBodyInnerHTML(
        "<div id='container1' style='position: relative; width: 200px; height: 200px; background-color: blue'></div>"
        "<div id='container2' style='position: absolute; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content2' style='position: relative; top: 200px; width: 100px; height: 100px; background-color: green'></div>"
        "</div>"
        "<div id='container3' style='position: absolute; z-index: 2; left: 300px; top: 0; width: 200px; height: 200px; background-color: blue'></div>");
    rootDisplayItemList().invalidateAll();

    DeprecatedPaintLayer& rootLayer = *layoutView().layer();
    DeprecatedPaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    LayoutObject& container1 = *document().getElementById("container1")->layoutObject();
    DeprecatedPaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
    LayoutObject& container2 = *document().getElementById("container2")->layoutObject();
    DeprecatedPaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
    LayoutObject& container3 = *document().getElementById("container3")->layoutObject();
    DeprecatedPaintLayer& container3Layer = *toLayoutBoxModelObject(container3).layer();

    document().view()->updateAllLifecyclePhases();
    GraphicsContext context(&rootDisplayItemList());
    DeprecatedPaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 400, 300), GlobalPaintNormalPhase, LayoutSize());
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootDisplayItemList().commitNewDisplayItems();

    // Container1 is fully in the interest rect;
    // Container2 is partly (including its stacking chidren) in the interest rect;
    // Content2 is out of the interest rect and output nothing;
    // Container3 is fully in the interest rect.
    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 14,
        TestDisplayItem(rootLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container3Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container3, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container3Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
        TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));

    // Container1 becomes partly in the interest rect, but uses cached subsequence
    // because it was fully painted before;
    // Container2's intersection with the interest rect changes;
    // Content2 is out of the interest rect and outputs nothing;
    // Container3 becomes out of the interest rect and outputs nothing.
    DeprecatedPaintLayerPaintingInfo paintingInfo1(&rootLayer, LayoutRect(0, 100, 300, 300), GlobalPaintNormalPhase, LayoutSize());
    DeprecatedPaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo1, PaintLayerPaintingCompositingAllPhases);

    EXPECT_DISPLAY_LIST(rootDisplayItemList().newDisplayItems(), 9,
        TestDisplayItem(rootLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(layoutView(), DisplayItem::drawingTypeToCachedDrawingType(DisplayItem::BoxDecorationBackground)),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::CachedSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container2, DisplayItem::drawingTypeToCachedDrawingType(DisplayItem::BoxDecorationBackground)),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
        TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));

    rootDisplayItemList().commitNewDisplayItems();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 11,
        TestDisplayItem(rootLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
        TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
}

TEST_F(DeprecatedPaintLayerPainterTestForSlimmingPaintV2, CachedSubsequence)
{
    setBodyInnerHTML(
        "<div id='container1' style='position: relative; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content1' style='width: 100px; height: 100px; background-color: red'></div>"
        "</div>"
        "<div id='container2' style='position: relative; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content2' style='width: 100px; height: 100px; background-color: green'></div>"
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
        TestDisplayItem(rootLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
        TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));

    toHTMLElement(content1.node())->setAttribute(HTMLNames::styleAttr, "width: 100px; height: 100px; background-color: green");
    updateLifecyclePhasesToPaintClean(LayoutRect::infiniteRect());

    EXPECT_DISPLAY_LIST(rootDisplayItemList().newDisplayItems(), 10,
        TestDisplayItem(rootLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(layoutView(), DisplayItem::drawingTypeToCachedDrawingType(DisplayItem::BoxDecorationBackground)),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1, DisplayItem::drawingTypeToCachedDrawingType(DisplayItem::BoxDecorationBackground)),
        TestDisplayItem(content1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::CachedSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
        TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));

    compositeForSlimmingPaintV2();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 13,
        TestDisplayItem(rootLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
        TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));

    // Repeated painting should just generate the root cached subsequence.
    updateLifecyclePhasesToPaintClean();
    EXPECT_DISPLAY_LIST(rootDisplayItemList().newDisplayItems(), 1,
        TestDisplayItem(rootLayer, DisplayItem::CachedSubsequence));

    compositeForSlimmingPaintV2();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 13,
        TestDisplayItem(rootLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(content2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
        TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
}

TEST_F(DeprecatedPaintLayerPainterTestForSlimmingPaintV2, CachedSubsequenceOnInterestRectChange)
{
    setBodyInnerHTML(
        "<div id='container1' style='position: relative; width: 200px; height: 200px; background-color: blue'></div>"
        "<div id='container2' style='position: absolute; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content2' style='position: relative; top: 200px; width: 100px; height: 100px; background-color: green'></div>"
        "</div>"
        "<div id='container3' style='position: absolute; z-index: 2; left: 300px; top: 0; width: 200px; height: 200px; background-color: blue'></div>");
    rootDisplayItemList().invalidateAll();

    DeprecatedPaintLayer& rootLayer = *layoutView().layer();
    DeprecatedPaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    LayoutObject& container1 = *document().getElementById("container1")->layoutObject();
    DeprecatedPaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
    LayoutObject& container2 = *document().getElementById("container2")->layoutObject();
    DeprecatedPaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
    LayoutObject& container3 = *document().getElementById("container3")->layoutObject();
    DeprecatedPaintLayer& container3Layer = *toLayoutBoxModelObject(container3).layer();

    document().view()->updateAllLifecyclePhases(LayoutRect(0, 0, 400, 300));

    // Container1 is fully in the interest rect;
    // Container2 is partly (including its stacking chidren) in the interest rect;
    // Content2 is out of the interest rect and output nothing;
    // Container3 is fully in the interest rect.
    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 14,
        TestDisplayItem(rootLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container3Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container3, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container3Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
        TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));

    // Container1 becomes partly in the interest rect, but uses cached subsequence
    // because it was fully painted before;
    // Container2's intersection with the interest rect changes;
    // Content2 is out of the interest rect and outputs nothing;
    // Container3 becomes out of the interest rect and outputs nothing.
    updateLifecyclePhasesToPaintClean(LayoutRect(0, 100, 300, 300));

    EXPECT_DISPLAY_LIST(rootDisplayItemList().newDisplayItems(), 9,
        TestDisplayItem(rootLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(layoutView(), DisplayItem::drawingTypeToCachedDrawingType(DisplayItem::BoxDecorationBackground)),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::CachedSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container2, DisplayItem::drawingTypeToCachedDrawingType(DisplayItem::BoxDecorationBackground)),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
        TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));

    compositeForSlimmingPaintV2();

    EXPECT_DISPLAY_LIST(rootDisplayItemList().displayItems(), 11,
        TestDisplayItem(rootLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(layoutView(), DisplayItem::BoxDecorationBackground),
        TestDisplayItem(htmlLayer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::BeginSubsequence),
        TestDisplayItem(container2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
        TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
}

} // namespace blink
