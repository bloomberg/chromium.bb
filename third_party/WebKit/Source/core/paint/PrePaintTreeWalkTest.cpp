// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutTreeAsText.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintControllerPaintTest.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintPropertyTreePrinter.h"
#include "core/paint/PrePaintTreeWalk.h"
#include "platform/graphics/paint/GeometryMapper.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/text/TextStream.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PrePaintTreeWalkTest : public PaintControllerPaintTest {
 public:
  const TransformPaintPropertyNode* FramePreTranslation() {
    LocalFrameView* frame_view = GetDocument().View();
    if (RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
      return frame_view->GetLayoutView()
          ->FirstFragment()
          ->PaintProperties()
          ->PaintOffsetTranslation();
    }
    return frame_view->PreTranslation();
  }

  const TransformPaintPropertyNode* FrameScrollTranslation() {
    LocalFrameView* frame_view = GetDocument().View();
    if (RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
      return frame_view->GetLayoutView()
          ->FirstFragment()
          ->PaintProperties()
          ->ScrollTranslation();
    }
    return frame_view->ScrollTranslation();
  }

 protected:
  PaintLayer* GetPaintLayerByElementId(const char* id) {
    return ToLayoutBoxModelObject(GetLayoutObjectByElementId(id))->Layer();
  }

 private:
  void SetUp() override {
    Settings::SetMockScrollbarsEnabled(true);

    RenderingTest::SetUp();
    EnableCompositing();
  }

  void TearDown() override {
    RenderingTest::TearDown();

    Settings::SetMockScrollbarsEnabled(false);
  }
};

INSTANTIATE_TEST_CASE_P(All,
                        PrePaintTreeWalkTest,
                        ::testing::ValuesIn(kDefaultPaintTestConfigurations));

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithBorderInvalidation) {
  SetBodyInnerHTML(
      "<style>"
      "  body { margin: 0; }"
      "  #transformed { transform: translate(100px, 100px); }"
      "  .border { border: 10px solid black; }"
      "</style>"
      "<div id='transformed'></div>");

  auto* transformed_element = GetDocument().getElementById("transformed");
  const auto* transformed_properties = transformed_element->GetLayoutObject()
                                           ->FirstFragment()
                                           ->PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate(100, 100),
            transformed_properties->Transform()->Matrix());

  // Artifically change the transform node.
  const_cast<ObjectPaintProperties*>(transformed_properties)->ClearTransform();
  EXPECT_EQ(nullptr, transformed_properties->Transform());

  // Cause a paint invalidation.
  transformed_element->setAttribute(HTMLNames::classAttr, "border");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Should have changed back.
  EXPECT_EQ(TransformationMatrix().Translate(100, 100),
            transformed_properties->Transform()->Matrix());
}

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithFrameScroll) {
  SetBodyInnerHTML("<style> body { height: 10000px; } </style>");
  EXPECT_EQ(TransformationMatrix().Translate(0, 0),
            FrameScrollTranslation()->Matrix());

  // Cause a scroll invalidation and ensure the translation is updated.
  GetDocument().domWindow()->scrollTo(0, 100);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(TransformationMatrix().Translate(0, -100),
            FrameScrollTranslation()->Matrix());
}

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithCSSTransformInvalidation) {
  SetBodyInnerHTML(
      "<style>"
      "  .transformA { transform: translate(100px, 100px); }"
      "  .transformB { transform: translate(200px, 200px); }"
      "  #transformed { will-change: transform; }"
      "</style>"
      "<div id='transformed' class='transformA'></div>");

  auto* transformed_element = GetDocument().getElementById("transformed");
  const auto* transformed_properties = transformed_element->GetLayoutObject()
                                           ->FirstFragment()
                                           ->PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate(100, 100),
            transformed_properties->Transform()->Matrix());

  // Invalidate the CSS transform property.
  transformed_element->setAttribute(HTMLNames::classAttr, "transformB");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // The transform should have changed.
  EXPECT_EQ(TransformationMatrix().Translate(200, 200),
            transformed_properties->Transform()->Matrix());
}

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithOpacityInvalidation) {
  // In SPv1 mode, we don't need or store property tree nodes for effects.
  if (!RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return;
  SetBodyInnerHTML(
      "<style>"
      "  .opacityA { opacity: 0.9; }"
      "  .opacityB { opacity: 0.4; }"
      "</style>"
      "<div id='transparent' class='opacityA'></div>");

  auto* transparent_element = GetDocument().getElementById("transparent");
  const auto* transparent_properties = transparent_element->GetLayoutObject()
                                           ->FirstFragment()
                                           ->PaintProperties();
  EXPECT_EQ(0.9f, transparent_properties->Effect()->Opacity());

  // Invalidate the opacity property.
  transparent_element->setAttribute(HTMLNames::classAttr, "opacityB");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // The opacity should have changed.
  EXPECT_EQ(0.4f, transparent_properties->Effect()->Opacity());
}

TEST_P(PrePaintTreeWalkTest, ClearSubsequenceCachingClipChange) {
  SetBodyInnerHTML(
      "<style>"
      "  .clip { overflow: hidden }"
      "</style>"
      "<div id='parent' style='transform: translateZ(0); width: 100px;"
      "  height: 100px;'>"
      "  <div id='child' style='isolation: isolate'>"
      "    content"
      "  </div>"
      "</div>");

  auto* parent = GetDocument().getElementById("parent");
  auto* child = GetDocument().getElementById("child");
  auto* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  EXPECT_FALSE(child_paint_layer->NeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  parent->setAttribute(HTMLNames::classAttr, "clip");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_TRUE(child_paint_layer->NeedsRepaint());
}

TEST_P(PrePaintTreeWalkTest, ClearSubsequenceCachingClipChange2DTransform) {
  SetBodyInnerHTML(
      "<style>"
      "  .clip { overflow: hidden }"
      "</style>"
      "<div id='parent' style='transform: translateX(0); width: 100px;"
      "  height: 100px;'>"
      "  <div id='child' style='isolation: isolate'>"
      "    content"
      "  </div>"
      "</div>");

  auto* parent = GetDocument().getElementById("parent");
  auto* child = GetDocument().getElementById("child");
  auto* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  EXPECT_FALSE(child_paint_layer->NeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  parent->setAttribute(HTMLNames::classAttr, "clip");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_TRUE(child_paint_layer->NeedsRepaint());
}

TEST_P(PrePaintTreeWalkTest, ClearSubsequenceCachingClipChangePosAbs) {
  SetBodyInnerHTML(
      "<style>"
      "  .clip { overflow: hidden }"
      "</style>"
      "<div id='parent' style='transform: translateZ(0); width: 100px;"
      "  height: 100px; position: absolute'>"
      "  <div id='child' style='overflow: hidden; position: relative;"
      "      z-index: 0; width: 50px; height: 50px'>"
      "    content"
      "  </div>"
      "</div>");

  auto* parent = GetDocument().getElementById("parent");
  auto* child = GetDocument().getElementById("child");
  auto* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  EXPECT_FALSE(child_paint_layer->NeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  // This changes clips for absolute-positioned descendants of "child" but not
  // normal-position ones, which are already clipped to 50x50.
  parent->setAttribute(HTMLNames::classAttr, "clip");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_TRUE(child_paint_layer->NeedsRepaint());
}

TEST_P(PrePaintTreeWalkTest, ClearSubsequenceCachingClipChangePosFixed) {
  SetBodyInnerHTML(
      "<style>"
      "  .clip { overflow: hidden }"
      "</style>"
      "<div id='parent' style='transform: translateZ(0); width: 100px;"
      "  height: 100px; trans'>"
      "  <div id='child' style='overflow: hidden; z-index: 0;"
      "      position: absolute; width: 50px; height: 50px'>"
      "    content"
      "  </div>"
      "</div>");

  auto* parent = GetDocument().getElementById("parent");
  auto* child = GetDocument().getElementById("child");
  auto* child_paint_layer =
      ToLayoutBoxModelObject(child->GetLayoutObject())->Layer();
  EXPECT_FALSE(child_paint_layer->NeedsRepaint());
  EXPECT_FALSE(child_paint_layer->NeedsPaintPhaseFloat());

  // This changes clips for absolute-positioned descendants of "child" but not
  // normal-position ones, which are already clipped to 50x50.
  parent->setAttribute(HTMLNames::classAttr, "clip");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_TRUE(child_paint_layer->NeedsRepaint());
}

TEST_P(PrePaintTreeWalkTest, VisualRectClipForceSubtree) {
  SetBodyInnerHTML(
      "<style>"
      "  #parent { height: 75px; position: relative; width: 100px; }"
      "</style>"
      "<div id='parent' style='height: 100px;'>"
      "  <div id='child' style='overflow: hidden; width: 100%; height: 100%; "
      "      position: relative'>"
      "    <div>"
      "      <div id='grandchild' style='width: 50px; height: 200px; '>"
      "      </div>"
      "    </div>"
      "  </div>"
      "</div>");

  auto* grandchild = GetLayoutObjectByElementId("grandchild");

  GetDocument().getElementById("parent")->removeAttribute("style");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // In SPv175 mode, VisualRects are in the space of the containing transform
  // node without applying any ancestor property nodes, including clip.
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    EXPECT_EQ(200, grandchild->VisualRect().Height());
  else
    EXPECT_EQ(75, grandchild->VisualRect().Height());
}

TEST_P(PrePaintTreeWalkTest, ClipChangeHasRadius) {
  SetBodyInnerHTML(
      "<style>"
      "  #target {"
      "    position: absolute;"
      "    z-index: 0;"
      "    overflow: hidden;"
      "    width: 50px;"
      "    height: 50px;"
      "  }"
      "</style>"
      "<div id='target'></div>");

  auto* target = GetDocument().getElementById("target");
  auto* target_object = ToLayoutBoxModelObject(target->GetLayoutObject());
  target->setAttribute(HTMLNames::styleAttr, "border-radius: 5px");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(target_object->Layer()->NeedsRepaint());
  // And should not trigger any assert failure.
  GetDocument().View()->UpdateAllLifecyclePhases();
}

}  // namespace blink
