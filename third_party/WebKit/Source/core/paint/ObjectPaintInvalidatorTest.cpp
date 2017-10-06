// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPaintInvalidator.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/json/JSONValues.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using ObjectPaintInvalidatorTest = RenderingTest;

TEST_F(ObjectPaintInvalidatorTest,
       TraverseNonCompositingDescendantsInPaintOrder) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  EnableCompositing();
  SetBodyInnerHTML(
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

  GetDocument().View()->SetTracksPaintInvalidations(true);
  ObjectPaintInvalidator(*GetLayoutObjectByElementId("container"))
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  std::unique_ptr<JSONArray> invalidations =
      GetDocument().View()->TrackedObjectPaintInvalidationsAsJSON();
  GetDocument().View()->SetTracksPaintInvalidations(false);

  ASSERT_EQ(4u, invalidations->size());
  String s;
  JSONObject::Cast(invalidations->at(0))->Get("object")->AsString(&s);
  EXPECT_EQ(GetLayoutObjectByElementId("container")->DebugName(), s);
  JSONObject::Cast(invalidations->at(1))->Get("object")->AsString(&s);
  EXPECT_EQ(GetLayoutObjectByElementId("normal-child")->DebugName(), s);
  JSONObject::Cast(invalidations->at(2))->Get("object")->AsString(&s);
  EXPECT_EQ(GetLayoutObjectByElementId("stacked-child")->DebugName(), s);
  JSONObject::Cast(invalidations->at(3))->Get("object")->AsString(&s);
  EXPECT_EQ(GetLayoutObjectByElementId(
                "stacked-child-of-composited-non-stacking-context")
                ->DebugName(),
            s);
}

TEST_F(ObjectPaintInvalidatorTest, TraverseFloatUnderCompositedInline) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='compositedContainer' style='position: relative;"
      "    will-change: transform'>"
      "  <div id='containingBlock' style='position: relative'>"
      "    <span id='span' style='position: relative; will-change: transform'>"
      "      <div id='target' style='float: right'></div>"
      "    </span>"
      "  </div>"
      "</div>");

  auto* target = GetLayoutObjectByElementId("target");
  auto* containing_block = GetLayoutObjectByElementId("containingBlock");
  auto* containing_block_layer =
      ToLayoutBoxModelObject(containing_block)->Layer();
  auto* composited_container =
      GetLayoutObjectByElementId("compositedContainer");
  auto* composited_container_layer =
      ToLayoutBoxModelObject(composited_container)->Layer();
  auto* span = GetLayoutObjectByElementId("span");
  auto* span_layer = ToLayoutBoxModelObject(span)->Layer();

  // Thought |target| is under |span| which is a composited stacking context,
  // |span| is not the paint invalidation container of |target|.
  EXPECT_TRUE(span->IsPaintInvalidationContainer());
  EXPECT_TRUE(span->StyleRef().IsStackingContext());
  EXPECT_EQ(composited_container, &target->ContainerForPaintInvalidation());
  EXPECT_EQ(containing_block_layer, target->PaintingLayer());

  // Traversing from target should mark needsRepaint on correct layers.
  EXPECT_FALSE(containing_block_layer->NeedsRepaint());
  EXPECT_FALSE(composited_container_layer->NeedsRepaint());
  ObjectPaintInvalidator(*target)
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  EXPECT_TRUE(containing_block_layer->NeedsRepaint());
  EXPECT_TRUE(composited_container_layer->NeedsRepaint());
  EXPECT_FALSE(span_layer->NeedsRepaint());

  GetDocument().View()->UpdateAllLifecyclePhases();

  // Traversing from span should mark needsRepaint on correct layers for target.
  EXPECT_FALSE(containing_block_layer->NeedsRepaint());
  EXPECT_FALSE(composited_container_layer->NeedsRepaint());
  ObjectPaintInvalidator(*span)
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  EXPECT_TRUE(containing_block_layer->NeedsRepaint());
  EXPECT_TRUE(composited_container_layer->NeedsRepaint());
  EXPECT_TRUE(span_layer->NeedsRepaint());

  GetDocument().View()->UpdateAllLifecyclePhases();

  // Traversing from compositedContainer should reach target.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  EXPECT_FALSE(containing_block_layer->NeedsRepaint());
  EXPECT_FALSE(composited_container_layer->NeedsRepaint());
  ObjectPaintInvalidator(*composited_container)
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  EXPECT_TRUE(containing_block_layer->NeedsRepaint());
  EXPECT_TRUE(composited_container_layer->NeedsRepaint());
  EXPECT_FALSE(span_layer->NeedsRepaint());

  std::unique_ptr<JSONArray> invalidations =
      GetDocument().View()->TrackedObjectPaintInvalidationsAsJSON();
  GetDocument().View()->SetTracksPaintInvalidations(false);

  ASSERT_EQ(4u, invalidations->size());
  String s;
  JSONObject::Cast(invalidations->at(0))->Get("object")->AsString(&s);
  EXPECT_EQ(composited_container->DebugName(), s);
  JSONObject::Cast(invalidations->at(1))->Get("object")->AsString(&s);
  EXPECT_EQ(containing_block->DebugName(), s);
  JSONObject::Cast(invalidations->at(2))->Get("object")->AsString(&s);
  EXPECT_EQ(target->DebugName(), s);
  // This is the text node after the span.
  JSONObject::Cast(invalidations->at(3))->Get("object")->AsString(&s);
  EXPECT_EQ("LayoutText #text", s);
}

TEST_F(ObjectPaintInvalidatorTest,
       TraverseFloatUnderMultiLevelCompositedInlines) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  EnableCompositing();
  SetBodyInnerHTML(
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

  auto* target = GetLayoutObjectByElementId("target");
  auto* containing_block = GetLayoutObjectByElementId("containingBlock");
  auto* containing_block_layer =
      ToLayoutBoxModelObject(containing_block)->Layer();
  auto* composited_container =
      GetLayoutObjectByElementId("compositedContainer");
  auto* composited_container_layer =
      ToLayoutBoxModelObject(composited_container)->Layer();
  auto* span = GetLayoutObjectByElementId("span");
  auto* span_layer = ToLayoutBoxModelObject(span)->Layer();
  auto* inner_span = GetLayoutObjectByElementId("innerSpan");
  auto* inner_span_layer = ToLayoutBoxModelObject(inner_span)->Layer();

  EXPECT_TRUE(span->IsPaintInvalidationContainer());
  EXPECT_TRUE(span->StyleRef().IsStackingContext());
  EXPECT_TRUE(inner_span->IsPaintInvalidationContainer());
  EXPECT_TRUE(inner_span->StyleRef().IsStackingContext());
  EXPECT_EQ(composited_container, &target->ContainerForPaintInvalidation());
  EXPECT_EQ(containing_block_layer, target->PaintingLayer());

  // Traversing from compositedContainer should reach target.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  EXPECT_FALSE(containing_block_layer->NeedsRepaint());
  EXPECT_FALSE(composited_container_layer->NeedsRepaint());
  ObjectPaintInvalidator(*composited_container)
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  EXPECT_TRUE(containing_block_layer->NeedsRepaint());
  EXPECT_TRUE(composited_container_layer->NeedsRepaint());
  EXPECT_FALSE(span_layer->NeedsRepaint());
  EXPECT_FALSE(inner_span_layer->NeedsRepaint());

  std::unique_ptr<JSONArray> invalidations =
      GetDocument().View()->TrackedObjectPaintInvalidationsAsJSON();
  GetDocument().View()->SetTracksPaintInvalidations(false);

  ASSERT_EQ(4u, invalidations->size());
  String s;
  JSONObject::Cast(invalidations->at(0))->Get("object")->AsString(&s);
  EXPECT_EQ(composited_container->DebugName(), s);
  JSONObject::Cast(invalidations->at(1))->Get("object")->AsString(&s);
  EXPECT_EQ(containing_block->DebugName(), s);
  JSONObject::Cast(invalidations->at(2))->Get("object")->AsString(&s);
  EXPECT_EQ(target->DebugName(), s);
  // This is the text node after the span.
  JSONObject::Cast(invalidations->at(3))->Get("object")->AsString(&s);
  EXPECT_EQ("LayoutText #text", s);
}

TEST_F(ObjectPaintInvalidatorTest, TraverseStackedFloatUnderCompositedInline) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  EnableCompositing();
  SetBodyInnerHTML(
      "<span id='span' style='position: relative; will-change: transform'>"
      "  <div id='target' style='position: relative; float: right'></div>"
      "</span>");

  auto* target = GetLayoutObjectByElementId("target");
  auto* target_layer = ToLayoutBoxModelObject(target)->Layer();
  auto* span = GetLayoutObjectByElementId("span");
  auto* span_layer = ToLayoutBoxModelObject(span)->Layer();

  EXPECT_TRUE(span->IsPaintInvalidationContainer());
  EXPECT_TRUE(span->StyleRef().IsStackingContext());
  EXPECT_EQ(span, &target->ContainerForPaintInvalidation());
  EXPECT_EQ(target_layer, target->PaintingLayer());

  // Traversing from span should reach target.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  EXPECT_FALSE(span_layer->NeedsRepaint());
  ObjectPaintInvalidator(*span)
      .InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
          PaintInvalidationReason::kSubtree);
  EXPECT_TRUE(span_layer->NeedsRepaint());

  std::unique_ptr<JSONArray> invalidations =
      GetDocument().View()->TrackedObjectPaintInvalidationsAsJSON();
  GetDocument().View()->SetTracksPaintInvalidations(false);

  ASSERT_EQ(3u, invalidations->size());
  String s;
  JSONObject::Cast(invalidations->at(0))->Get("object")->AsString(&s);
  EXPECT_EQ(span->DebugName(), s);
  JSONObject::Cast(invalidations->at(1))->Get("object")->AsString(&s);
  EXPECT_EQ("LayoutText #text", s);
  JSONObject::Cast(invalidations->at(2))->Get("object")->AsString(&s);
  EXPECT_EQ(target->DebugName(), s);
}

TEST_F(ObjectPaintInvalidatorTest, InvalidatePaintRectangle) {
  EnableCompositing();
  SetBodyInnerHTML(
      "<div id='target' style='width: 200px; height: 200px; background: blue'>"
      "</div>");

  GetDocument().View()->SetTracksPaintInvalidations(true);

  auto* target = GetLayoutObjectByElementId("target");
  target->InvalidatePaintRectangle(LayoutRect(10, 10, 50, 50));
  EXPECT_EQ(LayoutRect(10, 10, 50, 50), target->PartialInvalidationRect());
  target->InvalidatePaintRectangle(LayoutRect(30, 30, 60, 60));
  EXPECT_EQ(LayoutRect(10, 10, 80, 80), target->PartialInvalidationRect());
  EXPECT_TRUE(target->MayNeedPaintInvalidation());

  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(), target->PartialInvalidationRect());

  auto object_invalidations =
      GetDocument().View()->TrackedObjectPaintInvalidationsAsJSON();
  ASSERT_EQ(1u, object_invalidations->size());
  String s;
  const auto* entry = JSONObject::Cast(object_invalidations->at(0));
  entry->Get("reason")->AsString(&s);
  EXPECT_EQ(String(PaintInvalidationReasonToString(
                PaintInvalidationReason::kRectangle)),
            s);
  entry->Get("object")->AsString(&s);
  EXPECT_EQ(target->DebugName(), s);

  const auto& raster_invalidations = GetLayoutView()
                                         .Layer()
                                         ->GraphicsLayerBacking()
                                         ->GetRasterInvalidationTracking()
                                         ->Invalidations();
  ASSERT_EQ(1u, raster_invalidations.size());
  EXPECT_EQ(IntRect(18, 18, 80, 80), raster_invalidations[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kRectangle,
            raster_invalidations[0].reason);

  GetDocument().View()->SetTracksPaintInvalidations(false);
}

}  // namespace blink
