// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/compositing/CompositingReasonFinder.h"

#include "core/frame/LocalFrameView.h"
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
      : RenderingTest(EmptyLocalFrameClient::Create()) {}

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    EnableCompositing();
  }
};

TEST_F(CompositingReasonFinderTest, PromoteOpaqueFixedPosition) {
  ScopedCompositeFixedPositionForTest composite_fixed_position(true);

  SetBodyInnerHTML(
      "<div id='translucent' style='width: 20px; height: 20px; position: "
      "fixed; top: 100px; left: 100px;'></div>"
      "<div id='opaque' style='width: 20px; height: 20px; position: fixed; "
      "top: 100px; left: 200px; background: white;'></div>"
      "<div id='opaque-with-shadow' style='width: 20px; height: 20px; "
      "position: fixed; top: 100px; left: 300px; background: white; "
      "box-shadow: 10px 10px 5px #888888;'></div>"
      "<div id='spacer' style='height: 2000px'></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  // The translucent fixed box should not be promoted.
  Element* element = GetDocument().getElementById("translucent");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  EXPECT_EQ(kNotComposited, paint_layer->GetCompositingState());

  // The opaque fixed box should be promoted and be opaque so that text will be
  // drawn with subpixel anti-aliasing.
  element = GetDocument().getElementById("opaque");
  paint_layer = ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  EXPECT_EQ(kPaintsIntoOwnBacking, paint_layer->GetCompositingState());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());

  // The opaque fixed box with shadow should not be promoted because the layer
  // will include the shadow which is not opaque.
  element = GetDocument().getElementById("opaque-with-shadow");
  paint_layer = ToLayoutBoxModelObject(element->GetLayoutObject())->Layer();
  EXPECT_EQ(kNotComposited, paint_layer->GetCompositingState());
}

TEST_F(CompositingReasonFinderTest, OnlyAnchoredStickyPositionPromoted) {
  SetBodyInnerHTML(
      "<style>"
      ".scroller {contain: paint; width: 400px; height: 400px; overflow: auto; "
      "will-change: transform;}"
      ".sticky { position: sticky; width: 10px; height: 10px;}</style>"
      "<div class='scroller'>"
      "  <div id='sticky-top' class='sticky' style='top: 0px;'></div>"
      "  <div id='sticky-no-anchor' class='sticky'></div>"
      "  <div style='height: 2000px;'></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(kPaintsIntoOwnBacking,
            ToLayoutBoxModelObject(GetLayoutObjectByElementId("sticky-top"))
                ->Layer()
                ->GetCompositingState());
  EXPECT_EQ(kNotComposited, ToLayoutBoxModelObject(
                                GetLayoutObjectByElementId("sticky-no-anchor"))
                                ->Layer()
                                ->GetCompositingState());
}

TEST_F(CompositingReasonFinderTest, OnlyScrollingStickyPositionPromoted) {
  SetBodyInnerHTML(
      "<style>.scroller {width: 400px; height: 400px; overflow: auto; "
      "will-change: transform;}"
      ".sticky { position: sticky; top: 0; width: 10px; height: 10px;}"
      "</style>"
      "<div class='scroller'>"
      "  <div id='sticky-scrolling' class='sticky'></div>"
      "  <div style='height: 2000px;'></div>"
      "</div>"
      "<div class='scroller'>"
      "  <div id='sticky-no-scrolling' class='sticky'></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(
      kPaintsIntoOwnBacking,
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("sticky-scrolling"))
          ->Layer()
          ->GetCompositingState());
  EXPECT_EQ(
      kNotComposited,
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("sticky-no-scrolling"))
          ->Layer()
          ->GetCompositingState());
}

// Tests that a transform on the fixed or an ancestor will prevent promotion
// TODO(flackr): Allow integer transforms as long as all of the ancestor
// transforms are also integer.
TEST_F(CompositingReasonFinderTest, OnlyNonTransformedFixedLayersPromoted) {
  ScopedCompositeFixedPositionForTest composite_fixed_position(true);

  SetBodyInnerHTML(
      "<style>"
      "#fixed { position: fixed; height: 200px; width: 200px; background: "
      "white; top: 0; }"
      "#spacer { height: 3000px; }"
      "</style>"
      "<div id=\"parent\">"
      "  <div id=\"fixed\"></div>"
      "  <div id=\"spacer\"></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::CompositeOpaqueScrollersEnabled());
  Element* parent = GetDocument().getElementById("parent");
  Element* fixed = GetDocument().getElementById("fixed");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(fixed->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_EQ(kPaintsIntoOwnBacking, paint_layer->GetCompositingState());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());

  // Change the parent to have a transform.
  parent->setAttribute(HTMLNames::styleAttr, "transform: translate(1px, 0);");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(fixed->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_EQ(kNotComposited, paint_layer->GetCompositingState());

  // Change the parent to have no transform again.
  parent->removeAttribute(HTMLNames::styleAttr);
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(fixed->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_EQ(kPaintsIntoOwnBacking, paint_layer->GetCompositingState());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());

  // Apply a transform to the fixed directly.
  fixed->setAttribute(HTMLNames::styleAttr, "transform: translate(1px, 0);");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(fixed->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_EQ(kNotComposited, paint_layer->GetCompositingState());
}

// Test that opacity applied to the fixed or an ancestor will cause the
// scrolling contents layer to not be promoted.
TEST_F(CompositingReasonFinderTest, OnlyOpaqueFixedLayersPromoted) {
  ScopedCompositeFixedPositionForTest composite_fixed_position(true);

  SetBodyInnerHTML(
      "<style>"
      "#fixed { position: fixed; height: 200px; width: 200px; background: "
      "white; top: 0}"
      "#spacer { height: 3000px; }"
      "</style>"
      "<div id=\"parent\">"
      "  <div id=\"fixed\"></div>"
      "  <div id=\"spacer\"></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::CompositeOpaqueScrollersEnabled());
  Element* parent = GetDocument().getElementById("parent");
  Element* fixed = GetDocument().getElementById("fixed");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(fixed->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_EQ(kPaintsIntoOwnBacking, paint_layer->GetCompositingState());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());

  // Change the parent to be partially translucent.
  parent->setAttribute(HTMLNames::styleAttr, "opacity: 0.5;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(fixed->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_EQ(kNotComposited, paint_layer->GetCompositingState());

  // Change the parent to be opaque again.
  parent->setAttribute(HTMLNames::styleAttr, "opacity: 1;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(fixed->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_EQ(kPaintsIntoOwnBacking, paint_layer->GetCompositingState());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());

  // Make the fixed translucent.
  fixed->setAttribute(HTMLNames::styleAttr, "opacity: 0.5");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(fixed->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_EQ(kNotComposited, paint_layer->GetCompositingState());
}

TEST_F(CompositingReasonFinderTest, RequiresCompositingForTransformAnimation) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetSubtreeWillChangeContents(false);

  style->SetHasCurrentTransformAnimation(false);
  style->SetIsRunningTransformAnimationOnCompositor(false);
  EXPECT_FALSE(
      CompositingReasonFinder::RequiresCompositingForTransformAnimation(
          *style));

  style->SetHasCurrentTransformAnimation(false);
  style->SetIsRunningTransformAnimationOnCompositor(true);
  EXPECT_FALSE(
      CompositingReasonFinder::RequiresCompositingForTransformAnimation(
          *style));

  style->SetHasCurrentTransformAnimation(true);
  style->SetIsRunningTransformAnimationOnCompositor(false);
  EXPECT_TRUE(CompositingReasonFinder::RequiresCompositingForTransformAnimation(
      *style));

  style->SetHasCurrentTransformAnimation(true);
  style->SetIsRunningTransformAnimationOnCompositor(true);
  EXPECT_TRUE(CompositingReasonFinder::RequiresCompositingForTransformAnimation(
      *style));

  style->SetSubtreeWillChangeContents(true);

  style->SetHasCurrentTransformAnimation(false);
  style->SetIsRunningTransformAnimationOnCompositor(false);
  EXPECT_FALSE(
      CompositingReasonFinder::RequiresCompositingForTransformAnimation(
          *style));

  style->SetHasCurrentTransformAnimation(false);
  style->SetIsRunningTransformAnimationOnCompositor(true);
  EXPECT_TRUE(CompositingReasonFinder::RequiresCompositingForTransformAnimation(
      *style));

  style->SetHasCurrentTransformAnimation(true);
  style->SetIsRunningTransformAnimationOnCompositor(false);
  EXPECT_FALSE(
      CompositingReasonFinder::RequiresCompositingForTransformAnimation(
          *style));

  style->SetHasCurrentTransformAnimation(true);
  style->SetIsRunningTransformAnimationOnCompositor(true);
  EXPECT_TRUE(CompositingReasonFinder::RequiresCompositingForTransformAnimation(
      *style));
}

TEST_F(CompositingReasonFinderTest, RequiresCompositingForEffectAnimation) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();

  style->SetSubtreeWillChangeContents(false);

  // In the interest of brevity, for each side of subtreeWillChangeContents()
  // code path we only check that any one of the effect related animation flags
  // being set produces true, rather than every permutation.

  style->SetHasCurrentOpacityAnimation(false);
  style->SetHasCurrentFilterAnimation(false);
  style->SetHasCurrentBackdropFilterAnimation(false);
  EXPECT_FALSE(
      CompositingReasonFinder::RequiresCompositingForEffectAnimation(*style));

  style->SetHasCurrentOpacityAnimation(true);
  style->SetHasCurrentFilterAnimation(false);
  style->SetHasCurrentBackdropFilterAnimation(false);
  EXPECT_TRUE(
      CompositingReasonFinder::RequiresCompositingForEffectAnimation(*style));

  style->SetHasCurrentOpacityAnimation(false);
  style->SetHasCurrentFilterAnimation(true);
  style->SetHasCurrentBackdropFilterAnimation(false);
  EXPECT_TRUE(
      CompositingReasonFinder::RequiresCompositingForEffectAnimation(*style));

  style->SetHasCurrentOpacityAnimation(false);
  style->SetHasCurrentFilterAnimation(false);
  style->SetHasCurrentBackdropFilterAnimation(true);
  EXPECT_TRUE(
      CompositingReasonFinder::RequiresCompositingForEffectAnimation(*style));

  // Check the other side of subtreeWillChangeContents.
  style->SetSubtreeWillChangeContents(true);
  style->SetHasCurrentOpacityAnimation(false);
  style->SetHasCurrentFilterAnimation(false);
  style->SetHasCurrentBackdropFilterAnimation(false);
  EXPECT_FALSE(
      CompositingReasonFinder::RequiresCompositingForEffectAnimation(*style));

  style->SetIsRunningOpacityAnimationOnCompositor(true);
  style->SetIsRunningFilterAnimationOnCompositor(false);
  style->SetIsRunningBackdropFilterAnimationOnCompositor(false);
  EXPECT_TRUE(
      CompositingReasonFinder::RequiresCompositingForEffectAnimation(*style));

  style->SetIsRunningOpacityAnimationOnCompositor(false);
  style->SetIsRunningFilterAnimationOnCompositor(true);
  style->SetIsRunningBackdropFilterAnimationOnCompositor(false);
  EXPECT_TRUE(
      CompositingReasonFinder::RequiresCompositingForEffectAnimation(*style));

  style->SetIsRunningOpacityAnimationOnCompositor(false);
  style->SetIsRunningFilterAnimationOnCompositor(false);
  style->SetIsRunningBackdropFilterAnimationOnCompositor(true);
  EXPECT_TRUE(
      CompositingReasonFinder::RequiresCompositingForEffectAnimation(*style));
}

TEST_F(CompositingReasonFinderTest, CompositeNestedSticky) {
  ScopedCompositeFixedPositionForTest composite_fixed_position(true);

  SetBodyInnerHTML(
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
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* outer_sticky = GetDocument().getElementById("outerSticky");
  PaintLayer* outer_sticky_layer =
      ToLayoutBoxModelObject(outer_sticky->GetLayoutObject())->Layer();
  ASSERT_TRUE(outer_sticky_layer);

  Element* inner_sticky = GetDocument().getElementById("innerSticky");
  PaintLayer* inner_sticky_layer =
      ToLayoutBoxModelObject(inner_sticky->GetLayoutObject())->Layer();
  ASSERT_TRUE(inner_sticky_layer);

  EXPECT_EQ(kPaintsIntoOwnBacking, outer_sticky_layer->GetCompositingState());
  EXPECT_EQ(kPaintsIntoOwnBacking, inner_sticky_layer->GetCompositingState());
}

}  // namespace blink
