// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/compositing/CompositingReasonFinder.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"

namespace blink {

class CompositingReasonFinderTest : public RenderingTest {
 public:
  CompositingReasonFinderTest()
      : RenderingTest(EmptyLocalFrameClient::create()) {}

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    enableCompositing();
  }
};

TEST_F(CompositingReasonFinderTest, PromoteOpaqueFixedPosition) {
  ScopedCompositeFixedPositionForTest compositeFixedPosition(true);

  setBodyInnerHTML(
      "<div id='translucent' style='width: 20px; height: 20px; position: "
      "fixed; top: 100px; left: 100px;'></div>"
      "<div id='opaque' style='width: 20px; height: 20px; position: fixed; "
      "top: 100px; left: 200px; background: white;'></div>"
      "<div id='opaque-with-shadow' style='width: 20px; height: 20px; "
      "position: fixed; top: 100px; left: 300px; background: white; "
      "box-shadow: 10px 10px 5px #888888;'></div>"
      "<div id='spacer' style='height: 2000px'></div>");

  document().view()->updateAllLifecyclePhases();

  // The translucent fixed box should not be promoted.
  Element* element = document().getElementById("translucent");
  PaintLayer* paintLayer =
      toLayoutBoxModelObject(element->layoutObject())->layer();
  EXPECT_EQ(NotComposited, paintLayer->compositingState());

  // The opaque fixed box should be promoted and be opaque so that text will be
  // drawn with subpixel anti-aliasing.
  element = document().getElementById("opaque");
  paintLayer = toLayoutBoxModelObject(element->layoutObject())->layer();
  EXPECT_EQ(PaintsIntoOwnBacking, paintLayer->compositingState());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());

  // The opaque fixed box with shadow should not be promoted because the layer
  // will include the shadow which is not opaque.
  element = document().getElementById("opaque-with-shadow");
  paintLayer = toLayoutBoxModelObject(element->layoutObject())->layer();
  EXPECT_EQ(NotComposited, paintLayer->compositingState());
}

// Tests that a transform on the fixed or an ancestor will prevent promotion
// TODO(flackr): Allow integer transforms as long as all of the ancestor
// transforms are also integer.
TEST_F(CompositingReasonFinderTest, OnlyNonTransformedFixedLayersPromoted) {
  ScopedCompositeFixedPositionForTest compositeFixedPosition(true);

  setBodyInnerHTML(
      "<style>"
      "#fixed { position: fixed; height: 200px; width: 200px; background: "
      "white; top: 0; }"
      "#spacer { height: 3000px; }"
      "</style>"
      "<div id=\"parent\">"
      "  <div id=\"fixed\"></div>"
      "  <div id=\"spacer\"></div>"
      "</div>");
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
  Element* parent = document().getElementById("parent");
  Element* fixed = document().getElementById("fixed");
  PaintLayer* paintLayer =
      toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(PaintsIntoOwnBacking, paintLayer->compositingState());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());

  // Change the parent to have a transform.
  parent->setAttribute(HTMLNames::styleAttr, "transform: translate(1px, 0);");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(NotComposited, paintLayer->compositingState());

  // Change the parent to have no transform again.
  parent->removeAttribute(HTMLNames::styleAttr);
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(PaintsIntoOwnBacking, paintLayer->compositingState());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());

  // Apply a transform to the fixed directly.
  fixed->setAttribute(HTMLNames::styleAttr, "transform: translate(1px, 0);");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(NotComposited, paintLayer->compositingState());
}

// Test that opacity applied to the fixed or an ancestor will cause the
// scrolling contents layer to not be promoted.
TEST_F(CompositingReasonFinderTest, OnlyOpaqueFixedLayersPromoted) {
  ScopedCompositeFixedPositionForTest compositeFixedPosition(true);

  setBodyInnerHTML(
      "<style>"
      "#fixed { position: fixed; height: 200px; width: 200px; background: "
      "white; top: 0}"
      "#spacer { height: 3000px; }"
      "</style>"
      "<div id=\"parent\">"
      "  <div id=\"fixed\"></div>"
      "  <div id=\"spacer\"></div>"
      "</div>");
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
  Element* parent = document().getElementById("parent");
  Element* fixed = document().getElementById("fixed");
  PaintLayer* paintLayer =
      toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(PaintsIntoOwnBacking, paintLayer->compositingState());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());

  // Change the parent to be partially translucent.
  parent->setAttribute(HTMLNames::styleAttr, "opacity: 0.5;");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(NotComposited, paintLayer->compositingState());

  // Change the parent to be opaque again.
  parent->setAttribute(HTMLNames::styleAttr, "opacity: 1;");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(PaintsIntoOwnBacking, paintLayer->compositingState());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());

  // Make the fixed translucent.
  fixed->setAttribute(HTMLNames::styleAttr, "opacity: 0.5");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(NotComposited, paintLayer->compositingState());
}

TEST_F(CompositingReasonFinderTest, RequiresCompositingForTransformAnimation) {
  RefPtr<ComputedStyle> style = ComputedStyle::create();
  style->setSubtreeWillChangeContents(false);

  style->setHasCurrentTransformAnimation(false);
  style->setIsRunningTransformAnimationOnCompositor(false);
  EXPECT_FALSE(
      CompositingReasonFinder::requiresCompositingForTransformAnimation(
          *style));

  style->setHasCurrentTransformAnimation(false);
  style->setIsRunningTransformAnimationOnCompositor(true);
  EXPECT_FALSE(
      CompositingReasonFinder::requiresCompositingForTransformAnimation(
          *style));

  style->setHasCurrentTransformAnimation(true);
  style->setIsRunningTransformAnimationOnCompositor(false);
  EXPECT_TRUE(CompositingReasonFinder::requiresCompositingForTransformAnimation(
      *style));

  style->setHasCurrentTransformAnimation(true);
  style->setIsRunningTransformAnimationOnCompositor(true);
  EXPECT_TRUE(CompositingReasonFinder::requiresCompositingForTransformAnimation(
      *style));

  style->setSubtreeWillChangeContents(true);

  style->setHasCurrentTransformAnimation(false);
  style->setIsRunningTransformAnimationOnCompositor(false);
  EXPECT_FALSE(
      CompositingReasonFinder::requiresCompositingForTransformAnimation(
          *style));

  style->setHasCurrentTransformAnimation(false);
  style->setIsRunningTransformAnimationOnCompositor(true);
  EXPECT_TRUE(CompositingReasonFinder::requiresCompositingForTransformAnimation(
      *style));

  style->setHasCurrentTransformAnimation(true);
  style->setIsRunningTransformAnimationOnCompositor(false);
  EXPECT_FALSE(
      CompositingReasonFinder::requiresCompositingForTransformAnimation(
          *style));

  style->setHasCurrentTransformAnimation(true);
  style->setIsRunningTransformAnimationOnCompositor(true);
  EXPECT_TRUE(CompositingReasonFinder::requiresCompositingForTransformAnimation(
      *style));
}

TEST_F(CompositingReasonFinderTest, RequiresCompositingForEffectAnimation) {
  RefPtr<ComputedStyle> style = ComputedStyle::create();

  style->setSubtreeWillChangeContents(false);

  // In the interest of brevity, for each side of subtreeWillChangeContents()
  // code path we only check that any one of the effect related animation flags
  // being set produces true, rather than every permutation.

  style->setHasCurrentOpacityAnimation(false);
  style->setHasCurrentFilterAnimation(false);
  style->setHasCurrentBackdropFilterAnimation(false);
  EXPECT_FALSE(
      CompositingReasonFinder::requiresCompositingForEffectAnimation(*style));

  style->setHasCurrentOpacityAnimation(true);
  style->setHasCurrentFilterAnimation(false);
  style->setHasCurrentBackdropFilterAnimation(false);
  EXPECT_TRUE(
      CompositingReasonFinder::requiresCompositingForEffectAnimation(*style));

  style->setHasCurrentOpacityAnimation(false);
  style->setHasCurrentFilterAnimation(true);
  style->setHasCurrentBackdropFilterAnimation(false);
  EXPECT_TRUE(
      CompositingReasonFinder::requiresCompositingForEffectAnimation(*style));

  style->setHasCurrentOpacityAnimation(false);
  style->setHasCurrentFilterAnimation(false);
  style->setHasCurrentBackdropFilterAnimation(true);
  EXPECT_TRUE(
      CompositingReasonFinder::requiresCompositingForEffectAnimation(*style));

  // Check the other side of subtreeWillChangeContents.
  style->setSubtreeWillChangeContents(true);
  style->setHasCurrentOpacityAnimation(false);
  style->setHasCurrentFilterAnimation(false);
  style->setHasCurrentBackdropFilterAnimation(false);
  EXPECT_FALSE(
      CompositingReasonFinder::requiresCompositingForEffectAnimation(*style));

  style->setIsRunningOpacityAnimationOnCompositor(true);
  style->setIsRunningFilterAnimationOnCompositor(false);
  style->setIsRunningBackdropFilterAnimationOnCompositor(false);
  EXPECT_TRUE(
      CompositingReasonFinder::requiresCompositingForEffectAnimation(*style));

  style->setIsRunningOpacityAnimationOnCompositor(false);
  style->setIsRunningFilterAnimationOnCompositor(true);
  style->setIsRunningBackdropFilterAnimationOnCompositor(false);
  EXPECT_TRUE(
      CompositingReasonFinder::requiresCompositingForEffectAnimation(*style));

  style->setIsRunningOpacityAnimationOnCompositor(false);
  style->setIsRunningFilterAnimationOnCompositor(false);
  style->setIsRunningBackdropFilterAnimationOnCompositor(true);
  EXPECT_TRUE(
      CompositingReasonFinder::requiresCompositingForEffectAnimation(*style));
}

TEST_F(CompositingReasonFinderTest, DoNotCompositeNestedSticky) {
  ScopedCompositeFixedPositionForTest compositeFixedPosition(true);

  setBodyInnerHTML(
      "<style>.scroller { overflow: scroll; height: 200px; width: 100px; }"
      ".container { height: 500px; }"
      ".opaque { background-color: white; contain: paint; }"
      "#outerSticky { height: 50px; position: sticky; top: 0px; }"
      "#innerSticky { height: 20px; position: sticky; top: 25px; }</style>"
      "<div class='scroller'>"
      "  <div class='container'>"
      "    <div id='outerSticky' class='opaque'>"
      "      <div id='innerSticky' class='opaque'></div>"
      "    </div>"
      "  </div>"
      "</div>");
  document().view()->updateAllLifecyclePhases();

  Element* outerSticky = document().getElementById("outerSticky");
  PaintLayer* outerStickyLayer =
      toLayoutBoxModelObject(outerSticky->layoutObject())->layer();
  ASSERT_TRUE(outerStickyLayer);

  Element* innerSticky = document().getElementById("innerSticky");
  PaintLayer* innerStickyLayer =
      toLayoutBoxModelObject(innerSticky->layoutObject())->layer();
  ASSERT_TRUE(innerStickyLayer);

  EXPECT_EQ(PaintsIntoOwnBacking, outerStickyLayer->compositingState());
  EXPECT_EQ(NotComposited, innerStickyLayer->compositingState());
}

}  // namespace blink
