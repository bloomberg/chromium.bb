// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPaintInvalidator.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/paint/PaintLayer.h"
#include "platform/json/JSONValues.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using ObjectPaintInvalidatorTest = RenderingTest;

TEST_F(ObjectPaintInvalidatorTest,
       TraverseNonCompositingDescendantsInPaintOrder) {
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  enableCompositing();
  setBodyInnerHTML(
      "<style>div { width: 10px; height: 10px; background-color: green; "
      "}</style>"
      "<div id='container' style='position: fixed'>"
      "  <div id='normal-child'></div>"
      "  <div id='stacked-child' style='position: relative'></div>"
      "  <div id='composited-stacking-context' style='will-change: transform'>"
      "    <div id='normal-child-of-composited-stacking-context'></div>"
      "    <div id='stacked-child-of-composited-stacking-context' "
      "style='position: relative'></div>"
      "  </div>"
      "  <div id='composited-non-stacking-context' style='backface-visibility: "
      "hidden'>"
      "    <div id='normal-child-of-composited-non-stacking-context'></div>"
      "    <div id='stacked-child-of-composited-non-stacking-context' "
      "style='position: relative'></div>"
      "    <div "
      "id='non-stacked-layered-child-of-composited-non-stacking-context' "
      "style='overflow: scroll'></div>"
      "  </div>"
      "</div>");

  document().view()->setTracksPaintInvalidations(true);
  ObjectPaintInvalidator(*getLayoutObjectByElementId("container"))
      .invalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationSubtree);
  std::unique_ptr<JSONArray> invalidations =
      document().view()->trackedObjectPaintInvalidationsAsJSON();
  document().view()->setTracksPaintInvalidations(false);

  ASSERT_EQ(4u, invalidations->size());
  String s;
  JSONObject::cast(invalidations->at(0))->get("object")->asString(&s);
  EXPECT_EQ(getLayoutObjectByElementId("container")->debugName(), s);
  JSONObject::cast(invalidations->at(1))->get("object")->asString(&s);
  EXPECT_EQ(getLayoutObjectByElementId("normal-child")->debugName(), s);
  JSONObject::cast(invalidations->at(2))->get("object")->asString(&s);
  EXPECT_EQ(getLayoutObjectByElementId("stacked-child")->debugName(), s);
  JSONObject::cast(invalidations->at(3))->get("object")->asString(&s);
  EXPECT_EQ(getLayoutObjectByElementId(
                "stacked-child-of-composited-non-stacking-context")
                ->debugName(),
            s);
}

TEST_F(ObjectPaintInvalidatorTest, TraverseFloatUnderCompositedInline) {
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  enableCompositing();
  setBodyInnerHTML(
      "<div id='compositedContainer' style='position: relative;"
      "    will-change: transform'>"
      "  <div id='containingBlock' style='position: relative'>"
      "    <span id='span' style='position: relative; will-change: transform'>"
      "      <div id='target' style='float: right'></div>"
      "    </span>"
      "  </div>"
      "</div>");

  auto* target = getLayoutObjectByElementId("target");
  auto* containingBlock = getLayoutObjectByElementId("containingBlock");
  auto* containingBlockLayer = toLayoutBoxModelObject(containingBlock)->layer();
  auto* compositedContainer = getLayoutObjectByElementId("compositedContainer");
  auto* compositedContainerLayer =
      toLayoutBoxModelObject(compositedContainer)->layer();
  auto* span = getLayoutObjectByElementId("span");
  auto* spanLayer = toLayoutBoxModelObject(span)->layer();

  // Thought |target| is under |span| which is a composited stacking context,
  // |span| is not the paint invalidation container of |target|.
  EXPECT_TRUE(span->isPaintInvalidationContainer());
  EXPECT_TRUE(span->styleRef().isStackingContext());
  EXPECT_EQ(compositedContainer, &target->containerForPaintInvalidation());
  EXPECT_EQ(containingBlockLayer, target->paintingLayer());

  // Traversing from target should mark needsRepaint on correct layers.
  EXPECT_FALSE(containingBlockLayer->needsRepaint());
  EXPECT_FALSE(compositedContainerLayer->needsRepaint());
  ObjectPaintInvalidator(*target)
      .invalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationSubtree);
  EXPECT_TRUE(containingBlockLayer->needsRepaint());
  EXPECT_TRUE(compositedContainerLayer->needsRepaint());
  EXPECT_FALSE(spanLayer->needsRepaint());

  document().view()->updateAllLifecyclePhases();

  // Traversing from span should mark needsRepaint on correct layers for target.
  EXPECT_FALSE(containingBlockLayer->needsRepaint());
  EXPECT_FALSE(compositedContainerLayer->needsRepaint());
  ObjectPaintInvalidator(*span)
      .invalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationSubtree);
  EXPECT_TRUE(containingBlockLayer->needsRepaint());
  EXPECT_TRUE(compositedContainerLayer->needsRepaint());
  EXPECT_TRUE(spanLayer->needsRepaint());

  document().view()->updateAllLifecyclePhases();

  // Traversing from compositedContainer should reach target.
  document().view()->setTracksPaintInvalidations(true);
  EXPECT_FALSE(containingBlockLayer->needsRepaint());
  EXPECT_FALSE(compositedContainerLayer->needsRepaint());
  ObjectPaintInvalidator(*compositedContainer)
      .invalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationSubtree);
  EXPECT_TRUE(containingBlockLayer->needsRepaint());
  EXPECT_TRUE(compositedContainerLayer->needsRepaint());
  EXPECT_FALSE(spanLayer->needsRepaint());

  std::unique_ptr<JSONArray> invalidations =
      document().view()->trackedObjectPaintInvalidationsAsJSON();
  document().view()->setTracksPaintInvalidations(false);

  ASSERT_EQ(4u, invalidations->size());
  String s;
  JSONObject::cast(invalidations->at(0))->get("object")->asString(&s);
  EXPECT_EQ(compositedContainer->debugName(), s);
  JSONObject::cast(invalidations->at(1))->get("object")->asString(&s);
  EXPECT_EQ(containingBlock->debugName(), s);
  JSONObject::cast(invalidations->at(2))->get("object")->asString(&s);
  EXPECT_EQ(target->debugName(), s);
  // This is the text node after the span.
  JSONObject::cast(invalidations->at(3))->get("object")->asString(&s);
  EXPECT_EQ("LayoutText #text", s);
}

TEST_F(ObjectPaintInvalidatorTest,
       TraverseFloatUnderMultiLevelCompositedInlines) {
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  enableCompositing();
  setBodyInnerHTML(
      "<div id='compositedContainer' style='position: relative;"
      "    will-change: transform'>"
      "  <div id='containingBlock' style='position: relative; z-index: 0'>"
      "    <span id='span' style='position: relative; will-change: transform'>"
      "      <span id='innerSpan'"
      "          style='position: relative; will-change: transform'>"
      "        <div id='target' style='float: right'></div>"
      "      </span>"
      "    </span>"
      "  </div>"
      "</div>");

  auto* target = getLayoutObjectByElementId("target");
  auto* containingBlock = getLayoutObjectByElementId("containingBlock");
  auto* containingBlockLayer = toLayoutBoxModelObject(containingBlock)->layer();
  auto* compositedContainer = getLayoutObjectByElementId("compositedContainer");
  auto* compositedContainerLayer =
      toLayoutBoxModelObject(compositedContainer)->layer();
  auto* span = getLayoutObjectByElementId("span");
  auto* spanLayer = toLayoutBoxModelObject(span)->layer();
  auto* innerSpan = getLayoutObjectByElementId("innerSpan");
  auto* innerSpanLayer = toLayoutBoxModelObject(innerSpan)->layer();

  EXPECT_TRUE(span->isPaintInvalidationContainer());
  EXPECT_TRUE(span->styleRef().isStackingContext());
  EXPECT_TRUE(innerSpan->isPaintInvalidationContainer());
  EXPECT_TRUE(innerSpan->styleRef().isStackingContext());
  EXPECT_EQ(compositedContainer, &target->containerForPaintInvalidation());
  EXPECT_EQ(containingBlockLayer, target->paintingLayer());

  // Traversing from compositedContainer should reach target.
  document().view()->setTracksPaintInvalidations(true);
  EXPECT_FALSE(containingBlockLayer->needsRepaint());
  EXPECT_FALSE(compositedContainerLayer->needsRepaint());
  ObjectPaintInvalidator(*compositedContainer)
      .invalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationSubtree);
  EXPECT_TRUE(containingBlockLayer->needsRepaint());
  EXPECT_TRUE(compositedContainerLayer->needsRepaint());
  EXPECT_FALSE(spanLayer->needsRepaint());
  EXPECT_FALSE(innerSpanLayer->needsRepaint());

  std::unique_ptr<JSONArray> invalidations =
      document().view()->trackedObjectPaintInvalidationsAsJSON();
  document().view()->setTracksPaintInvalidations(false);

  ASSERT_EQ(4u, invalidations->size());
  String s;
  JSONObject::cast(invalidations->at(0))->get("object")->asString(&s);
  EXPECT_EQ(compositedContainer->debugName(), s);
  JSONObject::cast(invalidations->at(1))->get("object")->asString(&s);
  EXPECT_EQ(containingBlock->debugName(), s);
  JSONObject::cast(invalidations->at(2))->get("object")->asString(&s);
  EXPECT_EQ(target->debugName(), s);
  // This is the text node after the span.
  JSONObject::cast(invalidations->at(3))->get("object")->asString(&s);
  EXPECT_EQ("LayoutText #text", s);
}

TEST_F(ObjectPaintInvalidatorTest, TraverseStackedFloatUnderCompositedInline) {
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  enableCompositing();
  setBodyInnerHTML(
      "<span id='span' style='position: relative; will-change: transform'>"
      "  <div id='target' style='position: relative; float: right'></div>"
      "</span>");

  auto* target = getLayoutObjectByElementId("target");
  auto* targetLayer = toLayoutBoxModelObject(target)->layer();
  auto* span = getLayoutObjectByElementId("span");
  auto* spanLayer = toLayoutBoxModelObject(span)->layer();

  EXPECT_TRUE(span->isPaintInvalidationContainer());
  EXPECT_TRUE(span->styleRef().isStackingContext());
  EXPECT_EQ(span, &target->containerForPaintInvalidation());
  EXPECT_EQ(targetLayer, target->paintingLayer());

  // Traversing from span should reach target.
  document().view()->setTracksPaintInvalidations(true);
  EXPECT_FALSE(spanLayer->needsRepaint());
  ObjectPaintInvalidator(*span)
      .invalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationSubtree);
  EXPECT_TRUE(spanLayer->needsRepaint());

  std::unique_ptr<JSONArray> invalidations =
      document().view()->trackedObjectPaintInvalidationsAsJSON();
  document().view()->setTracksPaintInvalidations(false);

  ASSERT_EQ(3u, invalidations->size());
  String s;
  JSONObject::cast(invalidations->at(0))->get("object")->asString(&s);
  EXPECT_EQ(span->debugName(), s);
  JSONObject::cast(invalidations->at(1))->get("object")->asString(&s);
  EXPECT_EQ("LayoutText #text", s);
  JSONObject::cast(invalidations->at(2))->get("object")->asString(&s);
  EXPECT_EQ(target->debugName(), s);
}

}  // namespace blink
