// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/EmbeddedContentPainter.h"

#include "core/frame/EmbeddedContentView.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/paint/AdjustPaintOffsetScope.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/ReplacedPainter.h"
#include "core/paint/RoundedInnerRectClipper.h"
#include "core/paint/ScrollableAreaPainter.h"
#include "core/paint/SelectionPaintingUtils.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/paint/DisplayItemCacheSkipper.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/wtf/Optional.h"

namespace blink {

bool EmbeddedContentPainter::IsSelected() const {
  SelectionState s = layout_embedded_content_.GetSelectionState();
  if (s == SelectionState::kNone)
    return false;

  return true;
}

void EmbeddedContentPainter::Paint(const PaintInfo& paint_info,
                                   const LayoutPoint& paint_offset) {
  // TODO(crbug.com/797779): For now embedded contents don't know whether
  // they are painted in a fragmented context and may do something bad in a
  // fragmented context, e.g. creating subsequences. Skip cache to avoid that.
  // This will be unnecessary when the contents are fragment aware.
  Optional<DisplayItemCacheSkipper> cache_skipper;
  DCHECK(layout_embedded_content_.HasLayer());
  if (layout_embedded_content_.Layer()->EnclosingPaginationLayer())
    cache_skipper.emplace(paint_info.context);

  AdjustPaintOffsetScope adjustment(layout_embedded_content_, paint_info,
                                    paint_offset);
  const auto& local_paint_info = adjustment.GetPaintInfo();
  auto adjusted_paint_offset = adjustment.AdjustedPaintOffset();
  if (!ReplacedPainter(layout_embedded_content_)
           .ShouldPaint(local_paint_info, adjusted_paint_offset))
    return;

  LayoutRect border_rect(adjusted_paint_offset,
                         layout_embedded_content_.Size());

  if (layout_embedded_content_.HasBoxDecorationBackground() &&
      (local_paint_info.phase == PaintPhase::kForeground ||
       local_paint_info.phase == PaintPhase::kSelection)) {
    BoxPainter(layout_embedded_content_)
        .PaintBoxDecorationBackground(local_paint_info, adjusted_paint_offset);
  }

  if (local_paint_info.phase == PaintPhase::kMask) {
    BoxPainter(layout_embedded_content_)
        .PaintMask(local_paint_info, adjusted_paint_offset);
    return;
  }

  if (ShouldPaintSelfOutline(local_paint_info.phase)) {
    ObjectPainter(layout_embedded_content_)
        .PaintOutline(local_paint_info, adjusted_paint_offset);
  }

  if (local_paint_info.phase != PaintPhase::kForeground)
    return;

  if (layout_embedded_content_.GetEmbeddedContentView()) {
    Optional<ScopedPaintChunkProperties> scoped_paint_chunk_properties;
    Optional<RoundedInnerRectClipper> clipper;
    if (layout_embedded_content_.Style()->HasBorderRadius()) {
      if (border_rect.IsEmpty())
        return;

      if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled()) {
        const auto* fragment =
            local_paint_info.FragmentToPaint(layout_embedded_content_);
        if (!fragment)
          return;
        const auto* properties = fragment->PaintProperties();
        DCHECK(properties && properties->InnerBorderRadiusClip());
        scoped_paint_chunk_properties.emplace(
            local_paint_info.context.GetPaintController(),
            properties->InnerBorderRadiusClip(), layout_embedded_content_,
            DisplayItem::PaintPhaseToDrawingType(local_paint_info.phase));
      } else {
        FloatRoundedRect rounded_inner_rect =
            layout_embedded_content_.Style()->GetRoundedInnerBorderFor(
                border_rect,
                LayoutRectOutsets(-(layout_embedded_content_.PaddingTop() +
                                    layout_embedded_content_.BorderTop()),
                                  -(layout_embedded_content_.PaddingRight() +
                                    layout_embedded_content_.BorderRight()),
                                  -(layout_embedded_content_.PaddingBottom() +
                                    layout_embedded_content_.BorderBottom()),
                                  -(layout_embedded_content_.PaddingLeft() +
                                    layout_embedded_content_.BorderLeft())),
                true, true);
        clipper.emplace(layout_embedded_content_, local_paint_info, border_rect,
                        rounded_inner_rect, kApplyToDisplayList);
      }
    }

    layout_embedded_content_.PaintContents(local_paint_info, paint_offset);
  }

  // Paint a partially transparent wash over selected EmbeddedContentViews.
  if (IsSelected() && !local_paint_info.IsPrinting() &&
      !DrawingRecorder::UseCachedDrawingIfPossible(local_paint_info.context,
                                                   layout_embedded_content_,
                                                   local_paint_info.phase)) {
    LayoutRect rect = layout_embedded_content_.LocalSelectionRect();
    rect.MoveBy(adjusted_paint_offset);
    IntRect selection_rect = PixelSnappedIntRect(rect);
    DrawingRecorder recorder(local_paint_info.context, layout_embedded_content_,
                             local_paint_info.phase);
    Color selection_bg = SelectionPaintingUtils::SelectionBackgroundColor(
        layout_embedded_content_.GetDocument(),
        layout_embedded_content_.StyleRef(),
        layout_embedded_content_.GetNode());
    local_paint_info.context.FillRect(selection_rect, selection_bg);
  }

  if (layout_embedded_content_.CanResize()) {
    ScrollableAreaPainter(
        *layout_embedded_content_.Layer()->GetScrollableArea())
        .PaintResizer(local_paint_info.context,
                      RoundedIntPoint(adjusted_paint_offset),
                      local_paint_info.GetCullRect());
  }
}

void EmbeddedContentPainter::PaintContents(const PaintInfo& paint_info,
                                           const LayoutPoint& paint_offset) {
  AdjustPaintOffsetScope adjustment(layout_embedded_content_, paint_info,
                                    paint_offset);
  const auto& local_paint_info = adjustment.GetPaintInfo();
  auto adjusted_paint_offset = adjustment.AdjustedPaintOffset();

  EmbeddedContentView* embedded_content_view =
      layout_embedded_content_.GetEmbeddedContentView();
  CHECK(embedded_content_view);

  IntPoint paint_location(RoundedIntPoint(
      adjusted_paint_offset +
      layout_embedded_content_.ReplacedContentRect().Location()));

  // Views don't support painting with a paint offset, but instead
  // offset themselves using the frame rect location. To paint Views at
  // our desired location, we need to apply paint offset as a transform, with
  // the frame rect neutralized.
  IntSize view_paint_offset =
      paint_location - embedded_content_view->FrameRect().Location();
  TransformRecorder transform(
      local_paint_info.context, layout_embedded_content_,
      AffineTransform::Translation(view_paint_offset.Width(),
                                   view_paint_offset.Height()));
  CullRect adjusted_cull_rect(local_paint_info.GetCullRect(),
                              -view_paint_offset);
  embedded_content_view->Paint(local_paint_info.context,
                               local_paint_info.GetGlobalPaintFlags(),
                               adjusted_cull_rect);
}

}  // namespace blink
