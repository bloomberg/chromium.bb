/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/paint/compositing/GraphicsLayerUpdater.h"

#include "core/html/HTMLMediaElement.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/layout/LayoutBlock.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/compositing/CompositedLayerMapping.h"
#include "core/paint/compositing/PaintLayerCompositor.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

class GraphicsLayerUpdater::UpdateContext {
 public:
  UpdateContext()
      : compositing_stacking_context_(nullptr),
        compositing_ancestor_(nullptr) {}

  UpdateContext(const UpdateContext& other, const PaintLayer& layer)
      : compositing_stacking_context_(other.compositing_stacking_context_),
        compositing_ancestor_(other.CompositingContainer(layer)) {
    CompositingState compositing_state = layer.GetCompositingState();
    if (compositing_state != kNotComposited &&
        compositing_state != kPaintsIntoGroupedBacking) {
      compositing_ancestor_ = &layer;
      if (layer.StackingNode()->IsStackingContext())
        compositing_stacking_context_ = &layer;
    }
  }

  const PaintLayer* CompositingContainer(const PaintLayer& layer) const {
    const PaintLayer* compositing_container;
    if (layer.StackingNode()->IsStacked()) {
      compositing_container = compositing_stacking_context_;
    } else if ((layer.Parent() &&
                !layer.Parent()->GetLayoutObject().IsLayoutBlock()) ||
               layer.GetLayoutObject().IsColumnSpanAll()) {
      // In these cases, compositingContainer may escape the normal layer
      // hierarchy. Use the slow path to ensure correct result.
      // See PaintLayer::containingLayer() for details.
      compositing_container =
          layer.EnclosingLayerWithCompositedLayerMapping(kExcludeSelf);
    } else {
      compositing_container = compositing_ancestor_;
    }

    // We should always get the same result as the slow path.
    DCHECK_EQ(compositing_container,
              layer.EnclosingLayerWithCompositedLayerMapping(kExcludeSelf));
    return compositing_container;
  }

  const PaintLayer* CompositingStackingContext() const {
    return compositing_stacking_context_;
  }

 private:
  const PaintLayer* compositing_stacking_context_;
  const PaintLayer* compositing_ancestor_;
};

GraphicsLayerUpdater::GraphicsLayerUpdater() : needs_rebuild_tree_(false) {}

GraphicsLayerUpdater::~GraphicsLayerUpdater() {}

void GraphicsLayerUpdater::Update(
    PaintLayer& layer,
    Vector<PaintLayer*>& layers_needing_paint_invalidation) {
  TRACE_EVENT0("blink", "GraphicsLayerUpdater::update");
  UpdateRecursive(layer, kDoNotForceUpdate, UpdateContext(),
                  layers_needing_paint_invalidation);
  layer.Compositor()->UpdateRootLayerPosition();
}

void GraphicsLayerUpdater::UpdateRecursive(
    PaintLayer& layer,
    UpdateType update_type,
    const UpdateContext& context,
    Vector<PaintLayer*>& layers_needing_paint_invalidation) {
  if (layer.HasCompositedLayerMapping()) {
    CompositedLayerMapping* mapping = layer.GetCompositedLayerMapping();

    if (update_type == kForceUpdate || mapping->NeedsGraphicsLayerUpdate()) {
      if (mapping->UpdateGraphicsLayerConfiguration())
        needs_rebuild_tree_ = true;
      mapping->UpdateGraphicsLayerGeometry(context.CompositingContainer(layer),
                                           context.CompositingStackingContext(),
                                           layers_needing_paint_invalidation);
      if (PaintLayerScrollableArea* scrollable_area = layer.GetScrollableArea())
        scrollable_area->PositionOverflowControls();
      update_type = mapping->UpdateTypeForChildren(update_type);
      mapping->ClearNeedsGraphicsLayerUpdate();
    }
  }

  UpdateContext child_context(context, layer);
  for (PaintLayer* child = layer.FirstChild(); child;
       child = child->NextSibling()) {
    UpdateRecursive(*child, update_type, child_context,
                    layers_needing_paint_invalidation);
  }
}

#if DCHECK_IS_ON()

void GraphicsLayerUpdater::AssertNeedsToUpdateGraphicsLayerBitsCleared(
    PaintLayer& layer) {
  if (layer.HasCompositedLayerMapping()) {
    layer.GetCompositedLayerMapping()
        ->AssertNeedsToUpdateGraphicsLayerBitsCleared();
  }

  for (PaintLayer* child = layer.FirstChild(); child;
       child = child->NextSibling())
    AssertNeedsToUpdateGraphicsLayerBitsCleared(*child);
}

#endif

}  // namespace blink
