// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/compositing/CompositingInputsUpdater.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/UseCounter.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/compositing/CompositedLayerMapping.h"
#include "core/paint/compositing/PaintLayerCompositor.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

CompositingInputsUpdater::CompositingInputsUpdater(PaintLayer* root_layer)
    : geometry_map_(kUseTransforms), root_layer_(root_layer) {}

CompositingInputsUpdater::~CompositingInputsUpdater() {}

void CompositingInputsUpdater::Update() {
  TRACE_EVENT0("blink", "CompositingInputsUpdater::update");
  UpdateRecursive(root_layer_, kDoNotForceUpdate, AncestorInfo());
}

static const PaintLayer* FindParentLayerOnClippingContainerChain(
    const PaintLayer* layer) {
  LayoutObject* current = &layer->GetLayoutObject();
  while (current) {
    if (current->Style()->GetPosition() == EPosition::kFixed) {
      for (current = current->Parent();
           current && !current->CanContainFixedPositionObjects();
           current = current->Parent()) {
        // CSS clip applies to fixed position elements even for ancestors that
        // are not what the fixed element is positioned with respect to.
        if (current->HasClip()) {
          DCHECK(current->HasLayer());
          return static_cast<const LayoutBoxModelObject*>(current)->Layer();
        }
      }
    } else {
      current = current->ContainingBlock();
    }

    if (current->HasLayer())
      return static_cast<const LayoutBoxModelObject*>(current)->Layer();
    // Having clip or overflow clip forces the LayoutObject to become a layer,
    // except for contains: paint, which may apply to SVG, and
    // control clip, which may apply to LayoutBox subtypes.
    // SVG (other than LayoutSVGRoot) cannot have PaintLayers.
    DCHECK(!current->HasClipRelatedProperty() ||
           current->StyleRef().ContainsPaint() ||
           (current->IsBox() && ToLayoutBox(current)->HasControlClip()) ||
           current->IsSVGChild());
  }
  NOTREACHED();
  return nullptr;
}

static const PaintLayer* FindParentLayerOnContainingBlockChain(
    const LayoutObject* object) {
  for (const LayoutObject* current = object; current;
       current = current->ContainingBlock()) {
    if (current->HasLayer())
      return static_cast<const LayoutBoxModelObject*>(current)->Layer();
  }
  NOTREACHED();
  return nullptr;
}

static bool HasClippedStackingAncestor(const PaintLayer* layer,
                                       const PaintLayer* clipping_layer) {
  if (layer == clipping_layer)
    return false;
  const LayoutObject& clipping_layout_object =
      clipping_layer->GetLayoutObject();
  for (const PaintLayer* current = layer->CompositingContainer(); current;
       current = current->CompositingContainer()) {
    if (clipping_layout_object.IsDescendantOf(&current->GetLayoutObject()))
      break;

    if (current->GetLayoutObject().HasClipRelatedProperty())
      return true;

    if (const LayoutObject* container = current->ClippingContainer()) {
      if (!clipping_layout_object.IsDescendantOf(container))
        return true;
    }
  }
  return false;
}

void CompositingInputsUpdater::UpdateRecursive(PaintLayer* layer,
                                               UpdateType update_type,
                                               AncestorInfo info) {
  if (!layer->ChildNeedsCompositingInputsUpdate() &&
      update_type != kForceUpdate)
    return;

  const PaintLayer* previous_overflow_layer = layer->AncestorOverflowLayer();
  layer->UpdateAncestorOverflowLayer(info.last_overflow_clip_layer);
  if (info.last_overflow_clip_layer && layer->NeedsCompositingInputsUpdate() &&
      layer->GetLayoutObject().Style()->HasStickyConstrainedPosition()) {
    if (!RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
      if (info.last_overflow_clip_layer != previous_overflow_layer) {
        // Old ancestor scroller should no longer have these constraints.
        DCHECK(!previous_overflow_layer ||
               !previous_overflow_layer->GetScrollableArea()
                    ->GetStickyConstraintsMap()
                    .Contains(layer));

        // If our ancestor scroller has changed and the previous one was the
        // root layer, we are no longer viewport constrained.
        if (previous_overflow_layer && previous_overflow_layer->IsRootLayer()) {
          layer->GetLayoutObject()
              .View()
              ->GetFrameView()
              ->RemoveViewportConstrainedObject(layer->GetLayoutObject());
        }
      }

      if (info.last_overflow_clip_layer->IsRootLayer()) {
        layer->GetLayoutObject()
            .View()
            ->GetFrameView()
            ->AddViewportConstrainedObject(layer->GetLayoutObject());
      }
    }
    layer->GetLayoutObject().UpdateStickyPositionConstraints();

    // Sticky position constraints and ancestor overflow scroller affect
    // the sticky layer position, so we need to update it again here.
    // TODO(flackr): This should be refactored in the future to be clearer
    // (i.e. update layer position and ancestor inputs updates in the
    // same walk)
    layer->UpdateLayerPosition();
  }

  geometry_map_.PushMappingsToAncestor(layer, layer->Parent());

  if (layer->HasCompositedLayerMapping())
    info.enclosing_composited_layer = layer;

  if (layer->NeedsCompositingInputsUpdate()) {
    if (info.enclosing_composited_layer) {
      info.enclosing_composited_layer->GetCompositedLayerMapping()
          ->SetNeedsGraphicsLayerUpdate(kGraphicsLayerUpdateSubtree);
    }
    update_type = kForceUpdate;
  }

  if (update_type == kForceUpdate) {
    PaintLayer::AncestorDependentCompositingInputs properties;

    if (!layer->IsRootLayer()) {
      if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
        properties.unclipped_absolute_bounding_box =
            EnclosingIntRect(geometry_map_.AbsoluteRect(
                FloatRect(layer->BoundingBoxForCompositingOverlapTest())));
        // FIXME: Setting the absBounds to 1x1 instead of 0x0 makes very little
        // sense, but removing this code will make JSGameBench sad.
        // See https://codereview.chromium.org/13912020/
        if (properties.unclipped_absolute_bounding_box.IsEmpty())
          properties.unclipped_absolute_bounding_box.SetSize(IntSize(1, 1));

        ClipRect clip_rect;
        layer->Clipper(PaintLayer::kDoNotUseGeometryMapper)
            .CalculateBackgroundClipRect(
                ClipRectsContext(root_layer_, kAbsoluteClipRects), clip_rect);
        IntRect snapped_clip_rect = PixelSnappedIntRect(clip_rect.Rect());
        properties.clipped_absolute_bounding_box =
            properties.unclipped_absolute_bounding_box;
        properties.clipped_absolute_bounding_box.Intersect(snapped_clip_rect);
      }

      const PaintLayer* parent = layer->Parent();
      properties.opacity_ancestor =
          parent->IsTransparent() ? parent : parent->OpacityAncestor();
      properties.transform_ancestor =
          parent->Transform() ? parent : parent->TransformAncestor();
      properties.filter_ancestor = parent->HasFilterInducingProperty()
                                       ? parent
                                       : parent->FilterAncestor();
      bool layer_is_fixed_position =
          layer->GetLayoutObject().Style()->GetPosition() == EPosition::kFixed;

      if (layer_is_fixed_position && properties.filter_ancestor &&
          layer->FixedToViewport()) {
        UseCounter::Count(layer->GetLayoutObject().GetDocument(),
                          WebFeature::kViewportFixedPositionUnderFilter);
      }

      properties.nearest_fixed_position_layer =
          layer_is_fixed_position ? layer : parent->NearestFixedPositionLayer();

      if (info.has_ancestor_with_clip_related_property) {
        const PaintLayer* parent_layer_on_clipping_container_chain =
            FindParentLayerOnClippingContainerChain(layer);
        const bool parent_has_clip_related_property =
            parent_layer_on_clipping_container_chain->GetLayoutObject()
                .HasClipRelatedProperty();
        properties.clipping_container =
            parent_has_clip_related_property
                ? &parent_layer_on_clipping_container_chain->GetLayoutObject()
                : parent_layer_on_clipping_container_chain->ClippingContainer();

        const PaintLayer* clipping_layer =
            properties.clipping_container
                ? properties.clipping_container->EnclosingLayer()
                : layer->Compositor()->RootLayer();

        if (!layer->SubtreeIsInvisible()) {
          if (layer->GetLayoutObject().IsOutOfFlowPositioned()) {
            if (HasClippedStackingAncestor(layer, clipping_layer))
              properties.clip_parent = clipping_layer;
          } else {
            if (clipping_layer && clipping_layer->CompositingContainer() ==
                                      layer->CompositingContainer()) {
              // If the clipping container of |layer| is a sibling in the
              // stacking tree, and it escapes a stacking ancestor clip,
              // this layer should escape that clip also.
              properties.clip_parent = clipping_layer->ClipParent();
            }
          }
        }
      }

      if (info.last_scrolling_ancestor) {
        const LayoutObject* containing_block =
            layer->GetLayoutObject().ContainingBlock();
        const PaintLayer* parent_layer_on_containing_block_chain =
            FindParentLayerOnContainingBlockChain(containing_block);

        properties.ancestor_scrolling_layer =
            parent_layer_on_containing_block_chain->AncestorScrollingLayer();
        if (parent_layer_on_containing_block_chain->ScrollsOverflow()) {
          properties.ancestor_scrolling_layer =
              parent_layer_on_containing_block_chain;
        }

        if (layer->StackingNode()->IsStacked() &&
            properties.ancestor_scrolling_layer &&
            !info.ancestor_stacking_context->GetLayoutObject().IsDescendantOf(
                &properties.ancestor_scrolling_layer->GetLayoutObject()))
          properties.scroll_parent = properties.ancestor_scrolling_layer;
      }
    }

    layer->UpdateAncestorDependentCompositingInputs(
        properties, info.has_ancestor_with_clip_path);
  }

  if (layer->StackingNode()->IsStackingContext())
    info.ancestor_stacking_context = layer;

  if (layer->IsRootLayer() || layer->GetLayoutObject().HasOverflowClip())
    info.last_overflow_clip_layer = layer;

  if (layer->ScrollsOverflow())
    info.last_scrolling_ancestor = layer;

  if (layer->GetLayoutObject().HasClipRelatedProperty())
    info.has_ancestor_with_clip_related_property = true;

  if (layer->GetLayoutObject().HasClipPath())
    info.has_ancestor_with_clip_path = true;

  for (PaintLayer* child = layer->FirstChild(); child;
       child = child->NextSibling())
    UpdateRecursive(child, update_type, info);

  layer->DidUpdateCompositingInputs();

  geometry_map_.PopMappingsToAncestor(layer->Parent());

  if (layer->SelfPaintingStatusChanged()) {
    layer->ClearSelfPaintingStatusChanged();
    // If the floating object becomes non-self-painting, so some ancestor should
    // paint it; if it becomes self-painting, it should paint itself and no
    // ancestor should paint it.
    if (layer->GetLayoutObject().IsFloating()) {
      LayoutBlockFlow::UpdateAncestorShouldPaintFloatingObject(
          *layer->GetLayoutBox());
    }
  }
}

#if DCHECK_IS_ON()

void CompositingInputsUpdater::AssertNeedsCompositingInputsUpdateBitsCleared(
    PaintLayer* layer) {
  DCHECK(!layer->ChildNeedsCompositingInputsUpdate());
  DCHECK(!layer->NeedsCompositingInputsUpdate());

  for (PaintLayer* child = layer->FirstChild(); child;
       child = child->NextSibling())
    AssertNeedsCompositingInputsUpdateBitsCleared(child);
}

#endif

}  // namespace blink
