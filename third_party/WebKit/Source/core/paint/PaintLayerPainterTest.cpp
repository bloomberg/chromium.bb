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
    USING_FAST_MALLOC(PaintLayerPainterTest);
public:
    FrameSettingOverrideFunction settingOverrider() const override { return GetParam(); }
};

INSTANTIATE_TEST_CASE_P(All, PaintLayerPainterTest, ::testing::Values(
    nullptr,
    RootLayerScrollsFrameSettingOverride));

TEST_P(PaintLayerPainterTest, CachedSubsequence)
{
    RuntimeEnabledFeatures::setSlimmingPaintSynchronizedPaintingEnabled(true);

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

    if (rootLayerScrolls) {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 7,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(container1, backgroundType),
            TestDisplayItem(content1, backgroundType),
            TestDisplayItem(container2, backgroundType),
            TestDisplayItem(content2, backgroundType),
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    } else {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 13,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
            TestDisplayItem(container1Layer, DisplayItem::Subsequence),
            TestDisplayItem(container1, backgroundType),
            TestDisplayItem(content1, backgroundType),
            TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
            TestDisplayItem(container2Layer, DisplayItem::Subsequence),
            TestDisplayItem(container2, backgroundType),
            TestDisplayItem(content2, backgroundType),
            TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
            TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    }

    toHTMLElement(content1.node())->setAttribute(HTMLNames::styleAttr, "position: absolute; width: 100px; height: 100px; background-color: green");
    updateLifecyclePhasesBeforePaint();
    paint();

    if (rootLayerScrolls) {
        EXPECT_DISPLAY_LIST(rootPaintController().newDisplayItemList(), 7,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), cachedBackgroundType),
            TestDisplayItem(container1, cachedBackgroundType),
            TestDisplayItem(content1, backgroundType),
            TestDisplayItem(container2, cachedBackgroundType),
            TestDisplayItem(content2, cachedBackgroundType),
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    } else {
        EXPECT_DISPLAY_LIST(rootPaintController().newDisplayItemList(), 10,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), cachedBackgroundType),
            TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
            TestDisplayItem(container1Layer, DisplayItem::Subsequence),
            TestDisplayItem(container1, cachedBackgroundType),
            TestDisplayItem(content1, backgroundType),
            TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
            TestDisplayItem(container2Layer, DisplayItem::CachedSubsequence),
            TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    }

    commit();

    if (rootLayerScrolls) {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 7,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(container1, backgroundType),
            TestDisplayItem(content1, backgroundType),
            TestDisplayItem(container2, backgroundType),
            TestDisplayItem(content2, backgroundType),
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    } else {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 13,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
            TestDisplayItem(container1Layer, DisplayItem::Subsequence),
            TestDisplayItem(container1, backgroundType),
            TestDisplayItem(content1, backgroundType),
            TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
            TestDisplayItem(container2Layer, DisplayItem::Subsequence),
            TestDisplayItem(container2, backgroundType),
            TestDisplayItem(content2, backgroundType),
            TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
            TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    }
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceOnInterestRectChange)
{
    RuntimeEnabledFeatures::setSlimmingPaintSynchronizedPaintingEnabled(true);

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
    LayoutObject& content2b = *document().getElementById("content2b")->layoutObject();
    LayoutObject& container3 = *document().getElementById("container3")->layoutObject();
    PaintLayer& container3Layer = *toLayoutBoxModelObject(container3).layer();
    LayoutObject& content3 = *document().getElementById("content3")->layoutObject();

    updateLifecyclePhasesBeforePaint();
    IntRect interestRect(0, 0, 400, 300);
    paint(&interestRect);
    commit();

    // Container1 is fully in the interest rect;
    // Container2 is partly (including its stacking chidren) in the interest rect;
    // Content2b is out of the interest rect and output nothing;
    // Container3 is partly in the interest rect.
    if (rootLayerScrolls) {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 9,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(container1, backgroundType),
            TestDisplayItem(content1, backgroundType),
            TestDisplayItem(container2, backgroundType),
            TestDisplayItem(content2a, backgroundType),
            TestDisplayItem(container3, backgroundType),
            TestDisplayItem(content3, backgroundType),
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    } else {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 17,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
            TestDisplayItem(container1Layer, DisplayItem::Subsequence),
            TestDisplayItem(container1, backgroundType),
            TestDisplayItem(content1, backgroundType),
            TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
            TestDisplayItem(container2Layer, DisplayItem::Subsequence),
            TestDisplayItem(container2, backgroundType),
            TestDisplayItem(content2a, backgroundType),
            TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
            TestDisplayItem(container3Layer, DisplayItem::Subsequence),
            TestDisplayItem(container3, backgroundType),
            TestDisplayItem(content3, backgroundType),
            TestDisplayItem(container3Layer, DisplayItem::EndSubsequence),
            TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    }

    updateLifecyclePhasesBeforePaint();
    IntRect newInterestRect(0, 100, 300, 1000);
    paint(&newInterestRect);

    // Container1 becomes partly in the interest rect, but uses cached subsequence
    // because it was fully painted before;
    // Container2's intersection with the interest rect changes;
    // Content2b is out of the interest rect and outputs nothing;
    // Container3 becomes out of the interest rect and outputs nothing.

    if (rootLayerScrolls) {
        EXPECT_DISPLAY_LIST(rootPaintController().newDisplayItemList(), 8,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), cachedBackgroundType),
            TestDisplayItem(container1, cachedBackgroundType),
            TestDisplayItem(content1, cachedBackgroundType),
            TestDisplayItem(container2, cachedBackgroundType),
            TestDisplayItem(content2a, cachedBackgroundType),
            TestDisplayItem(content2b, backgroundType),
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    } else {
        EXPECT_DISPLAY_LIST(rootPaintController().newDisplayItemList(), 11,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), cachedBackgroundType),
            TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
            TestDisplayItem(container1Layer, DisplayItem::CachedSubsequence),
            TestDisplayItem(container2Layer, DisplayItem::Subsequence),
            TestDisplayItem(container2, cachedBackgroundType),
            TestDisplayItem(content2a, cachedBackgroundType),
            TestDisplayItem(content2b, backgroundType),
            TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
            TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    }

    commit();

    if (rootLayerScrolls) {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 8,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(container1, backgroundType),
            TestDisplayItem(content1, backgroundType),
            TestDisplayItem(container2, backgroundType),
            TestDisplayItem(content2a, backgroundType),
            TestDisplayItem(content2b, backgroundType),
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    } else {
        EXPECT_DISPLAY_LIST(rootPaintController().displayItemList(), 14,
            TestDisplayItem(rootLayer, DisplayItem::Subsequence),
            TestDisplayItem(layoutView(), backgroundType),
            TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
            TestDisplayItem(container1Layer, DisplayItem::Subsequence),
            TestDisplayItem(container1, backgroundType),
            TestDisplayItem(content1, backgroundType),
            TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
            TestDisplayItem(container2Layer, DisplayItem::Subsequence),
            TestDisplayItem(container2, backgroundType),
            TestDisplayItem(content2a, backgroundType),
            TestDisplayItem(content2b, backgroundType),
            TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
            TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence),
            TestDisplayItem(rootLayer, DisplayItem::EndSubsequence));
    }
}

} // namespace blink
