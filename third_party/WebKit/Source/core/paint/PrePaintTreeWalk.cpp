// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PrePaintTreeWalk.h"

#include "core/dom/DocumentLifecycle.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/LayoutMultiColumnSpannerPlaceholder.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/compositing/CompositingLayerPropertyUpdater.h"
#include "platform/graphics/paint/GeometryMapper.h"

namespace blink {

struct PrePaintTreeWalkContext {
  PrePaintTreeWalkContext()
      : tree_builder_context(
            WTF::WrapUnique(new PaintPropertyTreeBuilderContext)),
        paint_invalidator_context(WTF::WrapUnique(new PaintInvalidatorContext)),
        ancestor_overflow_paint_layer(nullptr) {}

  PrePaintTreeWalkContext(const PrePaintTreeWalkContext& parent_context,
                          bool needs_tree_builder_context)
      : tree_builder_context(
            WTF::WrapUnique(needs_tree_builder_context || DCHECK_IS_ON()
                                ? new PaintPropertyTreeBuilderContext(
                                      *parent_context.tree_builder_context)
                                : nullptr)),
        paint_invalidator_context(WTF::WrapUnique(new PaintInvalidatorContext(
            *parent_context.paint_invalidator_context))),
        ancestor_overflow_paint_layer(
            parent_context.ancestor_overflow_paint_layer) {
#if DCHECK_IS_ON()
    if (needs_tree_builder_context)
      DCHECK(parent_context.tree_builder_context->is_actually_needed);
    tree_builder_context->is_actually_needed = needs_tree_builder_context;
#endif
  }

  // PaintPropertyTreeBuilderContext is large and can lead to stack overflows
  // when recursion is deep so this context object is allocated on the heap.
  // See: https://crbug.com/698653.
  std::unique_ptr<PaintPropertyTreeBuilderContext> tree_builder_context;

  std::unique_ptr<PaintInvalidatorContext> paint_invalidator_context;

  // The ancestor in the PaintLayer tree which has overflow clip, or
  // is the root layer. Note that it is tree ancestor, not containing
  // block or stacking ancestor.
  PaintLayer* ancestor_overflow_paint_layer;
};

void PrePaintTreeWalk::Walk(LocalFrameView& root_frame) {
  DCHECK(root_frame.GetFrame().GetDocument()->Lifecycle().GetState() ==
         DocumentLifecycle::kInPrePaint);

  PrePaintTreeWalkContext initial_context;

  // GeometryMapper depends on paint properties.
  if (NeedsTreeBuilderContextUpdate(root_frame, initial_context))
    GeometryMapper::ClearCache();

  Walk(root_frame, initial_context);
  paint_invalidator_.ProcessPendingDelayedPaintInvalidations();
}

void PrePaintTreeWalk::Walk(LocalFrameView& frame_view,
                            const PrePaintTreeWalkContext& parent_context) {
  if (frame_view.ShouldThrottleRendering()) {
    // Skip the throttled frame. Will update it when it becomes unthrottled.
    return;
  }

  bool needs_tree_builder_context_update =
      this->NeedsTreeBuilderContextUpdate(frame_view, parent_context);
  PrePaintTreeWalkContext context(parent_context,
                                  needs_tree_builder_context_update);
  // ancestorOverflowLayer does not cross frame boundaries.
  context.ancestor_overflow_paint_layer = nullptr;
  if (context.tree_builder_context) {
    property_tree_builder_.UpdateProperties(frame_view,
                                            *context.tree_builder_context);
  }
  paint_invalidator_.InvalidatePaint(frame_view,
                                     context.tree_builder_context.get(),
                                     *context.paint_invalidator_context);

  if (LayoutView* view = frame_view.GetLayoutView()) {
    Walk(*view, context);
#if DCHECK_IS_ON()
    view->AssertSubtreeClearedPaintInvalidationFlags();
#endif
  }
  frame_view.ClearNeedsPaintPropertyUpdate();
}

static void UpdateAuxiliaryObjectProperties(const LayoutObject& object,
                                            PrePaintTreeWalkContext& context) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  if (!object.HasLayer())
    return;

  PaintLayer* paint_layer = object.EnclosingLayer();
  paint_layer->UpdateAncestorOverflowLayer(
      context.ancestor_overflow_paint_layer);

  if (object.StyleRef().HasStickyConstrainedPosition()) {
    paint_layer->GetLayoutObject().UpdateStickyPositionConstraints();

    // Sticky position constraints and ancestor overflow scroller affect the
    // sticky layer position, so we need to update it again here.
    // TODO(flackr): This should be refactored in the future to be clearer (i.e.
    // update layer position and ancestor inputs updates in the same walk).
    paint_layer->UpdateLayerPosition();
  }
  if (paint_layer->IsRootLayer() || object.HasOverflowClip())
    context.ancestor_overflow_paint_layer = paint_layer;
}

void PrePaintTreeWalk::InvalidatePaintLayerOptimizationsIfNeeded(
    const LayoutObject& object,
    PrePaintTreeWalkContext& context) {
  if (!object.HasLayer())
    return;

  PaintLayer& paint_layer = *ToLayoutBoxModelObject(object).Layer();

  // Ignore clips across paint invalidation container or transform
  // boundaries.
  if (object ==
          context.paint_invalidator_context->paint_invalidation_container ||
      object.StyleRef().HasTransform())
    context.tree_builder_context->clip_changed = false;

  if (!paint_layer.SupportsSubsequenceCaching() ||
      !context.tree_builder_context->clip_changed)
    return;

  paint_layer.SetNeedsRepaint();
  paint_layer.SetPreviousPaintPhaseDescendantOutlinesEmpty(false);
  paint_layer.SetPreviousPaintPhaseFloatEmpty(false);
  paint_layer.SetPreviousPaintPhaseDescendantBlockBackgroundsEmpty(false);
  context.paint_invalidator_context->subtree_flags |=
      PaintInvalidatorContext::kSubtreeVisualRectUpdate;
}

bool PrePaintTreeWalk::NeedsTreeBuilderContextUpdate(
    const LocalFrameView& frame_view,
    const PrePaintTreeWalkContext& context) {
  return frame_view.NeedsPaintPropertyUpdate() ||
         (frame_view.GetLayoutView() &&
          NeedsTreeBuilderContextUpdate(*frame_view.GetLayoutView(), context));
}

bool PrePaintTreeWalk::NeedsTreeBuilderContextUpdate(
    const LayoutObject& object,
    const PrePaintTreeWalkContext& parent_context) {
  return object.NeedsPaintPropertyUpdate() ||
         object.DescendantNeedsPaintPropertyUpdate() ||
         (parent_context.tree_builder_context &&
          parent_context.tree_builder_context->force_subtree_update) ||
         // If the object needs visual rect update, we should update tree
         // builder context which is needed by visual rect update.
         parent_context.paint_invalidator_context->NeedsVisualRectUpdate(
             object);
}

void PrePaintTreeWalk::Walk(const LayoutObject& object,
                            const PrePaintTreeWalkContext& parent_context) {
  // Early out from the tree walk if possible.
  bool needs_tree_builder_context_update =
      this->NeedsTreeBuilderContextUpdate(object, parent_context);
  if (!needs_tree_builder_context_update &&
      !object.ShouldCheckForPaintInvalidation())
    return;

  PrePaintTreeWalkContext context(parent_context,
                                  needs_tree_builder_context_update);

  // This must happen before updatePropertiesForSelf, because the latter reads
  // some of the state computed here.
  UpdateAuxiliaryObjectProperties(object, context);

  if (context.tree_builder_context) {
    property_tree_builder_.UpdatePropertiesForSelf(
        object, *context.tree_builder_context);

    if (context.tree_builder_context->clip_changed) {
      context.paint_invalidator_context->subtree_flags |=
          PaintInvalidatorContext::kSubtreeVisualRectUpdate;
    }
  }

  paint_invalidator_.InvalidatePaint(object, context.tree_builder_context.get(),
                                     *context.paint_invalidator_context);

  if (context.tree_builder_context) {
    property_tree_builder_.UpdatePropertiesForChildren(
        object, *context.tree_builder_context);

    InvalidatePaintLayerOptimizationsIfNeeded(object, context);
  }

  CompositingLayerPropertyUpdater::Update(object);

  for (const LayoutObject* child = object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsLayoutMultiColumnSpannerPlaceholder()) {
      child->GetMutableForPainting().ClearPaintFlags();
      continue;
    }
    Walk(*child, context);
  }

  if (object.IsLayoutEmbeddedContent()) {
    const LayoutEmbeddedContent& layout_embedded_content =
        ToLayoutEmbeddedContent(object);
    LocalFrameView* frame_view = layout_embedded_content.ChildFrameView();
    if (frame_view) {
      if (context.tree_builder_context) {
        context.tree_builder_context->fragments[0].current.paint_offset +=
            layout_embedded_content.ReplacedContentRect().Location() -
            frame_view->FrameRect().Location();
        context.tree_builder_context->fragments[0].current.paint_offset =
            RoundedIntPoint(context.tree_builder_context->fragments[0]
                                .current.paint_offset);
      }
      Walk(*frame_view, context);
    }
    // TODO(pdr): Investigate RemoteFrameView (crbug.com/579281).
  }

  object.GetMutableForPainting().ClearPaintFlags();
}

}  // namespace blink
