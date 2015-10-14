// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/paint/PaintControllerPaintTest.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

class PaintLayerPainterTest
    : public PaintControllerPaintTest
    , public testing::WithParamInterface<FrameSettingOverrideFunction> {
    WTF_MAKE_FAST_ALLOCATED(PaintLayerPainterTest);
public:
    FrameSettingOverrideFunction settingOverrider() const override { return GetParam(); }
};

using PaintLayerPainterTestForSlimmingPaintV2 = PaintControllerPaintTestForSlimmingPaintV2;

INSTANTIATE_TEST_CASE_P(All, PaintLayerPainterTest, ::testing::Values(
    nullptr,
    RootLayerScrollsFrameSettingOverride));

TEST_P(PaintLayerPainterTest, CachedSubsequence)
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

    bool rootLayerScrolls = document().frame()->settings()->rootLayerScrolls();
    PaintLayer& rootLayer = *layoutView().layer();
    PaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    LayoutObject& container1 = *document().getElementById("container1")->layoutObject();
    PaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
    LayoutObject& content1 = *document().getElementById("content1")->layoutObject();
    LayoutObject& container2 = *document().getElementById("container2")->layoutObject();
    PaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
    LayoutObject& content2 = *document().getElementById("content2")->layoutObject();

    GraphicsContext context(&rootPaintController());
    PaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 800, 600), GlobalPaintNormalPhase, LayoutSize());
    PaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootPaintController().commitNewDisplayItems();

    if (rootLayerScrolls) {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 11,
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(rootLayer, subsequenceType),
            TestDisplayItem(container1, backgroundType),
            TestDisplayItem(container1Layer, subsequenceType),
            TestDisplayItem(content1, backgroundType),
            TestDisplayItem(container1Layer, endSubsequenceType),
            TestDisplayItem(container2, backgroundType),
            TestDisplayItem(container2Layer, subsequenceType),
            TestDisplayItem(content2, backgroundType),
            TestDisplayItem(container2Layer, endSubsequenceType),
            TestDisplayItem(rootLayer, endSubsequenceType));
    } else {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 13,
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

    toHTMLElement(content1.node())->setAttribute(HTMLNames::styleAttr, "position: absolute; width: 100px; height: 100px; background-color: green");
    document().view()->updateAllLifecyclePhases();
    PaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);

    if (rootLayerScrolls) {
        EXPECT_DISPLAY_LIST(rootPaintController().newDisplayItemList(), 9,
            TestDisplayItem(layoutView(), cachedBackgroundType),
            TestDisplayItem(rootLayer, subsequenceType),
            TestDisplayItem(container1, cachedBackgroundType),
            TestDisplayItem(container1Layer, subsequenceType),
            TestDisplayItem(content1, backgroundType),
            TestDisplayItem(container1Layer, endSubsequenceType),
            TestDisplayItem(container2, cachedBackgroundType),
            TestDisplayItem(container2Layer, cachedSubsequenceType),
            TestDisplayItem(rootLayer, endSubsequenceType));
    } else {
        EXPECT_DISPLAY_LIST(rootPaintController().newDisplayItemList(), 11,
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
    }

    rootPaintController().commitNewDisplayItems();

    if (rootLayerScrolls) {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 11,
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(rootLayer, subsequenceType),
            TestDisplayItem(container1, backgroundType),
            TestDisplayItem(container1Layer, subsequenceType),
            TestDisplayItem(content1, backgroundType),
            TestDisplayItem(container1Layer, endSubsequenceType),
            TestDisplayItem(container2, backgroundType),
            TestDisplayItem(container2Layer, subsequenceType),
            TestDisplayItem(content2, backgroundType),
            TestDisplayItem(container2Layer, endSubsequenceType),
            TestDisplayItem(rootLayer, endSubsequenceType));
    } else {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 13,
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

    // Repeated painting should just generate the root cached subsequence.
    PaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    EXPECT_DISPLAY_LIST(rootPaintController().newDisplayItemList(), 2,
        TestDisplayItem(layoutView(), cachedBackgroundType),
        TestDisplayItem(rootLayer, cachedSubsequenceType));

    rootPaintController().commitNewDisplayItems();

    if (rootLayerScrolls) {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 11,
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(rootLayer, subsequenceType),
            TestDisplayItem(container1, backgroundType),
            TestDisplayItem(container1Layer, subsequenceType),
            TestDisplayItem(content1, backgroundType),
            TestDisplayItem(container1Layer, endSubsequenceType),
            TestDisplayItem(container2, backgroundType),
            TestDisplayItem(container2Layer, subsequenceType),
            TestDisplayItem(content2, backgroundType),
            TestDisplayItem(container2Layer, endSubsequenceType),
            TestDisplayItem(rootLayer, endSubsequenceType));
    } else {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 13,
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
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceOnInterestRectChange)
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
    rootPaintController().invalidateAll();

    bool rootLayerScrolls = document().frame()->settings()->rootLayerScrolls();
    PaintLayer& rootLayer = *layoutView().layer();
    PaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    LayoutObject& container1 = *document().getElementById("container1")->layoutObject();
    PaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
    LayoutObject& content1 = *document().getElementById("content1")->layoutObject();
    LayoutObject& container2 = *document().getElementById("container2")->layoutObject();
    PaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
    LayoutObject& content2a = *document().getElementById("content2a")->layoutObject();
    LayoutObject& container3 = *document().getElementById("container3")->layoutObject();
    PaintLayer& container3Layer = *toLayoutBoxModelObject(container3).layer();
    LayoutObject& content3 = *document().getElementById("content3")->layoutObject();

    document().view()->updateAllLifecyclePhases();
    GraphicsContext context(&rootPaintController());
    PaintLayerPaintingInfo paintingInfo(&rootLayer, LayoutRect(0, 0, 400, 300), GlobalPaintNormalPhase, LayoutSize());
    PaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo, PaintLayerPaintingCompositingAllPhases);
    rootPaintController().commitNewDisplayItems();

    // Container1 is fully in the interest rect;
    // Container2 is partly (including its stacking chidren) in the interest rect;
    // Content2b is out of the interest rect and output nothing;
    // Container3 is partly in the interest rect.
    if (rootLayerScrolls) {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 15,
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(rootLayer, subsequenceType),
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
            TestDisplayItem(rootLayer, endSubsequenceType));
    } else {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 17,
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
    }

    // Container1 becomes partly in the interest rect, but uses cached subsequence
    // because it was fully painted before;
    // Container2's intersection with the interest rect changes;
    // Content2b is out of the interest rect and outputs nothing;
    // Container3 becomes out of the interest rect and outputs nothing.
    PaintLayerPaintingInfo paintingInfo1(&rootLayer, LayoutRect(0, 100, 300, 300), GlobalPaintNormalPhase, LayoutSize());
    PaintLayerPainter(rootLayer).paintLayerContents(&context, paintingInfo1, PaintLayerPaintingCompositingAllPhases);

    if (rootLayerScrolls) {
        EXPECT_DISPLAY_LIST(rootPaintController().newDisplayItemList(), 9,
            TestDisplayItem(layoutView(), cachedBackgroundType),
            TestDisplayItem(rootLayer, subsequenceType),
            TestDisplayItem(container1, cachedBackgroundType),
            TestDisplayItem(container1Layer, cachedSubsequenceType),
            TestDisplayItem(container2, cachedBackgroundType),
            TestDisplayItem(container2Layer, subsequenceType),
            TestDisplayItem(content2a, cachedBackgroundType),
            TestDisplayItem(container2Layer, endSubsequenceType),
            TestDisplayItem(rootLayer, endSubsequenceType));

    } else {
        EXPECT_DISPLAY_LIST(rootPaintController().newDisplayItemList(), 11,
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
    }

    rootPaintController().commitNewDisplayItems();

    if (rootLayerScrolls) {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 11,
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(rootLayer, subsequenceType),
            TestDisplayItem(container1, backgroundType),
            TestDisplayItem(container1Layer, subsequenceType),
            TestDisplayItem(content1, backgroundType),
            TestDisplayItem(container1Layer, endSubsequenceType),
            TestDisplayItem(container2, backgroundType),
            TestDisplayItem(container2Layer, subsequenceType),
            TestDisplayItem(content2a, backgroundType),
            TestDisplayItem(container2Layer, endSubsequenceType),
            TestDisplayItem(rootLayer, endSubsequenceType));
    } else {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 13,
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
}

TEST_F(PaintLayerPainterTestForSlimmingPaintV2, CachedSubsequence)
{
    setBodyInnerHTML(
        "<div id='container1' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content1' style='position: absolute; width: 100px; height: 100px; background-color: red'></div>"
        "</div>"
        "<div id='container2' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content2' style='position: absolute; width: 100px; height: 100px; background-color: green'></div>"
        "</div>");
    document().view()->updateAllLifecyclePhases();

    PaintLayer& rootLayer = *layoutView().layer();
    PaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    LayoutObject& container1 = *document().getElementById("container1")->layoutObject();
    PaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
    LayoutObject& content1 = *document().getElementById("content1")->layoutObject();
    LayoutObject& container2 = *document().getElementById("container2")->layoutObject();
    PaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
    LayoutObject& content2 = *document().getElementById("content2")->layoutObject();

    EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 13,
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
    updateLifecyclePhasesToPaintClean();

    EXPECT_DISPLAY_LIST(rootPaintController().newDisplayItemList(), 11,
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

    EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 13,
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

    // Repeated painting should just generate a CachedDisplayItemList item.
    updateLifecyclePhasesToPaintClean();
    EXPECT_DISPLAY_LIST(rootPaintController().newDisplayItemList(), 1,
        TestDisplayItem(*rootLayer.compositedLayerMapping(), DisplayItem::CachedDisplayItemList));

    compositeForSlimmingPaintV2();

    EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 13,
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

TEST_F(PaintLayerPainterTestForSlimmingPaintV2, CachedSubsequenceOnInterestRectChange)
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
    rootPaintController().invalidateAll();

    PaintLayer& rootLayer = *layoutView().layer();
    PaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    LayoutObject& container1 = *document().getElementById("container1")->layoutObject();
    PaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
    LayoutObject& content1 = *document().getElementById("content1")->layoutObject();
    LayoutObject& container2 = *document().getElementById("container2")->layoutObject();
    PaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
    LayoutObject& content2a = *document().getElementById("content2a")->layoutObject();
    LayoutObject& container3 = *document().getElementById("container3")->layoutObject();
    PaintLayer& container3Layer = *toLayoutBoxModelObject(container3).layer();
    LayoutObject& content3 = *document().getElementById("content3")->layoutObject();

    LayoutRect interestRect(0, 0, 400, 300);
    document().view()->updateAllLifecyclePhases(&interestRect);

    // Container1 is fully in the interest rect;
    // Container2 is partly (including its stacking chidren) in the interest rect;
    // Content2b is out of the interest rect and output nothing;
    // Container3 is partly in the interest rect.
    EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 17,
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
    LayoutRect newInterestRect(0, 100, 300, 300);
    updateLifecyclePhasesToPaintClean(&newInterestRect);

    EXPECT_DISPLAY_LIST(rootPaintController().newDisplayItemList(), 11,
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

    EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 13,
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
