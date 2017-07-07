// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FilterPainter.h"

#include <memory>
#include "core/paint/FilterEffectBuilder.h"
#include "core/paint/LayerClipRecorder.h"
#include "core/paint/PaintLayer.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/paint/FilterDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

FilterPainter::FilterPainter(PaintLayer& layer,
                             GraphicsContext& context,
                             const LayoutPoint& offset_from_root,
                             const ClipRect& clip_rect,
                             PaintLayerPaintingInfo& painting_info,
                             PaintLayerFlags paint_flags)
    : filter_in_progress_(false),
      context_(context),
      layout_object_(layer.GetLayoutObject()) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  if (!layer.PaintsWithFilters())
    return;

  FilterEffect* last_effect = layer.LastFilterEffect();
  if (!last_effect)
    return;

  sk_sp<SkImageFilter> image_filter =
      SkiaImageFilterBuilder::Build(last_effect, kInterpolationSpaceSRGB);
  if (!image_filter)
    return;

  // We'll handle clipping to the dirty rect before filter rasterization.
  // Filter processing will automatically expand the clip rect and the offscreen
  // to accommodate any filter outsets.
  // FIXME: It is incorrect to just clip to the damageRect here once multiple
  // fragments are involved.

  // Subsequent code should not clip to the dirty rect, since we've already
  // done it above, and doing it later will defeat the outsets.
  painting_info.clip_to_dirty_rect = false;

  if (clip_rect.Rect() != painting_info.paint_dirty_rect ||
      clip_rect.HasRadius()) {
    clip_recorder_ = WTF::WrapUnique(new LayerClipRecorder(
        context, layer, DisplayItem::kClipLayerFilter, clip_rect,
        painting_info.root_layer, LayoutPoint(), paint_flags,
        layer.GetLayoutObject()));
  }

  if (!context.GetPaintController().DisplayItemConstructionIsDisabled()) {
    CompositorFilterOperations compositor_filter_operations =
        layer.CreateCompositorFilterOperationsForFilter(
            layout_object_.StyleRef());
    // FIXME: It's possible to have empty CompositorFilterOperations here even
    // though the SkImageFilter produced above is non-null, since the
    // layer's FilterEffectBuilder can have a stale representation of
    // the layer's filter. See crbug.com/502026.
    if (compositor_filter_operations.IsEmpty())
      return;
    LayoutRect visual_bounds(
        layer.PhysicalBoundingBoxIncludingStackingChildren(offset_from_root));
    if (layer.EnclosingPaginationLayer()) {
      // Filters are set up before pagination, so we need to make the bounding
      // box visual on our own.
      visual_bounds.MoveBy(-offset_from_root);
      layer.ConvertFromFlowThreadToVisualBoundingBoxInAncestor(
          painting_info.root_layer, visual_bounds);
    }
    FloatPoint origin(offset_from_root);
    context.GetPaintController().CreateAndAppend<BeginFilterDisplayItem>(
        layout_object_, std::move(image_filter), FloatRect(visual_bounds),
        origin, std::move(compositor_filter_operations));
  }

  filter_in_progress_ = true;
}

FilterPainter::~FilterPainter() {
  if (!filter_in_progress_)
    return;

  context_.GetPaintController().EndItem<EndFilterDisplayItem>(layout_object_);
}

}  // namespace blink
