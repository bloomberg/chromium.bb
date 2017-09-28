// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ListMarkerPainter.h"

#include "core/layout/LayoutListItem.h"
#include "core/layout/LayoutListMarker.h"
#include "core/layout/ListMarkerText.h"
#include "core/layout/api/SelectionState.h"
#include "core/paint/BoxModelObjectPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/SelectionPaintingUtils.h"
#include "core/paint/TextPainter.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

static inline void PaintSymbol(GraphicsContext& context,
                               const Color& color,
                               const IntRect& marker,
                               EListStyleType list_style) {
  context.SetStrokeColor(color);
  context.SetStrokeStyle(kSolidStroke);
  context.SetStrokeThickness(1.0f);
  switch (list_style) {
    case EListStyleType::kDisc:
      context.FillEllipse(marker);
      break;
    case EListStyleType::kCircle:
      context.StrokeEllipse(marker);
      break;
    case EListStyleType::kSquare:
      context.FillRect(marker);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ListMarkerPainter::Paint(const PaintInfo& paint_info,
                              const LayoutPoint& paint_offset) {
  if (paint_info.phase != kPaintPhaseForeground)
    return;

  if (layout_list_marker_.Style()->Visibility() != EVisibility::kVisible)
    return;

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_list_marker_, paint_info.phase))
    return;

  LayoutPoint box_origin(paint_offset + layout_list_marker_.Location());
  LayoutRect overflow_rect(layout_list_marker_.VisualOverflowRect());
  overflow_rect.MoveBy(box_origin);

  IntRect pixel_snapped_overflow_rect = PixelSnappedIntRect(overflow_rect);
  if (!paint_info.GetCullRect().IntersectsCullRect(overflow_rect))
    return;

  LayoutObjectDrawingRecorder recorder(paint_info.context, layout_list_marker_,
                                       paint_info.phase,
                                       pixel_snapped_overflow_rect);

  LayoutRect box(box_origin, layout_list_marker_.Size());

  IntRect marker = layout_list_marker_.GetRelativeMarkerRect();
  marker.MoveBy(RoundedIntPoint(box_origin));

  GraphicsContext& context = paint_info.context;

  if (layout_list_marker_.IsImage()) {
    context.DrawImage(
        layout_list_marker_.GetImage()
            ->GetImage(layout_list_marker_, layout_list_marker_.GetDocument(),
                       layout_list_marker_.StyleRef(), marker.Size(), nullptr)
            .get(),
        marker);
    if (layout_list_marker_.GetSelectionState() != SelectionState::kNone) {
      LayoutRect sel_rect = layout_list_marker_.LocalSelectionRect();
      sel_rect.MoveBy(box_origin);
      Color selection_bg = SelectionPaintingUtils::SelectionBackgroundColor(
          layout_list_marker_.ListItem()->GetDocument(),
          layout_list_marker_.ListItem()->StyleRef(),
          layout_list_marker_.ListItem()->GetNode());
      context.FillRect(PixelSnappedIntRect(sel_rect), selection_bg);
    }
    return;
  }

  LayoutListMarker::ListStyleCategory style_category =
      layout_list_marker_.GetListStyleCategory();
  if (style_category == LayoutListMarker::ListStyleCategory::kNone)
    return;

  Color color(layout_list_marker_.ResolveColor(CSSPropertyColor));

  if (BoxModelObjectPainter::ShouldForceWhiteBackgroundForPrintEconomy(
          layout_list_marker_.ListItem()->GetDocument(),
          layout_list_marker_.StyleRef()))
    color = TextPainter::TextColorForWhiteBackground(color);

  // Apply the color to the list marker text.
  context.SetFillColor(color);

  const EListStyleType list_style =
      layout_list_marker_.Style()->ListStyleType();
  if (style_category == LayoutListMarker::ListStyleCategory::kSymbol) {
    PaintSymbol(context, color, marker, list_style);
    return;
  }

  if (layout_list_marker_.GetText().IsEmpty())
    return;

  const Font& font = layout_list_marker_.Style()->GetFont();
  TextRun text_run = ConstructTextRun(font, layout_list_marker_.GetText(),
                                      layout_list_marker_.StyleRef());

  GraphicsContextStateSaver state_saver(context, false);
  if (!layout_list_marker_.Style()->IsHorizontalWritingMode()) {
    marker.MoveBy(RoundedIntPoint(-box_origin));
    marker = marker.TransposedRect();
    marker.MoveBy(
        IntPoint(RoundToInt(box.X()),
                 RoundToInt(box.Y() - layout_list_marker_.LogicalHeight())));
    state_saver.Save();
    context.Translate(marker.X(), marker.MaxY());
    context.Rotate(static_cast<float>(deg2rad(90.)));
    context.Translate(-marker.X(), -marker.MaxY());
  }

  TextRunPaintInfo text_run_paint_info(text_run);
  text_run_paint_info.bounds = marker;
  const SimpleFontData* font_data =
      layout_list_marker_.Style()->GetFont().PrimaryFont();
  IntPoint text_origin = IntPoint(
      marker.X(),
      marker.Y() + (font_data ? font_data->GetFontMetrics().Ascent() : 0));

  // Text is not arbitrary. We can judge whether it's RTL from the first
  // character, and we only need to handle the direction RightToLeft for now.
  bool text_needs_reversing =
      WTF::Unicode::Direction(layout_list_marker_.GetText()[0]) ==
      WTF::Unicode::kRightToLeft;
  StringBuilder reversed_text;
  if (text_needs_reversing) {
    unsigned length = layout_list_marker_.GetText().length();
    reversed_text.ReserveCapacity(length);
    for (int i = length - 1; i >= 0; --i)
      reversed_text.Append(layout_list_marker_.GetText()[i]);
    DCHECK(reversed_text.length() == length);
    text_run.SetText(reversed_text.ToString());
  }

  const UChar suffix = ListMarkerText::Suffix(
      list_style, layout_list_marker_.ListItem()->Value());
  UChar suffix_str[2] = {suffix, static_cast<UChar>(' ')};
  TextRun suffix_run =
      ConstructTextRun(font, suffix_str, 2, layout_list_marker_.StyleRef(),
                       layout_list_marker_.Style()->Direction());
  TextRunPaintInfo suffix_run_info(suffix_run);
  suffix_run_info.bounds = marker;

  if (layout_list_marker_.Style()->IsLeftToRightDirection()) {
    context.DrawText(font, text_run_paint_info, text_origin);
    context.DrawText(font, suffix_run_info,
                     text_origin + IntSize(font.Width(text_run), 0));
  } else {
    context.DrawText(font, suffix_run_info, text_origin);
    context.DrawText(font, text_run_paint_info,
                     text_origin + IntSize(font.Width(suffix_run), 0));
  }
}

}  // namespace blink
