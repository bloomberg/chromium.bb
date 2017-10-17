// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/EllipsisBoxPainter.h"

#include "core/layout/TextRunConstructor.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/api/SelectionState.h"
#include "core/layout/line/EllipsisBox.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TextPainter.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

void EllipsisBoxPainter::Paint(const PaintInfo& paint_info,
                               const LayoutPoint& paint_offset,
                               LayoutUnit line_top,
                               LayoutUnit line_bottom) {
  if (paint_info.phase == PaintPhase::kSelection)
    return;

  const ComputedStyle& style = ellipsis_box_.GetLineLayoutItem().StyleRef(
      ellipsis_box_.IsFirstLineStyle());
  PaintEllipsis(paint_info, paint_offset, line_top, line_bottom, style);
}

void EllipsisBoxPainter::PaintEllipsis(const PaintInfo& paint_info,
                                       const LayoutPoint& paint_offset,
                                       LayoutUnit line_top,
                                       LayoutUnit line_bottom,
                                       const ComputedStyle& style) {
  LayoutPoint box_origin = ellipsis_box_.PhysicalLocation();
  box_origin.MoveBy(paint_offset);
  LayoutRect paint_rect(box_origin, ellipsis_box_.Size());

  GraphicsContext& context = paint_info.context;
  DisplayItem::Type display_item_type =
      DisplayItem::PaintPhaseToDrawingType(paint_info.phase);
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, ellipsis_box_,
                                                  display_item_type))
    return;

  DrawingRecorder recorder(context, ellipsis_box_, display_item_type,
                           FloatRect(paint_rect));

  LayoutRect box_rect(box_origin,
                      LayoutSize(ellipsis_box_.LogicalWidth(),
                                 ellipsis_box_.VirtualLogicalHeight()));

  GraphicsContextStateSaver state_saver(context);
  if (!ellipsis_box_.IsHorizontal())
    context.ConcatCTM(TextPainter::Rotation(box_rect, TextPainter::kClockwise));

  const Font& font = style.GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;

  TextPaintStyle text_style = TextPainter::TextPaintingStyle(
      ellipsis_box_.GetLineLayoutItem().GetDocument(), style, paint_info);
  TextRun text_run = ConstructTextRun(font, ellipsis_box_.EllipsisStr(), style,
                                      TextRun::kAllowTrailingExpansion);
  LayoutPoint text_origin(
      box_origin.X(), box_origin.Y() + font_data->GetFontMetrics().Ascent());
  TextPainter text_painter(context, font, text_run, text_origin, box_rect,
                           ellipsis_box_.IsHorizontal());
  text_painter.Paint(0, ellipsis_box_.EllipsisStr().length(),
                     ellipsis_box_.EllipsisStr().length(), text_style);
}

}  // namespace blink
