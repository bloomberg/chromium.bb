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
#include "platform/graphics/paint/FilterDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

sk_sp<SkImageFilter> FilterPainter::GetImageFilter(PaintLayer& layer) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return nullptr;

  if (!layer.PaintsWithFilters())
    return nullptr;

  FilterEffect* last_effect = layer.LastFilterEffect();
  if (!last_effect)
    return nullptr;

  return SkiaImageFilterBuilder::Build(last_effect, kInterpolationSpaceSRGB);
}

FilterPainter::FilterPainter(PaintLayer& layer,
                             GraphicsContext& context,
                             const LayoutPoint& offset_from_root,
                             const ClipRect& clip_rect,
                             PaintLayerPaintingInfo& painting_info,
                             PaintLayerFlags paint_flags)
    : filter_in_progress_(false),
      context_(context),
      layout_object_(layer.GetLayoutObject()) {
  sk_sp<SkImageFilter> image_filter = GetImageFilter(layer);
  if (!image_filter)
    return;

  if (clip_rect.Rect() != painting_info.paint_dirty_rect ||
      clip_rect.HasRadius()) {
    // Apply clips outside the filter. See discussion about these clips
    // in PaintLayerPainter regarding "clipping in the presence of filters".
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
