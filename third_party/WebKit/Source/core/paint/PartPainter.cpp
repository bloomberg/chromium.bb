// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PartPainter.h"

#include "core/frame/FrameOrPlugin.h"
#include "core/layout/LayoutPart.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/ReplacedPainter.h"
#include "core/paint/RoundedInnerRectClipper.h"
#include "core/paint/ScrollableAreaPainter.h"
#include "core/paint/TransformRecorder.h"
#include "platform/wtf/Optional.h"

namespace blink {

bool PartPainter::IsSelected() const {
  SelectionState s = layout_part_.GetSelectionState();
  if (s == SelectionState::kNone)
    return false;
  if (s == SelectionState::kInside)
    return true;

  int selection_start, selection_end;
  std::tie(selection_start, selection_end) = layout_part_.SelectionStartEnd();
  if (s == SelectionState::kStart)
    return selection_start == 0;

  int end = layout_part_.GetNode()->hasChildren()
                ? layout_part_.GetNode()->CountChildren()
                : 1;
  if (s == SelectionState::kEnd)
    return selection_end == end;
  if (s == SelectionState::kStartAndEnd)
    return selection_start == 0 && selection_end == end;

  DCHECK(0);
  return false;
}

void PartPainter::Paint(const PaintInfo& paint_info,
                        const LayoutPoint& paint_offset) {
  ObjectPainter(layout_part_).CheckPaintOffset(paint_info, paint_offset);
  LayoutPoint adjusted_paint_offset = paint_offset + layout_part_.Location();
  if (!ReplacedPainter(layout_part_)
           .ShouldPaint(paint_info, adjusted_paint_offset))
    return;

  LayoutRect border_rect(adjusted_paint_offset, layout_part_.Size());

  if (layout_part_.HasBoxDecorationBackground() &&
      (paint_info.phase == kPaintPhaseForeground ||
       paint_info.phase == kPaintPhaseSelection))
    BoxPainter(layout_part_)
        .PaintBoxDecorationBackground(paint_info, adjusted_paint_offset);

  if (paint_info.phase == kPaintPhaseMask) {
    BoxPainter(layout_part_).PaintMask(paint_info, adjusted_paint_offset);
    return;
  }

  if (ShouldPaintSelfOutline(paint_info.phase))
    ObjectPainter(layout_part_).PaintOutline(paint_info, adjusted_paint_offset);

  if (paint_info.phase != kPaintPhaseForeground)
    return;

  if (layout_part_.GetFrameOrPlugin()) {
    // TODO(schenney) crbug.com/93805 Speculative release assert to verify that
    // the crashes we see in FrameViewBase painting are due to a destroyed
    // LayoutPart object.
    CHECK(layout_part_.GetNode());
    Optional<RoundedInnerRectClipper> clipper;
    if (layout_part_.Style()->HasBorderRadius()) {
      if (border_rect.IsEmpty())
        return;

      FloatRoundedRect rounded_inner_rect =
          layout_part_.Style()->GetRoundedInnerBorderFor(
              border_rect,
              LayoutRectOutsets(
                  -(layout_part_.PaddingTop() + layout_part_.BorderTop()),
                  -(layout_part_.PaddingRight() + layout_part_.BorderRight()),
                  -(layout_part_.PaddingBottom() + layout_part_.BorderBottom()),
                  -(layout_part_.PaddingLeft() + layout_part_.BorderLeft())),
              true, true);
      clipper.emplace(layout_part_, paint_info, border_rect, rounded_inner_rect,
                      kApplyToDisplayList);
    }

    layout_part_.PaintContents(paint_info, paint_offset);
  }

  // Paint a partially transparent wash over selected FrameViewBases.
  if (IsSelected() && !paint_info.IsPrinting() &&
      !LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_part_, paint_info.phase)) {
    LayoutRect rect = layout_part_.LocalSelectionRect();
    rect.MoveBy(adjusted_paint_offset);
    IntRect selection_rect = PixelSnappedIntRect(rect);
    LayoutObjectDrawingRecorder drawing_recorder(
        paint_info.context, layout_part_, paint_info.phase, selection_rect);
    paint_info.context.FillRect(selection_rect,
                                layout_part_.SelectionBackgroundColor());
  }

  if (layout_part_.CanResize())
    ScrollableAreaPainter(*layout_part_.Layer()->GetScrollableArea())
        .PaintResizer(paint_info.context,
                      RoundedIntPoint(adjusted_paint_offset),
                      paint_info.GetCullRect());
}

void PartPainter::PaintContents(const PaintInfo& paint_info,
                                const LayoutPoint& paint_offset) {
  LayoutPoint adjusted_paint_offset = paint_offset + layout_part_.Location();

  FrameOrPlugin* frame_or_plugin = layout_part_.GetFrameOrPlugin();
  CHECK(frame_or_plugin);

  IntPoint paint_location(RoundedIntPoint(
      adjusted_paint_offset + layout_part_.ReplacedContentRect().Location()));

  // Views don't support painting with a paint offset, but instead
  // offset themselves using the frame rect location. To paint Views at
  // our desired location, we need to apply paint offset as a transform, with
  // the frame rect neutralized.
  IntSize view_paint_offset =
      paint_location - frame_or_plugin->FrameRect().Location();
  TransformRecorder transform(
      paint_info.context, layout_part_,
      AffineTransform::Translation(view_paint_offset.Width(),
                                   view_paint_offset.Height()));
  CullRect adjusted_cull_rect(paint_info.GetCullRect(), -view_paint_offset);
  frame_or_plugin->Paint(paint_info.context, adjusted_cull_rect);
}

}  // namespace blink
